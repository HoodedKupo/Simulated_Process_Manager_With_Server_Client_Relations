/*
 * macD
 * written by: Nathan Koop
 *
 * description:
 *     the executable detects a -i flag to find the file to open.
 *     for each line in the file the executable will create a new process
 *     where the process created is specified by the file.
 *     The program will then send a normal report every 5 seconds
 *     in which the cpu usage, as a percent, and memory usage, in MB
 *     is displayed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include "macD.h"
#include "macD_server.h"
#define SERVER_PATH "macd.socket.server"

int MAX_ARG_LENGTH = 1000;
int MAX_PROCESSES = 10;
int MAX_SEGMENT_LENGTH = 100;
double TARGET_TIME = -1;
int KILL_STATE = -1;
double START_TIME = -1;
int SERVER_SOCK;
int *CLIENTS;
int CLIENT_LIST_SIZE;
int MAX_CLIENTS = 10;
FILE *OUTPUT_FILE;
int *PIDS;
int WAITING_KILL = 0;
pthread_mutex_t KILLLOCK;
pthread_mutex_t PIDLOCK;


/*
 * main
 * description:
 *     called when the program is executed.
 *     checks for the -i flag and opens the following file.
 * parameters:
 *     argc: number of command line arguments
 *     argv: array of strings containing the command line arguments
 */
int main(int argc, char *argv[])
{
	OUTPUT_FILE = stdout;
	read_flags(argc, argv);
}

/*
 * read_flags
 * description:
 *     reads through all flags passed and applies, or initiates
 *     the appropriate changes as indicated by the flag.
 * parameters:
 *     argc: the number of command line arguments.
 *     argv: an array of strings representing the command line arguments.
 */
void read_flags(int argc, char *argv[]){
	int opt;
	char *i = NULL;
	int q = 0;
	while ((opt = getopt(argc, argv, "i:qho:")) != -1) {
		if(opt == 'i'){
			if (optarg == NULL) {
				printf("option requires an argument --i");
			} else {
				i = optarg;
			}
		} else if (opt == 'o') {
			if (optarg == NULL) {
				printf("option requires an argument --o");
			} else {
				FILE *fptr = fopen(optarg, "w");
				if (fptr == NULL) {
					printf("invalid file for argument --o");
				} else {
					OUTPUT_FILE = fptr;
				}
			}
		} else if (opt == 'q') {
			q = 1;
		}
	}
	if (i != NULL){
		read_file(i, q);
		if (PIDS == NULL){
			exit(1);
		}
		START_TIME = time(NULL);
		start_server();
		register_handler();
		periodic_reports(PIDS);
	}
}

/*
 * kill_process
 * description:
 *    kills the process and the specified index in the array of processes 
 *    checks if the indicated process is running then terminates it.
 * parameters:
 *    index: the index in the running array of processes of the process to terminate
 * post-condition:
 *    specified process is terminated.
 */
char *kill_process(int index){
	//check if index is valid
	int i = 0;
	pthread_mutex_lock(&PIDLOCK);
	while(PIDS[i] != -1){
		if(i == index){
			pid_t result = waitpid(PIDS[i], NULL, WNOHANG);
			if(result == 0){ // kill process
				kill(PIDS[i], SIGKILL);
				pthread_mutex_unlock(&PIDLOCK);
				return "SUCC";
			}
			pthread_mutex_unlock(&PIDLOCK);
			return "FAIL";
		}
		i++;
	}
	pthread_mutex_unlock(&PIDLOCK);
	return "FAIL";
}

/*
 * get_num_running
 * description:
 *     calculates the number of running processes.
 * parameters:
 *     pids: the array of child process id's
 * returns:
 *     number of processes in the given array that are running.
 */
int get_num_running(int *pids){
	int index = 0;
	int return_value = 0;
	while(pids[index]!=-1){
		pid_t result = waitpid(pids[index], NULL, WNOHANG);
		if(result == 0)
			return_value++;
		index++;
	}
	return return_value;
}

