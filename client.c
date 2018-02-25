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

#include "shared.h"
#include "cards.h"

// Exit Statuses
// 1: Wrong arguments
// 2: Wrong password
// 3: Invalid username
// 4: Connect failed
// 5: Unexpected exit

// function declaration
struct in_addr* name_to_IP_addr(char* hostname);
int connect_to(struct in_addr* ipAddress, int port);
void game_loop(int fd);

//Test comment, James is in da house. Memes 2.00 This will work surely
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
    
    fprintf(stdout, "Connected successfully!\n");
    
    // receive start from server
    read(fd, readBuffer, 6);
        
    // tell them yes, we are still here
    write(fd, "yes\n", 4);
    
    char* readBuffer2 = malloc(6 * sizeof(char));
    
    // receive start from server which lets us know everyone is here
    read(fd, readBuffer2, 6); // confirmation
        
    if (strcmp(readBuffer2, "start\n") != 0) {
        // exit
        fprintf(stderr, "Unexpected exit\n");
        
        // Close socket
        close(fd);
        return 5;

    }
    
    free(readBuffer);
    free(readBuffer2);
    free(newBuffer);
    
    // take us to the game!
    fprintf(stdout, "Game started!\n");
    game_loop(fd);

}

// game loop.
void game_loop(int fd) { 

    // here we first receive all the players names in the game from the server.
    fprintf(stdout, "Players Connected:\n");

    for (int i = 0; i < 4; i++) {
        char* readBuffer = read_from_fd(fd, BUFFER_LENGTH);
        fprintf(stdout, "%s\n", readBuffer);
        
        // send yes to server, to tell them we got it
        write(fd, "yes\n", 4);
        
    }
        
    // print out all the cards from the server
    char* message = malloc(BUFFER_LENGTH * sizeof(char));
    read(fd, message, BUFFER_LENGTH);
    fprintf(stdout, "Your hand: %s", message);
    
    fprintf(stdout, "Betting round starting\n");

    // betting round
    while (1) {
        // server will either send bet or info they will print out or betover
        char* result = read_from_fd(fd, BUFFER_LENGTH);

        if (strcmp(result, "bet") == 0) {
            fprintf(stdout, "Your bet!\n");
            
            // send a bet from the player input
            char* buff = malloc(4 * sizeof(char));
            fgets(buff, 4, stdin);
            write(fd, buff, 3);
            
        } else if (strcmp(result, "betover") == 0) {
            // betting round over
            break;
            
        } else if (strcmp(result, "betexit") == 0) {
            // bet ended unexpectedly, exit game
            fprintf(stderr, "Unexpected exit\n");
            close(fd);
            exit(5);
            
        } else {
            // server sent info on betting, print.
            fprintf(stdout, "%s\n", result);
            
        }
        
    }
    
    // result from betting round here
    char* result = read_from_fd(fd, BUFFER_LENGTH);
    fprintf(stdout, "%s\nWaiting for Kitty\n", result);
    
    // find if we won or didn't 
    char* newResult = read_from_fd(fd, 10);
        
    if (strcmp(newResult, "kittywin") == 0) {
        // this player won the kitty bet.
        
        // send that we are ready for more
        write(fd, "yes\n", 4);
        
        // read our current deck from the server 
        char* newerResult = malloc(BUFFER_LENGTH * sizeof(char));
        read(fd, newerResult, BUFFER_LENGTH);
        fprintf(stdout, "%s", newerResult);
        
        // choose which cards to discard
        
        

        
        // server will finally tell us that we are done
        result = read_from_fd(fd, 9);
        
    } 
    
    // kitty is over now
    fprintf(stdout, "Kitty finished\nGame Begins\n");


    // Close socket
    close(fd);
    exit(0);

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
        exit(4);
    }

    // Populate server address structure with IP address and port number of
    // the server
    socketAddr.sin_family = AF_INET;    // IPv4
    socketAddr.sin_port = htons(port);    // convert port num to network byte
    socketAddr.sin_addr.s_addr = inet_addr("0"); // IP address
                                
    // Attempt to connect to the server
    if(connect(fd, (struct sockaddr*)&socketAddr, sizeof(socketAddr)) < 0) {
        fprintf(stderr, "Connect failed\n");
        exit(4);
    }

    // Now have connected socket
    return fd;
}