#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include "blather.h"

client_t *server_get_client(server_t *server, int idx) {
	client_t * requested = &server->client[idx];
	return requested;
}
// Gets a pointer to the client_t struct at the given index. If the
// index is beyond n_clients, the behavior of the function is
// unspecified and may cause a program crash.

void server_start(server_t *server, char *server_name, int perms) {
	remove(server_name);
	server->n_clients = 0;
	strcpy(server->server_name, server_name);
	if(mkfifo(server_name, perms) == -1) {
		perror("ERROR: Could not create server!");
	}
	server->join_fd = open(server_name, DEFAULT_PERMS);
	return;
}
// Initializes and starts the server with the given name. A join fifo
// called "server_name.fifo" should be created. Removes any existing
// file of that name prior to creation. Opens the FIFO and stores its
// file descriptor in join_fd._
//
// ADVANCED: create the log file "server_name.log" and write the
// initial empty who_t contents to its beginning. Ensure that the
// log_fd is position for appending to the end of the file. Create the
// POSIX semaphore "/server_name.sem" and initialize it to 1 to
// control access to the who_t portion of the log.

void server_shutdown(server_t *server) {
	mesg_t shutdownmsg;
	shutdownmsg.kind = BL_SHUTDOWN;
	server_broadcast(server, &shutdownmsg);
	// SEND BL_SHUTDOWN message to all clients
	for(int i = server->n_clients; i >= 0; i--) {
		server_remove_client(server, i);
	}

	close(server->join_fd);
	unlink(server->server_name);
	exit(0);
};
// Shut down the server. Close the join FIFO and unlink (remove) it so
// that no further clients can join. Send a BL_SHUTDOWN message to all
// clients and proceed to remove all clients in any order.
//
// ADVANCED: Close the log file. Close the log semaphore and unlink
// it.

int server_add_client(server_t *server, join_t *join) {
	//creating client_t struct and copying fields from join_t
	client_t client;
	strcpy(client.name, join->name);
	strcpy(client.to_client_fname, join->to_client_fname);
	strcpy(client.to_server_fname, join->to_server_fname);

	//opening file descriptors and preparing for server
	client.to_client_fd = open(client.to_client_fname, DEFAULT_PERMS);
	client.to_server_fd = open(client.to_server_fname, DEFAULT_PERMS);
	client.data_ready = 0;

	//adding to server
	if (server->n_clients == MAXCLIENTS) {
		return 1;
	}
	server->client[server->n_clients] = client;
	server->n_clients++;

	mesg_t joinmsg_;
	mesg_t *joinmsg = &joinmsg_;
	joinmsg->kind = BL_JOINED;
	strncpy(joinmsg->name, client.name, MAXNAME);
	server_broadcast(server, joinmsg);

	return 0;
}
// Adds a client to the server according to the parameter join which
// should have fields such as name filed in.  The client data is
// copied into the client[] array and file descriptors are opened for
// its to-server and to-client FIFOs. Initializes the data_ready field
// for the client to 0. Returns 0 on success and non-zero if the
// server as no space for clients (n_clients == MAXCLIENTS).

int server_remove_client(server_t *server, int idx) {
	client_t * client = server_get_client(server, idx);
	close(client->to_client_fd);
	close(client->to_server_fd);

	client_t tempclient;
	for(int i = idx; i < (server->n_clients)-1; i++) {
		memcpy(&tempclient, &(server->client[i+1]), sizeof(client_t));
		memcpy(&(server->client[i]), &tempclient, sizeof(client_t));
	}

	server->n_clients--;
	return 0;
}
// Remove the given client likely due to its having departed or
// disconnected. Close fifos associated with the client and remove
// them.  Shift the remaining clients to lower indices of the client[]
// preserving their order in the array; decreases n_clients.

int server_broadcast(server_t *server, mesg_t *mesg) {
	for (int i = 0; i < server->n_clients; i++) {
		int fd = open(server->client[i].to_client_fname, DEFAULT_PERMS);
		int ret = write(fd, mesg, sizeof(mesg_t));
		if(ret == -1) {
			printf("ERROR WRITING\n");
		}
		close(fd);
	}
	return 0;
}
// Send the given message to all clients connected to the server by
// writing it to the file descriptors associated with them.
//
// ADVANCED: Log the broadcast message unless it is a PING which
// should not be written to the log.