/*
 * add_client
 * description:
 *     creates a new thread to read the specified client.
 * parameter:
 *     client_sock: the client socket to be observed.
 * post-condition:
 *     a new thread is running server_mannager on the given client_sock.
 */
void add_client(int client_sock){
	pthread_t new_thread;
	pthread_create(&new_thread, NULL, server_mannager,(void*)&client_sock);
}

/*
 * server_listener
 * description:
 *     a thread function that listens for new client connections.
 *     if a new client is detected its socket is added to the client list.
 */
void *server_listener(void* argvp){
	while(1){
		struct sockaddr_un client_address;
		memset(&client_address, 0, sizeof(struct sockaddr_un));
		unsigned int len;
		int client_sock, rc;
		client_sock = accept(SERVER_SOCK, (struct sockaddr *) &client_address, &len);
		if(client_sock == -1){
			fprintf(stderr, "Acceptance error %s\n", strerror(errno));
			close_server();
			exit(1);
		}
		rc = getpeername(client_sock, (struct sockaddr *) &client_address, &len);
		if (rc == -1){
			fprintf(stderr, "Getpeername Error\n");
			close_server();
			exit(1);
		}
		add_client(client_sock);
	}
}

/*
 * close_server
 * description:
 *     closes the server socket.
 * post-condition:
 *     server socket is closed.
 */
void close_server(){
	close(SERVER_SOCK);
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
 * server_mannager
 * description:
 *     a thread function that detects messages from the clients and sends
 *     the appropriate response to these clients.
 */
void *server_mannager(void* void_client){
	int rc;
	int client_sock = *(int*)void_client;
	while(1){
		char *buffer = malloc(5);
		memset(buffer, 0, 4);
		int rec = recv(client_sock, buffer, 4, 0);
		if(rec == -1){
			fprintf(stderr, "receive error %s\n",strerror(errno));
			close_server();
			exit(1);
		}
		buffer[4] = '\0';
		str_lower(buffer);
		pthread_mutex_lock(&KILLLOCK);
		if(WAITING_KILL == 1){
			pthread_mutex_unlock(&KILLLOCK);
			//parse buffer as integer
			int value = *((int *)buffer);
			char *response = kill_process(value);
			rc = send(client_sock, response, 4, 0);
			if(rc == -1){
				fprintf(stderr, "Sending Error\n");
				close_server();
				exit(1);
			}
			pthread_mutex_lock(&KILLLOCK);
			WAITING_KILL = 0;
			pthread_mutex_unlock(&KILLLOCK);
		} else if(strcmp(buffer, "stat") == 0){
			pthread_mutex_unlock(&KILLLOCK);
			pthread_mutex_lock(&PIDLOCK);
			int results = get_num_running(PIDS);
			pthread_mutex_unlock(&PIDLOCK);
			rc = send(client_sock, &results, 4, 0);
			if(rc == -1){
				fprintf(stderr, "Sending Error\n");
				close_server();
				exit(1);
			}
		} else if(strcmp(buffer,"kill") == 0) {
			WAITING_KILL = 1;
			pthread_mutex_unlock(&KILLLOCK);
		} else {
			pthread_mutex_unlock(&KILLLOCK);
		}
		free(buffer);
	}
}

/*
 * start_server
 * description:
 *     initializes the server socket and starts the 
 *     server_mannager and server_listener threads.
 * post-condition:
 *     SERVER_SOCK is set to the server socket.
 *     server is initialized.
 */
void start_server()
{
	int server_sock, rc;
	struct sockaddr_un server_address;
	memset(&server_address, 0, sizeof(struct sockaddr_un));
	server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server_sock == -1){
		fprintf(stderr, "Socket error\n");
		exit(1);
	}

	//set up binding.
	server_address.sun_family = AF_UNIX;
	strncpy(server_address.sun_path, SERVER_PATH, 19);
	unsigned int len = sizeof(server_address);
	unlink(SERVER_PATH);
	rc = bind(server_sock, (struct sockaddr *) &server_address, len);
	if (rc == -1) {
		fprintf(stderr, "binding error\n");
		close(server_sock);
		exit(1);
	}

	//set up listening.
	rc = listen(server_sock, 10);
	if (rc == -1){
		fprintf(stderr, "listening error\n");
		close(server_sock);
		exit(1);
	}
	//initialize CLIENT_SOCKS
	CLIENTS = malloc(sizeof(int)*MAX_CLIENTS);
	CLIENT_LIST_SIZE = MAX_CLIENTS;
	for(int i = 0; i<MAX_CLIENTS; i++){
		CLIENTS[i] = -1;
	}
	SERVER_SOCK = server_sock;
	pthread_t listen_thread;
	pthread_create(&listen_thread, NULL, server_listener, NULL);
}

