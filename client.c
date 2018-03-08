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
void send_input(int fd);

//Test comment, James is in da house. Memes 2.00 This will work surely
// main
int main(int argc, char** argv) {

    // argument checking
    if (argc != 5) {
        fprintf(stderr, "client.exe ipaddr port password username\n");
        return 1;
        
    }

    // port, validity is checked when connecting
    int port = atoi(argv[2]); 
           
    // connect to given port
    int fd;
    struct in_addr* ipAddress;
    
    // Convert hostname to IP address
    ipAddress = name_to_IP_addr(argv[1]);
    
    // Establish a connection to given port
    fd = connect_to(ipAddress, port);
    
    // send the password
    {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", argv[3]);
        write(fd, message, strlen(message));
    
    }
        
    // read yes from server, otherwise exit
    if (strcmp(read_from_fd(fd, BUFFER_LENGTH), "yes") != 0) {
        // exit
        fprintf(stderr, "Auth failed\n");
        close(fd);
        return 2;

    } 
    
    // send player name
    {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", argv[4]);
        write(fd, message, strlen(message));
        
    }
    
    // check if server sent yes, otherwise invalid username
    if (strcmp(read_from_fd(fd, BUFFER_LENGTH), "yes") != 0) {
        // exit
        fprintf(stderr, "Invalid username\n");
        
        // Close socket
        close(fd);
        return 3;

    }
    
    fprintf(stdout, "Connected successfully! Waiting for others\n");
    
    // receive start from server
    read_from_fd(fd, BUFFER_LENGTH);
    
    // tell them yes, we are still here
    write(fd, "yes\n", 4);
        
    // receive start from server which lets us know everyone is here
    // server will either send start\n or something else
        
    if (strcmp(read_from_fd(fd, BUFFER_LENGTH), "start") != 0) {
        // exit
        fprintf(stderr, "Unexpected exit\n");
        
        // Close socket
        close(fd);
        return 5;

    }
    
    // take us to the game!
    fprintf(stdout, "Game started!\n");
    game_loop(fd);

}

// game loop.
void game_loop(int fd) {

    // here we first receive all the players names in the game from the server.
    fprintf(stdout, "Players Connected:\n");
    
    for (int i = 0; i < 4; i++) {
        // print the players details from the server
        fprintf(stdout, "%s\n", read_from_fd(fd, BUFFER_LENGTH));
        
        // send yes to server, to tell them we got it
        write(fd, "yes\n", 4);
        
    }
        
    // print out all the cards from the server
    fprintf(stdout, "Your hand: %s\n", read_from_fd(fd, BUFFER_LENGTH));
    
    fprintf(stdout, "Betting round starting\n");

    // betting round
    while (1) {
        // server will either send bet or info they will print out or betover
        char* result = read_from_fd(fd, BUFFER_LENGTH);

        if (strcmp(result, "bet") == 0) {
            fprintf(stdout, "Your bet!\n");
            // send a bet from the player input
            send_input(fd);
            
        } else if (strcmp(result, "betover") == 0) {
            // betting round over
            break;
            
        } else if (strcmp(result, "badinput") == 0) {
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
    fprintf(stdout, "%s\n", read_from_fd(fd, BUFFER_LENGTH));
    
    fprintf(stdout, "Waiting for Kitty\n");
    
    // kitty
    while (1) {
        char* result = read_from_fd(fd, BUFFER_LENGTH); // kitty info or not 
        
        if (strcmp(result, "send") == 0) {
            // send input from user
            send_input(fd);                            
            
        } else if (strcmp(result, "kittyend") == 0) {
            // exit from the loop
            break;
            
        } else if (strcmp("badinput", result) == 0) {
            // game ended unexpectedly
            fprintf(stderr, "Unexpected exit\n");
            close(fd);
            exit(5);
            
        } else {
            // server sent the joker suite here, we will get jokerdone next
            fprintf(stdout, "%s\n", result);
            
        }
            
    }
        
    // kitty is over now
    fprintf(stdout, "Kitty finished\n");

    // we need to choose the suite for the joker if we are doing no trumps
    // very similar to how kitty round is handled
    fprintf(stdout, "Choosing joker suite\n");
    
    // joker suite
    while (1) {
        char* result = read_from_fd(fd, BUFFER_LENGTH); // jokerwant or not 
        
        if (strcmp(result, "jokerwant") == 0) {
            // print to user that we want the joker
            fprintf(stdout, "Choose joker suite:\n");
            send_input(fd);
            
        } else if (strcmp(result, "jokerdone") == 0) {
            // exit from the loop
            break;
            
        } else if (strcmp("badinput", result) == 0) {
            // game ended unexpectedly
            fprintf(stderr, "Unexpected exit\n");
            close(fd);
            exit(5);
            
        } else {
            // server sent the joker suite here, we will get jokerdone next
            fprintf(stdout, "%s\n", result);
            
        }
            
    }
    
    // game begins
    fprintf(stdout, "Game Begins\n");

    // actual game loop
    while (1) {
        
        // get hand from server
        fprintf(stdout, "Your hand: %s\n", read_from_fd(fd, BUFFER_LENGTH));
        
        // get round number from server
        fprintf(stdout, "%s\n", read_from_fd(fd, BUFFER_LENGTH));
        
        while (1) {
            char* result = read_from_fd(fd, BUFFER_LENGTH);
            
            if (strcmp("send", result) == 0) {
                // send card to server
                fprintf(stdout, "Choose card to play\n");
                send_input(fd);

            } else if (strcmp("roundover", result) == 0) {
                // round over
                break;
             
            } else if (strcmp("badinput", result) == 0) {
                // game ended unexpectedly
                fprintf(stderr, "Unexpected exit\n");
                close(fd);
                exit(5);
            
            } else {
                fprintf(stdout, "%s\n", result);
                
            }
            
        }
        
        // get number of tricks betting team has won
        fprintf(stdout, "%s\n", read_from_fd(fd, BUFFER_LENGTH));
        
    }
    
    // get final result of the game
    fprintf(stdout, "%s\n", read_from_fd(fd, BUFFER_LENGTH));
    
    // Close socket
    close(fd);
    exit(0);

}

// sends input from stdin to the fd
void send_input(int fd) {
    char* buff = malloc(BUFFER_LENGTH * sizeof(char));
    fgets(buff, BUFFER_LENGTH, stdin);
    write(fd, buff, strlen(buff));        
    free(buff);
    
}
                
// converts name to IP address
struct in_addr* name_to_IP_addr(char* hostname) {
    int error;
    struct addrinfo* addressInfo;
    error = getaddrinfo(hostname, NULL, NULL, &addressInfo);
    
    if (error) {
        return NULL;
    
    }
    
    // extract the IP address and return it
    return &(((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr);
    
}

// connects to the given ip address at the given port
int connect_to(struct in_addr* ipAddress, int port) {
    struct sockaddr_in socketAddr;
    int fd;
    
    // create socket - TCP socket IPv4
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "Connect failed\n");
        exit(4);
    }

    // populate server address structure with IP address and port number
    socketAddr.sin_family = AF_INET;    // IPv4
    socketAddr.sin_port = htons(port);    // convert port num to network byte
    socketAddr.sin_addr.s_addr = ipAddress->s_addr; // IP address  
                                
    // attempt to connect to the server
    if (connect(fd, (struct sockaddr*)&socketAddr, sizeof(socketAddr)) < 0) {
        perror("Connect failed\n");
        exit(4);
        
    }

    // now have connected socket
    return fd;
}