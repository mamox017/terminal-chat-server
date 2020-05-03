all: bl_server_maker.o bl_client_maker.o
include test_Makefile

bl_server.o:
	gcc -Wall -g -c bl_server.c

server_funcs.o:
	gcc -Wall -g -c server_funcs.c

util.o:
	gcc -Wall -g -c util.c

bl_server_maker.o: server_funcs.o bl_server.o util.o
	gcc -Wall -g -o bl_server bl_server.o server_funcs.o util.o -lpthread

bl_client.o:
	gcc -Wall -g -c bl_client.c

simpio.o:
	gcc -Wall -g -c simpio.c

bl_client_maker.o: bl_client.o simpio.o util.o
	gcc -Wall -g -o bl_client bl_client.o simpio.o util.o -lpthread



clean:
	rm -f *.o *.fifo all