/*
 * get_num_args
 * description:
 *     counts the number of arguments in a given line of text
 *     assuming this line is from the process list file.
 *     counts the number of spaces in the line and uses this
 *     to determin the number of args.
 * parameters:
 *     line: a string of the command to count the args of
 * pre-condition:
 *    line is initialized.
 * returns:
 *     number of args in line.
 */
int get_num_args(char *line)
{
	int len = strlen(line);
	int count = 0;

	for (int i = 0; i < len; i++) {
		if (line[i] == ' ')
			count++;
	}
	return count+1;
}

/*
 * get_args
 * description:
 *     creates a list of strings containing the arguments
 *     of the given line, which represents a process from the process list file.
 * parameters:
 *     line: a string of the command to get the args of
 * pre-condition:
 *     line is initialized.
 * returns:
 *     list of strings each element is an argument.
 */
char **get_args(char *line)
{
	int args = get_num_args(line);
	char **return_array = malloc(sizeof(char *)*args);
	char *token = strtok(line, " ");
	int index = 0;

	while (token != NULL) {
		return_array[index] = token;
		token = strtok(NULL, " ");
		index++;
	}
	return return_array;
}

/*
 * create_process
 * description:
 *     creates a new process using the fork function.
 *     changes the process to the process indicated by process_path.
 *     terminates any failed forks.
 * parameters:
 *     process_line: string containing path to the process to create.
 *     out_pid: pointer to store the pid of the new process to.
 *     quite_mode: 1 if the output of the process should be muted
 * pre-conditions:
 *     process_path is initialized.
 */
void create_process(char *process_line, int *out_pid, int quite_mode)
{
	int pid = fork();

	if (pid == -1)
		exit(1);
	if (pid == 0) {
		char **args = get_args(process_line);

		if (args[0] == NULL)
			exit(1);
		if(quite_mode == 1){
			int output_file = open("/dev/null",O_RDWR);
			if(output_file<0){
				fprintf(stderr, "couldn't open file");
				exit(1);
			}
			dup2(output_file, STDOUT_FILENO);
			close(output_file);
		}
		execvp(args[0], args);
		*out_pid = -1;
		exit(errno);
	} else {
		usleep(100000);
		int result = waitpid(pid, NULL, WNOHANG);

		if (*out_pid != -1 && result == 0)
			*out_pid = pid;
	}
}

/*
 * read_next_line
 * description:
 *     reads the next line in the given file object
 * parameters:
 *     fptr: pointer to the FILE object to read from.
 * pre-conditions:
 *     fptr is initialized and is a valid FILE object
 * returns:
 *     char array, string, containing the next line of the file.
 */
