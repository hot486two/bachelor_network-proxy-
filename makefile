proxy : proxy.o
	gcc -o proxy proxy.o -lpthread
proxy.o : proxy.c
	gcc -c proxy.c
