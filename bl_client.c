#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "blather.h"

char name_for_handler[MAXNAME];
int serv_for_handler;
simpio_t simpio_actual;
simpio_t *simpio = &simpio_actual;
pthread_t userpthread;
pthread_t serverpthread;

// Function run when a SIGINT is sent to the program
void handle_SIGINT(int sig_num) {
  mesg_t depart;
  depart.kind = BL_DEPARTED;
  strcpy(depart.name, name_for_handler);
  write(serv_for_handler, &depart, sizeof(depart));
  simpio_reset_terminal_mode();
  exit(0);
}
 
// Function run when a SIGTERM is sent to the program
void handle_SIGTERM(int sig_num) {
  mesg_t depart;
  depart.kind = BL_DEPARTED;
  strcpy(depart.name, name_for_handler);
  write(serv_for_handler, &depart, sizeof(depart));
  simpio_reset_terminal_mode();
  exit(0);
}

void *userthread(void *arg) {
	// parsing fifonames to get server and client names
	join_t *fifos = (join_t *) arg;
	char serv_name[MAXNAME];
	strcpy(serv_name, fifos->to_server_fname);
	char client_name[MAXNAME];
	strcpy(client_name, fifos->to_client_fname);
	int serv_fd = open(serv_name, DEFAULT_PERMS);
	serv_for_handler = serv_fd;

  	while(!simpio->end_of_input) {
  		simpio_reset(simpio);
  		iprintf(simpio, "");
  		while(!simpio->line_ready && !simpio->end_of_input){          // read until line is complete
      		simpio_get_char(simpio);
    	}
  		if(simpio->line_ready) {
  			//iprintf(simpio, "[%s]: %s\n",fifos->name, simpio->buf);
  			mesg_t message;
  			mesg_kind_t kind = 10;
  			message.kind = kind;
  			strcpy(message.body, simpio->buf);
  			strcpy(message.name, fifos->name);
  			//iprintf(simpio, "[%s]: %s\n", fifos->name, message.body);
  			int ret = write(serv_fd, &message, sizeof(mesg_t));
  			if (ret == -1) {
  				iprintf(simpio, "UNSUCCESSFUL WRITE!\n");
  			}

  			simpio->line_ready = 0;
  		}
  	}
	
	mesg_t depart;
  	depart.kind = BL_DEPARTED;
  	strcpy(depart.name, name_for_handler);
  	write(serv_for_handler, &depart, sizeof(depart));
	pthread_cancel(serverpthread);

	return NULL;
}

void *serverthread(void *arg) {
	join_t *fifos = (join_t *) arg;
	char serv_name[MAXNAME];
	strcpy(serv_name, fifos->to_server_fname);
	char client_name[MAXNAME];
	strcpy(client_name, fifos->to_client_fname);
	int client_fd = open(client_name, DEFAULT_PERMS);

	while(1) {
		mesg_t readmsg;
		int nread = read(client_fd, &readmsg, sizeof(mesg_t));
		if (nread != sizeof(mesg_t)) {
			iprintf(simpio, "nread couldn't sufficiently read bytes\n");
			return NULL;
		}
		if(readmsg.kind == BL_JOINED) {
			iprintf(simpio, "-- %s JOINED --\n", readmsg.name);
		} else if(readmsg.kind == BL_MESG) {
			iprintf(simpio, "[%s] : %s\n", readmsg.name, readmsg.body);
		} else if(readmsg.kind == BL_DEPARTED) {
			iprintf(simpio, "-- %s DEPARTED --\n", readmsg.name, readmsg.body);
		} else if(readmsg.kind == BL_SHUTDOWN) {
			iprintf(simpio, "!!! server is shutting down !!!\n");
			simpio_reset_terminal_mode();
		}
	}
	
	return NULL;
}

int main(int argc, char *argv[]) {
	//argument checking
	if (argc < 3) {
		printf("usage: /bl_client <server> <nickname>\n");
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


	//setting up names
	char* joinserv_name = strdup(argv[1]);
	strcat(joinserv_name, ".fifo");

	//naming fifos
	char serv_name[MAXNAME];
	sprintf(serv_name, "%dserver.fifo", getpid());
	char client_name[MAXNAME];
	sprintf(client_name, "%dclient.fifo", getpid());

	//removing old fifos and creating user fifos
	remove(client_name);
	remove(serv_name);
	if((mkfifo(client_name, DEFAULT_PERMS) == -1) || (mkfifo(serv_name, DEFAULT_PERMS) == -1)) {
		perror("ERROR: Could not create client fifos!");
		return 1;
	}
	
	int fd = open(joinserv_name, DEFAULT_PERMS);
	if (fd < 0) {
		perror("ERROR: Server does not exist!");
	}

	//joining joinserv fifo
	join_t user;
	strcpy(name_for_handler, strdup(argv[2]));
	strcpy(user.name, name_for_handler);
	strcpy(user.to_client_fname, client_name);
	strcpy(user.to_server_fname, serv_name);
	int written = write(fd, &user, sizeof(user));
	if (written < 0) {
		perror("User not written to server!\n");
	}

	char prompt[MAXNAME+2];
	strcpy(prompt, user.name);
	strcat(prompt, ">>");
	simpio_set_prompt(simpio, prompt);
	simpio_reset(simpio);
	simpio_noncanonical_terminal_mode();

	//multithreading
	pthread_create(&userpthread, NULL, userthread, &user);
	pthread_create(&serverpthread, NULL, serverthread, &user);
	pthread_join(userpthread, NULL);
	pthread_join(serverpthread, NULL);

	simpio_reset_terminal_mode();
	printf("\n");
	remove(client_name);
	remove(serv_name);
	return 0;
}
