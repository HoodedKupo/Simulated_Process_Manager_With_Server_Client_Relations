#Establish the makefile variables
CC = gcc-10
CFLAGS = -Wall -g -fsanitize=leak -fsanitize=address
#CFLAGS = -Wall -g -fsanitize=thread

#makes all the executables
all: macD macD_c

#creates the macD executable
macD: macD.c
	$(CC) $(CFLAGS) $^ -o $@
	chmod -cf 777 ./$@

#creates the macD_c executable
macD_c: macD_c.c
	$(CC) $(CFLAGS) $^ -o $@

#removes all executable files
clean:
	rm macD
	rm macD_c
