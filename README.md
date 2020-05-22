# (C) Terminal Chat Server
This C language chat server makes use of fifos and signals in order to communicate through terminals!

# Directions
Build the program with the following command
- make

Build the tests and with the following command
- make test

## Server
Run the server in a terminal with the given command format
- /bl_server (server name)
![Server Picture](https://i.imgur.com/IDNOJRN.png)

## Clients
Run other terminals with the given command format for clients
- ./bl_client (server name) (nickname)
![Client Picture](https://i.imgur.com/3iBbbYY.png)


Once joined, you can send messages to others who are in the chat server.  If you close your terminal or press Ctrl+C, you depart from the server and a departing message is sent to all others remaining in the server.
