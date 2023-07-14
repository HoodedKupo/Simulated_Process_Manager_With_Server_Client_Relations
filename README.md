# Simulated_Process_Manager_With_Server_Client_Relations
creates and monitors processes as indicated by the user input across the server socket in the linux enviorment.
## Files
The macD.c file is the main file that contains the logic for the program.\
macD.h is a header file used by macD.c, it contains the functions relating to process monitoring\
macD\_server.h is a header file used by macD.c, it contains functions relating to server mannagement.\
macD\_c.c is the client side code.\
macD\_c.h is a header file used by macD\_c.c, it contains the functions related to mannaging the client.\
makefile is a file used to compile the program, see "How To Use"\
README is a file that contains useful information about the software.
## Description
the macD executable currently checks for the -i flag followed by a file path.\
the program will then open the file, if the path provided is a valid file,\
and create a new process for each line in the file.\
if the -o flag is passed followed by a file, the program will send all periodic reports to\
the specified file instead of stdout.\
if the -q flag is used then the program will silence the output of all child processes.\
macD will then monitor these processes across their life time and report\
if they exit or are terminated.\
at the end of the session, either by timeout, all processes exiting, or recieving a kill signal\
macD will display the time it was active for.\
\
running macD\_c will connect a client to macD.\
from here the client program can type four letter commands to be sent\
to macD.\
if the command is "STAT" then the server, being run from the macD program, will send the\
number of active processes to the client which will then be displayed.\
if the command "KIll" is used then macD expected an integer to follow. It will then attempt\
to terminate the process at the specified index. If that process is already terminated or the\
index is out of range then the client receives the message FAIL, otherwise the process is terminated\
and the client receives a SUCC message.\
\
the client connects to macd.socket.client\
the server connects to macd.socket.server
## How To Use
First type the command "make" in order to compile the executable.\
Next call the function with the command "./macD -i <filepath> [optional]-o <outputFile> [optional]-q".\
the program will create the processes specified in the filepath.\
the program will then send the periodic reports to outputFile. \
and if -q is used the program will silence the child processes.\
Use the command "./macD_c" to connect a client.\
in the client window type a four letter command and click enter.\
if this command is "KILL" you next command should be the index of the process to terminate.\
the client will then display either "Echo from server: FAIL" or "Echo from server: SUCC"\
if this command is "STAT" the client will receive the message "there are N running processes".\
use ctrl+c to exit the client or terminating the server will also result in client disconneciton.\
Once you're done run "make clean" to remove the executable files.\
to terminate the program while it is running click ctrl+C.
## Quirks
the client reads in a buffer of four bytes to send to the server.\
This means if you type a five letter word, like "hello", then stat the program will not run stat.\
this is because the program reads in the first four bytes "hell" then it reads the next four, "osta".\
if you accidentally type a longer/shorter word you can reset the buffer by\
typing a letter, enter, then typing stat. Repeat this until the stat command works\ 
(you will only need to do this at most 3 times to reset the buffer).
