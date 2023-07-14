/*
 * server_listener
 * description:
 *     a thread function that listens for new client connections.
 *     if a new client is detected its socket is added to the client list.
 */
void *server_listener(void *argvp);

/*
 * server_mannager
 * description:
 *     a thread function that detects messages from the clients and sends
 *     the appropriate response to these clients.
 */
void *server_mannager(void *void_client);

/*
 * add_client
 * description:
 *     adds the provided client to the list of currently connected clients.
 * parameter:
 *     client_sock: the client socket to be added to the list of clients.
 * post-condition:
 *     client_sock is appended to CLIENTS, CLIENTS is resized if needed.
 */
void add_client(int client_sock);

/*
 * start_server
 * description:
 *     initializes the server socket and starts the 
 *     server_mannager and server_listener threads.
 * post-condition:
 *     SERVER_SOCK is set to the server socket.
 *     server is initialized.
 */
void start_server();

/*
 * close_server
 * description:
 *     terminates all clients then closes the server socket.
 * post-condition:
 *     server socket and all connected client sockets are closed.
 */
void close_server();