char *read_next_line(FILE *fptr)
{
	char *scanner = malloc(1);
	char *line = malloc(PATH_MAX+MAX_ARG_LENGTH);
	int index = 0;
	int extensions = 0;
	int read_items = fread(scanner, 1, 1, fptr);

	if (read_items != 1) {
		free(scanner);
		free(line);
		return NULL;
	}
	while (*scanner != '\n' && read_items == 1) {
		line[index] = *scanner;
		index++;
		if (index >= PATH_MAX + (MAX_ARG_LENGTH * (extensions+1))) {
			//grow the array.
			extensions++;
			char *temp = line;

			line = malloc(PATH_MAX + (MAX_ARG_LENGTH*(extensions+1)));
			strncpy(line, temp, PATH_MAX+MAX_ARG_LENGTH*(extensions)-1);
			free(temp);
		}
		read_items = fread(scanner, 1, 1, fptr);
	}
	free(scanner);
	line[index] = '\0';
	return line;
}

/*
 * convert_str_to_int
 * description:
 *     converts the given string to its integer form
 * parameters:
 *     str: the string to convert to an integer.
 * pre-condition:
 *     str is initialized.
 * returns:
 *     integer form of str if str is numeric.
 *     -1 otherwise.
 */
int convert_str_to_int(char *str)
{
	char c = str[0];
	int index = 0;
	int total = 0;

	while (c != '\0') {
		int add = -1;

		switch (c) {
		case '0':
			add = 0;
			break;
		case '1':
			add = 1;
			break;
		case '2':
			add = 2;
			break;
		case '3':
			add = 3;
			break;
		case '4':
			add = 4;
			break;
		case '5':
			add = 5;
			break;
		case '6':
			add = 6;
			break;
		case '7':
			add = 7;
			break;
		case '8':
			add = 8;
			break;
		case '9':
			add = 9;
			break;
		}
		if (add == -1)
			return -1; //str is not numeric
		total *= 10;
		total += add;
		index++;
		c = str[index];
	}
	return total;
}

/*
 * read_timer
 * description:
 *     checks if the given line indicates a timer.
 *     if the line is in the form "timelimit [integer]" then
 *     [integer] is returned.
 * parameters:
 *     line: line to check if its a timer.
 * returns:
 *     the value of the time limit if the given line is a timer.
 *     -1 otherwise.
 */
int read_timer(char *line)
{
	if (line == NULL)
		return -1;
	if (line[0] == '\0')
		return -1;
	char *token = strtok(line, " ");

	if (strcmp(token, "timelimit") == 0) {
		token = strtok(NULL, " ");
		if (token == NULL)
			return -1;
		int timer = convert_str_to_int(token);

		return timer;
	}
	return -1;
}

/*
 * get_month
 * description:
 *     converts the month in current time to
 *     its string representation.
 * parameters:
 *     current_time: tm structure containing the current time.
 * returns:
 *     string representation of the month.
 */
char *get_month(struct tm current_time)
{
	int month_num = current_time.tm_mon;
	char *month = "";

	switch (month_num) {
	case 0:
		month = "Jan";
		break;
	case 1:
		month = "Feb";
		break;
	case 2:
		month = "Mar";
		break;
	case 3:
		month = "Apr";
		break;
	case 4:
		month = "May";
		break;
	case 5:
		month = "June";
		break;
	case 6:
		month = "July";
		break;
	case 7:
		month = "Aug";
		break;
	case 8:
		month = "Sept";
		break;
	case 9:
		month = "Oct";
		break;
	case 10:
		month = "Nov";
		break;
	case 11:
		month = "Dec";
		break;
	}
	return month;
}

/*
 * get_day_of_week
 * description:
 *     converts the day in current time to
 *     its string representaion.
 * parameters:
 *     current_time: tm structure containing the current time.
 * returns:
 *     string representation of the day.
 */
char *get_day_of_week(struct tm current_time)
{
	int day = current_time.tm_wday;
	char *day_of_week = "";

	switch (day) {
	case 0:
		day_of_week = "Sun";
		break;
	case 1:
		day_of_week = "Mon";
		break;
	case 2:
		day_of_week = "Tue";
		break;
	case 3:
		day_of_week = "Wed";
		break;
	case 4:
		day_of_week = "Thu";
		break;
	case 5:
		day_of_week = "Fri";
		break;
	case 6:
		day_of_week = "Sat";
		break;
	}
	return day_of_week;
}

