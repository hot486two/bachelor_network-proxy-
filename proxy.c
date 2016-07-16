#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>


#define MAX_REQUEST 2048
#define MAX_URL 2048
struct args 
{
    int id;
    int connfd;
    char buffer[MAX_REQUEST];
    char ip[20];
};
struct req
{
    char request[MAX_REQUEST];
    char method[16];
    char path[MAX_URL];
    char version[16];
    char host[MAX_URL];
    char page[MAX_URL];
};

void *handleConnection(void *args);
FILE *fp_log; //log

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int main(int argc, char** argv) {
    int port = atoi(argv[1]);
    int threadID = 0;
	int sockfd;
	struct sockaddr_in serv_addr;
	
	sockfd=socket(PF_INET, SOCK_STREAM,0);

	if(sockfd<0){
		perror("Error open socket\n");
		exit(0);
	}
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(atoi(argv[1]));

	if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1){
		perror("Error bind socket\n");
		exit(0);
	}


    while (1)
    {
	    struct args* myargs;
	    int /*clnt_addr_size,*/clnt_sock;
	    socklen_t clnt_addr_size;
	    struct sockaddr_in clnt_addr;
	    char *str_temp;
        myargs = (struct args*) malloc(sizeof(struct args));
	if(listen(sockfd,5)==-1){
		perror("listen() error");
		exit(0);
	}
	clnt_addr_size=sizeof(clnt_addr);
	clnt_sock=accept(sockfd,(struct sockaddr*)&clnt_addr,&clnt_addr_size);
	if(clnt_sock==-1){
		perror("accept() error");
		exit(0);
	}
	myargs->connfd=clnt_sock;
	
	getpeername(clnt_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);
	str_temp = inet_ntoa(clnt_addr.sin_addr);
	strcpy(myargs->ip,str_temp);
	printf("%s\n",myargs->ip);

        pthread_t thread;
        threadID++;
        myargs->id = threadID;
        pthread_create(&thread, NULL, handleConnection, myargs); //thread never joins
	pthread_detach(thread);
	printf("%d loop end\n",myargs->id);
    }
    close(sockfd);     
    return 0;
}
void *handleConnection(void *args)
{

    struct args* myarg = (struct args*) args;
    struct req* myreq;
int bytesRead=0;	
    char *temp;
    char host[MAX_URL], page[MAX_URL];

    //to send the message to server and receive the content by server
    //and send the content to client
    int chunkRead;
    size_t fd;
    char data[100000];
    char reqBuffer[512];
    int totalBytesWritten = 0;
    int chunkWritten=0;

    time_t result;
    result = time(NULL);
    struct tm* brokentime = localtime(&result);
	    

    //for connect to server
    int serv_sock;
    struct sockaddr_in serv_addr;
    struct hostent *server;



    myreq = (struct req*) malloc(sizeof(struct req));
    printf("thread %d created\n",myarg->id);
    memset(myarg->buffer,0,MAX_REQUEST);
    bytesRead = read(myarg->connfd, myarg->buffer, sizeof(myarg->buffer));
    printf("byteRead=%d\n",bytesRead);
	
    if(strcmp(myarg->buffer,"")==0){
	    printf("it is no data\n");
	    pthread_exit(NULL);
	    return;
    }
	strncpy(myreq->request,myarg->buffer,MAX_REQUEST-1);
	strncpy(myreq->method, strtok_r(myarg->buffer," ",&temp),15);
	strncpy(myreq->path,strtok_r(NULL," ",&temp),MAX_URL-1);
	strncpy(myreq->version, strtok_r(NULL, "\r\n",&temp),15);
	sscanf(myreq->path,"http://%99[^/]%99[^\n]",host,page);
	strncpy(myreq->page,page,MAX_URL-1);
	strncpy(myreq->host,host,MAX_URL-1);
	printf("request : %s\n method : %s\n path : %s\n version : %s\n page : %s\n host : ",myreq->request,myreq->method,myreq->path,myreq->version,myreq->page,myreq->host);

	memset(reqBuffer,0,512);
	serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	server = gethostbyname(host);
	if (server == NULL) {
		printf("No such host\n");
		exit(0);
	}
	memset((char *)&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memmove((char *)&serv_addr.sin_addr.s_addr,(char *)server->h_addr,server->h_length);
	serv_addr.sin_port = htons(80);
	if (connect(serv_sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
		printf("Error Connecting\n");
		exit(0);
	}

	strcat(reqBuffer, myreq->method);
	strcat(reqBuffer," ");
	strcat(reqBuffer, myreq->page);
	strcat(reqBuffer," ");
	strcat(reqBuffer, myreq->version);
	strcat(reqBuffer,"\r\n");
	strcat(reqBuffer, "host: ");
	strcat(reqBuffer, host);
	strcat(reqBuffer, "\r\n");
	strcat(reqBuffer, "\r\n");
	printf("request sent to %s :\n%s\nSent at: %s\n", host, reqBuffer, asctime(brokentime));

	chunkRead = write(serv_sock, reqBuffer, strlen(reqBuffer));
	while ((chunkRead = read(serv_sock, data, sizeof(data)))!= (size_t)NULL)
	{
		chunkWritten = write(myarg->connfd, data, chunkRead);
		totalBytesWritten += chunkWritten;
	}



    pthread_mutex_lock(&mutex);
    fp_log = fopen("proxy.log", "a");
    fprintf(fp_log,"%s %s %s %d\n",asctime(brokentime),myarg->ip,myreq->path,totalBytesWritten);
    fclose(fp_log);
    pthread_mutex_unlock(&mutex);
    printf("completed sending %s at %d bytes at %s\n------------------------------------------------------------------------------------\n", myreq->page, totalBytesWritten, asctime(brokentime));
    printf("%s %s %s %d\n",asctime(brokentime),myarg->ip,myreq->path,totalBytesWritten);
    close(serv_sock);
    close(myarg->connfd);
    printf("Thread %d exits\n", myarg->id);
    pthread_exit(NULL);
}
