/*
 * macD_c
 * written by: Nathan Koop
 *
 * description:
 *    used to create a client connection to the macD server socket.
 *    mannages the user input for stat and kill commands
 *    as well as displaying the results from the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "macD_c.h"

#define SERVER_PATH "macd.socket.server"
#define CLIENT_PATH "macd.socket.client"

struct sockaddr_un CLIENT_ADDRESS;
int CLIENT_SOCK;
int STATE = 0; //0: normal, 1: kill command used, 2: stat command used
pthread_t THREAD;
pthread_mutex_t STATELOCK;

/*
 * main
 * description:
 *     called when the executable is started.
 *     starts the client
 * parameters:
 *     argc: the number of command line arguments
 *     argv: array of strings representing the command line arguments.
 * returns:
 *     0 if executes properly.
 */
int main(int argc, char *argv[]){
	start_client();
	return 0;
}

/*
 * close_client
 * description:
 *     closes the client socket and joins the threads of this client.
 * post-condition:
 *     threads are joined, the connection is closed, this program termiantes.
 */
void close_client(){
	fprintf(stderr, "Closing Client\n");
	close(CLIENT_SOCK);
	kill(getpid(),SIGKILL);
	exit(1);
}

/*
 * client_reciever
 * description:
 *     a thread function that is responsible for recieving data for the client.
 */
void client_reciever(){
	while(1){
		char *buffer = malloc(5);
		memset(buffer,0,5);
		int rc = recv(CLIENT_SOCK, buffer, 4, 0);
		if (rc == -1){
			fprintf(stderr, "Recieve Error\n");
			close(CLIENT_SOCK);
			exit(1);
		} else if(buffer != NULL){
			if(buffer[0] == '\0'){
				close_client();
			}
			buffer[4] = '\0';
			pthread_mutex_lock(&STATELOCK);
			if(STATE != 2){ //read as string
				fprintf(stderr, "Echo From Server: %s\n", buffer);
			}else{	//read as int
				int value = *(int *)buffer;
				fprintf(stderr, "There are %d running processes\n", value);
			}
			pthread_mutex_unlock(&STATELOCK);
		}
		pthread_mutex_lock(&STATELOCK);
		STATE = 0;
		pthread_mutex_unlock(&STATELOCK);
		free(buffer);
	}
}

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
void str_lower(char *str){
	int index = 0;
	while(str[index] != '\0'){
		char c = tolower(str[index]);
		str[index] = c;
		index++;
	}
}

/*
 * convert_char_to_digit
 * description:
 *     returns the integer representation of the single character, c.
 * parameters:
 *     c: the character to convert to integer
 * returns:
 *     the integer representation of character c.
 */
int convert_char_to_digit(char c){
	switch (c){
	case '0':
		return 0;
		break;
	case '1':
		return 1;
		break;
	case '2':
		return 2;
		break;
	case '3':
		return 3;
		break;
	case '4':
		return 4;
		break;
	case '5':
		return 5;
		break;
	case '6':
		return 6;
		break;
	case '7':
		return 7;
		break;
	case '8':
		return 8;
		break;
	case '9':
		return 9;
		break;
	}
	return -1;
}

/*
 * client_sender
 * description:
 *     scans for input from the user and sends it to the server.
 */
void *client_sender(void *vargp){
	while(1){
		pthread_mutex_lock(&STATELOCK);
		if(STATE != 1){ //read in 4 byte string
			pthread_mutex_unlock(&STATELOCK);
			char *buffer = malloc(5);
			fread(buffer, 1, 5, stdin);
			buffer[4] = '\0';
			str_lower(buffer);
			if(strcmp(buffer, "kill") == 0){
				pthread_mutex_lock(&STATELOCK);
				STATE = 1;
				pthread_mutex_unlock(&STATELOCK);
			}else if(strcmp(buffer, "stat") == 0){
				pthread_mutex_lock(&STATELOCK);
				STATE = 2;
				pthread_mutex_unlock(&STATELOCK);
			}
			int rc = send(CLIENT_SOCK, buffer, 4, 0);
			if(rc == -1){
				fprintf(stderr, "Sending Error\n");
				close_client();
			}
			free(buffer);
		}else{ //read in integer
			pthread_mutex_unlock(&STATELOCK);
			int x = 0;
			char c;
			int r = fread(&c, 1, 1, stdin);
			if(r == EOF || r == 0){
				pthread_mutex_lock(&STATELOCK);
				STATE = 0;
				pthread_mutex_unlock(&STATELOCK);
			}
			int d = convert_char_to_digit(c);
			if(d!=-1){
				x = d;
			}
			while(r != 0 && r!=EOF){
				r = fread(&c, 1, 1, stdin);
				d = convert_char_to_digit(c);
				if(d == -1){
					break;
				}else{
					x = (x*10)+r;
				}
			}
			int rc = send(CLIENT_SOCK, &x, 4, 0);
			if(rc == -1){
				fprintf(stderr, "Sending Error\n");
				close_client();
			}
			pthread_mutex_lock(&STATELOCK);
			STATE = 0;
			pthread_mutex_unlock(&STATELOCK);
		}
	}
}

/*
 * start_client
 * description:
 *     initializes the client and connects it to the server socket.
 *     it then starts the client_sender and client_reciever functions
 *     in a concurrent fashion.
 */
void start_client(){
	int client_sock, rc;
	int len;
	struct sockaddr_un server_address;
	struct sockaddr_un client_address;
	memset(&server_address, 0, sizeof(struct sockaddr_un));
	memset(&client_address, 0, sizeof(struct sockaddr_un));
	client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (client_sock == -1){
		fprintf(stderr, "Socket Error\n");
		exit(1);
	}
	client_address.sun_family = AF_UNIX;
	strncpy(client_address.sun_path, CLIENT_PATH, 19);
	len = sizeof(client_address);
	unlink(CLIENT_PATH);
	rc = bind(client_sock, (struct sockaddr *) &client_address, len);
	if(rc == -1){
		fprintf(stderr, "Binding Error\n");
		close(client_sock);
		exit(1);
	}
	server_address.sun_family = AF_UNIX;
	strncpy(server_address.sun_path, SERVER_PATH, 19);
	rc = connect(client_sock, (struct sockaddr *) &server_address, len);
	if (rc == -1) {
		fprintf(stderr, "Connection Error\n");
		close(client_sock);
		exit(1);
	}
	CLIENT_ADDRESS = client_address;
	CLIENT_SOCK = client_sock;
	pthread_t send_thread;
	pthread_create(&send_thread, NULL, client_sender, NULL);
	THREAD = send_thread;
	client_reciever();
}