/*
 * display_date
 * description:
 *     displays the current time in the following format:
 *     [day_of_week], [month] [day], [year] [hour]:[min]:[sec] [AM/PM]
 */
void display_date(void)
{
	time_t t = time(NULL);
	struct tm current_time = *localtime(&t);
	char *wkday = get_day_of_week(current_time);
	char *month = get_month(current_time);
	int date = current_time.tm_mday;
	int year = 1900 + current_time.tm_year;
	int hour = current_time.tm_hour;
	char *xm = "AM";

	if (hour >= 12) {
		xm = "PM";
		hour -= 12;
	}
	if (hour == 0)
		hour = 12;
	int min = current_time.tm_min;
	int sec = current_time.tm_sec;

	fprintf(OUTPUT_FILE, "%s, %s %d, %d %d:%d:%d %s\n", wkday, month, date, year, hour, min, sec, xm);
}

/*
 * read_file
 * description:
 *     reads all lines in the given file.
 *     creates a process for each line in the file where the line
 *     indicates what process to create. The mutes the output of the child
 *     if quite_mode is set to 1.
 * parameters:
 *     file_path: string of the path to the file to read.
 *     quite_mode: 1 if it should mute child out put 0 otherwise.
 * pre-conditions:
 *     file_path is initialized.
 */
int *read_file(char *file_path, int quite_mode)
{
	FILE *fptr = fopen(file_path, "r");

	if (fptr == NULL) {
		fprintf(OUTPUT_FILE, "macD: %s not found", file_path);
		return NULL;
	}
	//read file for processes
	fprintf(OUTPUT_FILE, "%s", "Starting report, ");
	display_date();
	char *line = read_next_line(fptr);

	TARGET_TIME = read_timer(line);
	if (TARGET_TIME != -1) {
		free(line);
		line = read_next_line(fptr);
	}
	PIDS = malloc(sizeof(int)*MAX_PROCESSES);
	int index = 0;
	int line_number = 0;
	int len_pids = MAX_PROCESSES;
	int *out_pid = malloc(sizeof(int *));

	while (line != NULL) {
		*out_pid = -2;
		if (line[0] != '\0')
			create_process(line, out_pid, quite_mode);
		if (*out_pid >= 0) {
			PIDS[index] = *out_pid;
			char *path = strtok(line, " ");

			fprintf(OUTPUT_FILE, "[%d] %s, started successfully (pid: %d)\n", line_number, path, *out_pid);
			index++;
		} else {
			if (line[0] == '\0') {
				fprintf(OUTPUT_FILE, "[%d] badprogram , failed to start\n", line_number);
			} else {
				char *path = strtok(line, " ");

				fprintf(OUTPUT_FILE, "[%d] badprogram %s, failed to start\n", line_number, path);
			}
		}
		if (index >= len_pids - 1) {
			//increase size of pids
			len_pids = len_pids + MAX_PROCESSES;
			int *temp = PIDS;

			PIDS = malloc(sizeof(int)*len_pids);
			for (int i = 0; i < index; i++)
				PIDS[i] = temp[i];
			free(temp);
		}
		line_number++;
		free(line);
		line = read_next_line(fptr);
	}
	free(line);
	free(out_pid);
	fclose(fptr);
	PIDS[index] = -1;//to indicate end of array
	return PIDS;
}

/*
 * get_num_digits
 * description:
 *     counts the number of digits in i.
 * parameters:
 *    i: integer to count the digits of.
 * returns:
 *     the number of digits in i.
 */
int get_num_digits(int i)
{
	int digits = 0;

	if (i < 0)
		i = i*-1;
	if (i == 0)
		return 1;
	while (i >= 1) {
		digits += 1;
		i = i / 10;
	}
	return digits;
}