void server_check_sources(server_t *server) {
	int numclients = server->n_clients;
	struct pollfd fds[numclients+1];

	for(int i = 0; i < numclients; i++) {
		fds[i].fd = open(server->client[i].to_server_fname, DEFAULT_PERMS);
		fds[i].events = POLLIN;
	}
	fds[numclients].fd = server->join_fd;
	fds[numclients].revents = 0;
	fds[numclients].events = POLLIN;

	//printf("calling poll()\n");
		//blocks and polls through fds until os ready
	int ret = poll(fds, numclients+1, 1000);
	if (ret < 0) {
		perror("poll() failed!\n");
	}
	for(int i = 0; i < numclients + 1; i++) {
		if ((fds[i].revents & POLLIN)) {
			if (i == numclients) {
				server->join_ready = 1;
			} else {
				server->client[i].data_ready = 1;
			}		
		}
	}
	return;
}
// Checks all sources of data for the server to determine if any are
// ready for reading. Sets the servers join_ready flag and the
// data_ready flags of each of client if data is ready for them.
// Makes use of the poll() system call to efficiently determine
// which sources are ready.

int server_join_ready(server_t *server) {
	return server->join_ready;
}
// Return the join_ready flag from the server which indicates whether
// a call to server_handle_join() is safe.

int server_handle_join(server_t *server) {
	//printf("READY STATUS: %d\n", server_join_ready(server));
	if (server_join_ready(server)) {
		join_t join;
		read(server->join_fd, &join, sizeof(join_t));
		server_add_client(server, &join);
		server->join_ready = 0;
		return 0;
	}
	return 1;
}
// Call this function only if server_join_ready() returns true. Read a
// join request and add the new client to the server. After finishing,
// set the servers join_ready flag to 0.

int server_client_ready(server_t *server, int idx) {
	return server->client[idx].data_ready;
}
// Return the data_ready field of the given client which indicates
// whether the client has data ready to be read from it.

int server_handle_client(server_t *server, int idx) {
	if(server_client_ready(server, idx)) {
		mesg_t message;
		client_t * client = server_get_client(server, idx);
		read(open(client->to_server_fname, DEFAULT_PERMS), &message, sizeof(mesg_t));
		if (message.kind == BL_MESG) {
			server_broadcast(server, &message);
		} else if (message.kind == BL_DEPARTED) {
			int client_num = -1;
			for(int i = 0; i < server->n_clients; i++){
				if(strcmp(server->client[i].name, message.name) == 0) {
					client_num = i;
				}
			}
			printf("REMOVING CLIENT %d\n", client_num);
			server_remove_client(server, client_num);
			server_broadcast(server, &message);
		}
		client->data_ready = 0;
		return 0;
	}
	return 1;
	
}
// Process a message from the specified client. This function should
// only be called if server_client_ready() returns true. Read a
// message from to_server_fd and analyze the message kind. Departure
// and Message types should be broadcast to all other clients.  Ping
// responses should only change the last_contact_time below. Behavior
// for other message types is not specified. Clear the client's
// data_ready flag so it has value 0.
//
// ADVANCED: Update the last_contact_time of the client to the current
// server time_sec.

/*
void server_tick(server_t *server);
// ADVANCED: Increment the time for the server

void server_ping_clients(server_t *server);
// ADVANCED: Ping all clients in the server by broadcasting a ping.

void server_remove_disconnected(server_t *server, int disconnect_secs);
// ADVANCED: Check all clients to see if they have contacted the
// server recently. Any client with a last_contact_time field equal to
// or greater than the parameter disconnect_secs should be
// removed. Broadcast that the client was disconnected to remaining
// clients.  Process clients from lowest to highest and take care of
// loop indexing as clients may be removed during the loop
// necessitating index adjustments.

void server_write_who(server_t *server);
// ADVANCED: Write the current set of clients logged into the server
// to the BEGINNING the log_fd. Ensure that the write is protected by
// locking the semaphore associated with the log file. Since it may
// take some time to complete this operation (acquire semaphore then
// write) it should likely be done in its own thread to preven the
// main server operations from stalling.  For threaded I/O, consider
// using the pwrite() function to write to a specific location in an
// open file descriptor which will not alter the position of log_fd so
// that appends continue to write to the end of the file.

void server_log_message(server_t *server, mesg_t *mesg);
// ADVANCED: Write the given message to the end of log file associated
// with the server.*/