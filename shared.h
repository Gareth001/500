#ifndef SHARED_H
#define SHARED_H

// dependencies
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// cross platform sockets
#ifdef _WIN32
    #include <winsock2.h>
    #include <Ws2tcpip.h>
    #include <Windows.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
#endif
 
// definitions
#define BUFFER_LENGTH 100
#define MAX_PASS_LENGTH 20
#define MAX_NAME_LENGTH 20

// reads from the fileDes until the newline character is found,
// and then writes this to the buffer
char* read_from_fd(int fileDes, char* buffer, int length, bool newline);

// returns 0 if the number of digits of i is not the same as
// the length of s
int check_input_digits(int i, char* s);

// initialise socket for cross platform
int socket_init(void);

// write multiplatform, return negative if error
int write_new(int fileDes, char* buffer, int length);

// given buffer, returns number of characters until
// and including \n
int string_length(char* buffer);

#endif /* SHARED_H */