/*
 * convert_int_to_string
 * description:
 *     converts the given integer to a string.
 * parameters:
 *     i: integer to convert to string.
 * pre-condition:
 *     i>=0
 * returns:
 *     string representaion of i.
 */
char *convert_int_to_string(int i)
{
	if (i == 0)
		return "0";
	char *str = malloc(get_num_digits(i));
	int index = 0;

	while (i > 0) {
		int d = i % 10;

		str[index] = '0' + d;
		i -= d;
		i = i / 10;
		index++;
	}
	//reverse string
	int lptr = 0;
	int rptr = index-1;

	while (lptr < rptr) {
		char temp = str[rptr];

		str[rptr] = str[lptr];
		str[lptr] = temp;
		lptr++;
		rptr--;
	}
	return str;
}

/*
 * read_until_space
 * description:
 *     reads through the given file until it reaches a space
 *     or the end of a file.
 * parameters:
 *     fptr: FILE pointer to be read through.
 * pre-condition:
 *     fptr is initalized.
 * post-condition:
 *     the read from point in fptr is moved such that the next character read
 *     will be the character immediately after the space, or EOF if no space was found.
 * returns:
 *     1 if a space was found, meaning fptr not at EOF.
 *     0 if no space was found, meaning fptr is at EOF.
 */
int read_until_space(FILE *fptr)
{
	char *c = malloc(1);
	int read = fread(c, 1, 1, fptr);

	while (*c != ' ' && read == 1)
		read = fread(c, 1, 1, fptr);
	free(c);
	return read;
}

/*
 * get_next_segment
 * description:
 *     reads fptr until the next space character or EOF.
 *     returns all characters between read before finding a space.
 * parameters:
 *     fptr: FILE pointer to be read through.
 * pre-condition:
 *     fptr is initalized.
 * post-condition:
 *     the read from point in fptr is moved such that the next character read
 *     will be the character immediately after the space, or EOF if no space was found.
 * returns:
 *     char array of all characters read before finding a space or EOF.
 */
char *get_next_segment(FILE *fptr)
{
	char *segment = malloc(MAX_SEGMENT_LENGTH);
	int seg_size = MAX_SEGMENT_LENGTH;
	int index = 0;
	char *c = malloc(1);
	int read = fread(c, 1, 1, fptr);

	while (*c != ' ' && read == 1) {
		segment[index] = *c;
		index++;
		if (index >= seg_size) {
			//adjust size
			char *temp = segment;

			segment = malloc(seg_size+MAX_SEGMENT_LENGTH);
			seg_size += MAX_SEGMENT_LENGTH;
			for (int i = 0; i < index; i++)
				segment[i] = temp[i];
			free(temp);
		}
		read = fread(c, 1, 1, fptr);
	}
	free(c);
	segment[index] = '\0';
	return segment;
}

/*
 * get_cpu_usage
 * description:
 *     computes the total amount of time the process has spent
 *     on the cpu, measured in clock ticks by
 *     reading /proc/[pid]/stat to find the user time and kernal time.
 * parameters:
 *     pid: the process id of the process to compute the cpu usage for.
 * returns:
 *     the number of ticks the process has been on the cpu for
 *     or -1 if no such process with pid exists.
 */
int get_cpu_usage(int pid)
{
	char *path = malloc(12+get_num_digits(pid));

	path[0] = '\0';
	char *part1 = "/proc/";
	char *part2 = "/stat";

	strncat(path, part1, 6);
	char *str_pid = convert_int_to_string(pid);

	strncat(path, str_pid, get_num_digits(pid));
	free(str_pid);
	strncat(path, part2, 5);
	FILE *fptr = fopen(path, "r");

	free(path);
	if (fptr == NULL) {
		fclose(fptr);
		return -1;
	}
	//Read until the usertime section of the file
	// which is the 14th segment (after 13 spaces).
	for (int i = 0; i < 13; i++) {
		int r = read_until_space(fptr);

		if (r == 0) {
			fclose(fptr);
			return -1; //file not long enough
		}
	}
	//get user time
	char *s_user = get_next_segment(fptr);
	int user_time = atoi(s_user);
	//get kernal time
	char *s_kernal = get_next_segment(fptr);
	int kernal_time = atoi(s_kernal);

	free(s_user);
	free(s_kernal);
	fclose(fptr);
	return user_time + kernal_time;
}

