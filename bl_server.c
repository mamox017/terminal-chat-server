#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include "blather.h"

server_t server;

// Function run when a SIGINT is sent to the program
void handle_SIGINT(int sig_num) {
  server_shutdown(&server);
}
 
// Function run when a SIGTERM is sent to the program
void handle_SIGTERM(int sig_num) {
  server_shutdown(&server);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("usage: ./bl_server <name>\n");
		return 1;
	}

	// signal handlers
	struct sigaction my_sa = {};
  	sigemptyset(&my_sa.sa_mask);
  	my_sa.sa_flags = SA_RESTART;
 	my_sa.sa_handler = handle_SIGTERM;
  	sigaction(SIGTERM, &my_sa, NULL);
 	my_sa.sa_handler = handle_SIGINT;
  	sigaction(SIGINT, &my_sa, NULL);

	//setting up the join fifo
	//server waits for clients to write join requests to FIFO
	char* fifo_name = strdup(argv[1]);
	strcat(fifo_name,".fifo");

	//setting up server
	server.n_clients = 0;
	server_start(&server, fifo_name, DEFAULT_PERMS);
	int count = 0;
	while(1) {
		printf("RUNTIME: %d, num of clients: %d\n", count, server.n_clients);
		server_check_sources(&server);
		server_handle_join(&server);
		for(int i = 0; i < server.n_clients; i++) {
			if(server_client_ready(&server, i)) {
				server_handle_client(&server, i);
			}
		}
		count++;
	}

	remove(fifo_name);
	return 0;
}