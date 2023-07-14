/*
 * close_client
 * description:
 *     closes the client socket and joins the threads of this client.
 * post-condition:
 *     threads are joined, the connection is closed, this program termiantes.
 */
void close_client();

/*
 * client_reciever
 * description:
 *     a thread function that is responsible for recieving data for the client.
 */
void client_reciever();

/*
 * str_lower
 * description:
 *     sets the given string to its lowercase form.
 * parameters:
 *     str: the string to convert to lowercase.
 * pre-condition:
 *     str is initialized
 * post-condition:
 *     str is now in its lower case form.
 */
void str_lower(char *str);

/*
 * convert_char_to_digit
 * description:
 *     returns the integer representation of the single character, c.
 * parameters:
 *     c: the character to convert to integer
 * returns:
 *     the integer representation of character c.
 */
int convert_char_to_digit(char c);

/*
 * client_sender
 * description:
 *     scans for input from the user and sends it to the server.
 */
void *client_sender(void *vargp);

/*
 * start_client
 * description:
 *     initializes the client and connects it to the server socket.
 *     it then starts the client_sender and client_reciever functions
 *     in a concurrent fashion.
 */
void start_client();