/*
 * get_mem_usage
 * description:
 *     computes the amount of memory used by the process
 * parameters:
 *     pid: id of the process to compute memory usage of.
 * returns:
 *     the memory usage, in MB, of the given process
 *     or -1 if no such process exists.
 */
int get_mem_usage(int pid)
{
	char *path = malloc(13+get_num_digits(pid));

	path[0] = '\0';
	char *part1 = "/proc/";
	char *part2 = "/statm";

	strncat(path, part1, 6);
	char *str_pid = convert_int_to_string(pid);

	strncat(path, str_pid, get_num_digits(pid));
	free(str_pid);
	strncat(path, part2, 6);
	FILE *fptr = fopen(path, "r");

	free(path);
	if (fptr == NULL) {
		fclose(fptr);
		return -1;
	}
	//read contents of file, summing up all numbers as they are read.
	int sum = 0;
	char *segment = get_next_segment(fptr);

	while (strlen(segment) != 0) {
		sum += atoi(segment);
		free(segment);
		segment = get_next_segment(fptr);
	}
	free(segment);
	fclose(fptr);
	return sum/1024;
}

/*
 * initialize_cpu_counters
 * description:
 *     creates an array with a size equal to the amount of processes.
 *     this array will contain the cpu usage of each process from the
 *     previous reporting cycle.
 * parameters:
 *     pids: list of process ids
 *     num_processes: the length of pids, the number of processes.
 * pre-conditions:
 *     pids is initialized.
 *     num_processes is the length of pids.
 * returns:
 *     list of integers containing the number of ticks each process
 *     has run on the cpu for. This list is in the same order as pids.
 *     so the processes at pids[i] will have run for counters[i] ticks.
 */
int *initialize_cpu_counters(int *pids, int num_processes)
{
	int *counters = malloc(sizeof(int)*num_processes);

	for (int i = 0; i < num_processes; i++) {
		//get initial cpu usage
		counters[i] = get_cpu_usage(pids[i]);
		if (counters[i] < 0)
			counters[i] = 0;
	}
	return counters;
}

/*
 * len_pids
 * description:
 *     counts the number of elements in the pids list.
 *     this is calculated by searching the list until -1 is found.
 * parameters:
 *     pids: list of pids to calculate length of.
 * pre-conditions:
 *     pids is initialized.
 *     pids is terminated by a -1 element.
 * returns:
 *     the length of pids.
 */
int len_pids(int *pids)
{
	if (pids == NULL)
		return 0;
	int len = 0;

	while (pids[len] != -1)
		len++;
	return len;
}

/*
 * terminate_program
 * description:
 *     terminates this process and all children processes.
 *     It then displays the final status for all children
 *     and the total runtime of the process.
 * parameters:
 *     pids: list of all children process ids
 *     elapsed_time: the time the program has been running for.
 * pre-conditions:
 *     pids is initalized.
 * post-conditions:
 *     all processes with pid in pids list will be killed.
 *     this program will terminate.
 */
void terminate_program(int *pids, double elapsed_time)
{
	pthread_mutex_lock(&PIDLOCK);
	fprintf(OUTPUT_FILE, "%s", "Terminating, ");
	display_date();
	int pid = pids[0];
	int index = 0;

	while (pid != -1) {
		//check if process is still active
		pid_t result = waitpid(pid, NULL, WNOHANG);

		if (result == 0) {
			fprintf(OUTPUT_FILE, "[%d] %s\n", index, "Terminated");
			kill(pid, SIGKILL);
		} else {
			fprintf(OUTPUT_FILE, "[%d] %s\n", index, "Exited");
		}
		index++;
		pid = pids[index];
	}
	free(pids);
	pthread_mutex_unlock(&PIDLOCK);
	fprintf(OUTPUT_FILE, "Exiting (total time: %d seconds)\n", (int)(elapsed_time/1));
	close_server();
	exit(0);
}

