#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_LENGTH 100
#define MAX_PASS_LENGTH 20
#define MAX_NAME_LENGTH 20

int check_input_digits(int i, char* s);
struct in_addr* name_to_IP_addr(char* hostname);
int connect_to(struct in_addr* ipAddress, int port);
char* read_from_fd(int fd);

// main
int main(int argc, char** argv) {

    // argument checking
    if (argc != 4) {
        fprintf(stderr, "client.exe port password username\n");
        return 1;
    }

    // port, validity is checked when connecting
    int port = atoi(argv[1]); 
           
    // connect to given port
    int fd;
    struct in_addr* ipAddress;
    
    // Convert hostname to IP address
    ipAddress = name_to_IP_addr("0");
    
    // Establish a connection to given port
    fd = connect_to(ipAddress, port);
        
    // prepare first message
    char* message = malloc(MAX_PASS_LENGTH * sizeof(char));
        
    // send the password
    sprintf(message, "%s\n", argv[2]);
    write(fd, message, strlen(message));
    free(message);
    
        
    char* readBuffer = malloc(5 * sizeof(char));

    read(fd, readBuffer, 4); // we expect either yes\n or no\n, so 4 char
    
    if (strcmp(readBuffer, "yes\n") != 0) {
        // exit
        fprintf(stderr, "Auth failed\n");
        
        // Close socket
        close(fd);
        return 2;

    } 
    
    // otherwise it worked
    char* newBuffer = malloc(BUFFER_LENGTH * sizeof(char));
    
    // send player name
    sprintf(newBuffer, "%s\n", argv[3]);
    write(fd, newBuffer, strlen(newBuffer));
    
    read(fd, readBuffer, 4); // confirmation
        
    if (strcmp(readBuffer, "yes\n") != 0) {
        // exit
        fprintf(stderr, "Invalid username\n");
        
        // Close socket
        close(fd);
        return 3;

    }
    
    fprintf(stderr, "Connected successfully!\n");
    
    // wait for go ahead to start, which is another yes
    read(fd, readBuffer, 4); // confirmation
        
    if (strcmp(readBuffer, "yes\n") != 0) {
        // exit
        fprintf(stderr, "Unexpected exit\n");
        
        // Close socket
        close(fd);
        return 4;

    }
    
    free(readBuffer);
    free(newBuffer);
    
    // take us to the game!
    fprintf(stderr, "game started!\n");
    
    
    // Close socket
    close(fd);
    return 0;

}

// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fd) {
    
    char* buffer = malloc(BUFFER_LENGTH * sizeof(char));
    
    char* charBuffer = malloc(sizeof(char));
    
    buffer[0] = '\0';

    int i = 0;
    
    for (i = 0; i < BUFFER_LENGTH - 1; i++) {
        
        read(fd, charBuffer, 1);
        
        if (charBuffer[0] != '\n') {
            buffer[i] = charBuffer[0];
        } else {
            break;
        }
        
    }
    
    free(charBuffer);
    
    buffer[i] = '\0';

    return buffer;
    
}

// returns 0 if the number of digits of i is not the same as
// the length of s
int check_input_digits(int i, char* s) {
    // get digits of i
    int dupI = i;
    if (!dupI) {
        dupI++;
    }
    int length = 0;
    while(dupI) {
        dupI /= 10;
        length++;
    }
    // compare to length of string
    return (length == strlen(s));
    
}

// converts name to IP address
struct in_addr* name_to_IP_addr(char* hostname) {
    int error;
    struct addrinfo* addressInfo;

    error = getaddrinfo(hostname, NULL, NULL, &addressInfo);
    if(error) {
    return NULL;
    }
    // Extract the IP address and return it
    return &(((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr);
}

// connects to the given ip address at the given port
int connect_to(struct in_addr* ipAddress, int port) {
    struct sockaddr_in socketAddr;
    int fd;
    
    // Create socket - TCP socket IPv4
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        fprintf(stderr, "Connect failed\n");
        exit(3);
    }

    // Populate server address structure with IP address and port number of
    // the server
    socketAddr.sin_family = AF_INET;    // IPv4
    socketAddr.sin_port = htons(port);    // convert port num to network byte
    
    socketAddr.sin_addr.s_addr = inet_addr("0");//ipAddress->s_addr; // IP address - already in
                                // network byte order
                                
    // Attempt to connect to the server
    if(connect(fd, (struct sockaddr*)&socketAddr, sizeof(socketAddr)) < 0) {
        fprintf(stderr, "Connect failed\n");
        exit(3);
    }

    // Now have connected socket
    return fd;
}