/*
 * check_timer
 * description:
 *     checks if the program has run longer than the TARGET_TIME
 *     or if KILL_STATE == 1.
 *     if either of these condtions are true returns 1.
 * parameters:
 *     current_time: the current time. Created by calling time(NULL)
 * returns:
 *     1 if program has run for TARGET_TIME or KILL_STATE = 1.
 *     0 otherwise.
 */
int check_timer(double current_time)
{
	if (current_time - START_TIME >= TARGET_TIME && TARGET_TIME != -1)
		return 1;
	if (KILL_STATE == 1)
		return 1;
	return 0;
}

/*
 * display_proc_state
 * description:
 *     displays the cpu usage and mem usage of a process.
 * parameters:
 *     index: the index of the process in the pids array.
 *     cpu: the percentage of time this process has spent on the cpu.
 *     mem: the amount of memory, in MB, used by the process.
 */
void display_proc_state(int index, int cpu, int mem)
{
	char percent = '%';

	fprintf(OUTPUT_FILE, "[%d] Running, cpu usage: %d%c,", index, cpu, percent);
	fprintf(OUTPUT_FILE, " mem usage: %d MB\n", mem);
}

/*
 * periodic_reports
 * description:
 *     displays the status of all processes every 5 seconds.
 * parameters:
 *     pids: list of process ids
 * pre-conditions:
 *     pids is initialized.
 *     pids is terminated by a -1 element.
 */
void periodic_reports(int *pids)
{
	int *counters = initialize_cpu_counters(pids, len_pids(pids));
	int full_cpu_increase = 5*sysconf(_SC_CLK_TCK);

	while (1) {
		int done = 1;
		int index = 0;

		fprintf(OUTPUT_FILE, "%s\n", "...");
		fprintf(OUTPUT_FILE, "%s", "Normal report, ");
		display_date();
		pthread_mutex_lock(&PIDLOCK);
		while (pids[index] != -1) {
			pid_t result = waitpid(pids[index], NULL, WNOHANG);

			if (result == 0) {
				int cpu = get_cpu_usage(pids[index]);
				int cpu_percent = ((cpu - counters[index])*100);
				int mem = get_mem_usage(pids[index]);

				cpu_percent = cpu_percent/full_cpu_increase;
				counters[index] = cpu;
				done = 0;
				display_proc_state(index, cpu_percent, mem);
			} else {
				fprintf(OUTPUT_FILE, "[%d] Exited\n", index);
			}
			index++;
		}
		pthread_mutex_unlock(&PIDLOCK);
		if (done == 1) {
			double current_time = time(NULL);
			int total_time = (int)(current_time - START_TIME);

			fprintf(OUTPUT_FILE, "Exiting (total time: %d seconds)\n...\n", total_time);
			exit(0);
		}
		fprintf(OUTPUT_FILE, "%s\n", "...");
		double t = time(NULL);
		double current_time = t;

		while (current_time - t < 5) {
			current_time = time(NULL);
			if (check_timer(current_time) == 1)
				terminate_program(pids, current_time - START_TIME);
		}
	}
}

/*
 * sig_handler
 * description:
 *     called when SIGINT is passed to the process
 *     sets KILL_STATE to 1 which tells the process to terminate
 *     itself and its children.
 * parameters:
 *     sig: the id of the signal passed to the process
 */
void sig_handler(int sig)
{
	fprintf(OUTPUT_FILE, "Signal Received - ");
	KILL_STATE = 1;
}

/*
 * register_handler
 * description:
 *     registers this program to react to the SIGINT signal
 */
void register_handler(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = 0;
	sa.sa_handler = sig_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1)
		err(1, "sigaction error");
}
