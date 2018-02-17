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
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFFER_LENGTH 100
#define MAX_PASS_LENGTH 20
#define MAX_NAME_LENGTH 20

// ERRORS
// 1: Wrong arguments
// 2: Password too long
// 3: Invalid port

// stores player info
typedef struct Player {
	
	char* playerName;
	
	int fd;

} Player;


// stores game info
typedef struct GameInfo {
	
	Player* player;
	
	int players;
	
	// permanent stats
    char* password;
    unsigned int timeout;
    int fdServer; // for passing to threads

} GameInfo;

// function declaration
int check_input_digits(int i, char* s);
int open_listen(int port, int* listenPort);
void process_connections(int fdServer, GameInfo* games);
char* readFromFD(int fd, int length);
int send_to_all(char* message, GameInfo* gameInfo);


int main(int argc, char** argv) {

	// check arg count
	if (argc != 3) {
        fprintf(stderr, "server.exe password port");
        return 1;
    }

	char* password = argv[1]; // read password
	
	if (strlen(password) > MAX_PASS_LENGTH) {
		fprintf(stderr, "password too long");
		return 2;
	}
	
    int port = atoi(argv[2]); // read port

    // check valid port. 
    if (port < 1 || port > 65535 || (port == 0 && strcmp(argv[2], "0") != 0) ||
            !check_input_digits(port, argv[2])) {
        fprintf(stderr, "Bad port\n");
        return 3;
    }

	// our fd server
    int fdServer;
	int listenPort = 0;
	
    // attempt to listen (exits here if fail) and print
    fdServer = open_listen(port, &listenPort);
	fprintf(stdout, "%d\n", listenPort);
	
	// create game info struct and populate
	GameInfo gameInfo;
	gameInfo.fdServer = fdServer;
	gameInfo.password = password;
	gameInfo.players = 0;
	gameInfo.player = malloc(4 * sizeof(Player));
	
	// process connections
	process_connections(fdServer, &gameInfo);

}

// process connections (from the lecture with my additions)
void process_connections(int fdServer, GameInfo* gameInfo) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    while(1) {

	    fromAddrSize = sizeof(struct sockaddr_in);
        // block, waiting for a new connection (fromAddr will be populated
        // with address of the client. Accept        
				
        fd = accept(fdServer, (struct sockaddr*)&fromAddr,  &fromAddrSize);

		// check password
		char* passBuffer = readFromFD(fd, MAX_PASS_LENGTH);
			
		if (strcmp(passBuffer, gameInfo->password) == 0) {
			// password was valid
			free(passBuffer);

			// send yes to client for password
            write(fd, "yes\n", 4);
			
			// get user name from client
			char* userBuffer = readFromFD(fd, MAX_NAME_LENGTH);
			
			// check username is valid, i.e. not taken before
			/////////////////////////////////////////////////////////////////

			// send yes to client for user name
            write(fd, "yes\n", 4);

			// server message
			fprintf(stdout, "Player %d connected with user name '%s'\n",
				gameInfo->players, userBuffer);
			
			// add name
			gameInfo->player[gameInfo->players].playerName = userBuffer;
			gameInfo->player[gameInfo->players].fd = fd;
			gameInfo->players++;
			
			// check if we are able to start the game
			if (gameInfo->players == 4) {
				// send yes to all players, game starting
				send_to_all("yes\n", gameInfo);

				
			}

			
		} else {
            // otherwise we got illegal input. Send no.
            write(fd, "no\n", 3);
            close(fd);

        }

		free(passBuffer);
	
	}
	
	
}

// sends a message to all players
int send_to_all(char* message, GameInfo* gameInfo) {
	for (int i = 0; i < 4; i++) {
		write(gameInfo->player[i].fd, message, strlen(message));
	}
	
	return 0;
	
}

// opens the port for listening, exits with error if didn't work also writes
// the listenPort
int open_listen(int port, int* listenPort) {
    int fd;
    struct sockaddr_in serverAddr;
    int optVal;
    int error = 0;
    
    // Create socket - IPv4 TCP socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        error = 1;
    }

    // Allow address (port number) to be reused immediately
    optVal = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        error = 1;
    }

    // Populate server address structure to indicate the local address(es)
    serverAddr.sin_family = AF_INET; // IPv4
    
    if (port == '-') {
        port = 0;
    }
        
    serverAddr.sin_port = htons(port); // port number - in network byte order
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // any IP address of this host

    // Bind our socket to this address (IP addresses + port number)
    if(bind(fd, (struct sockaddr*)&serverAddr,
            sizeof(struct sockaddr_in)) < 0) {
        error = 1;
    }
    
    socklen_t sa_len = sizeof(struct sockaddr_in);
    getsockname(fd, (struct sockaddr*)&serverAddr, &sa_len);
    *listenPort = (unsigned int) ntohs(serverAddr.sin_port);

    // Indicate our willingness to accept connections.
    // Queue length is SOMAXCONN
    // (128 by default) - max length of queue of connections
    if(listen(fd, SOMAXCONN) < 0) {
        error = 1;
    }
    
    if (error == 1) {
        fprintf(stderr, "Failed listen\n");
        exit(6);
    }
    
    return fd;
}

// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* readFromFD(int fd, int length) {
    
    char* buffer = malloc(length * sizeof(char));
    
    char* charBuffer = malloc(sizeof(char));
    
    buffer[0] = '\0';

    int i = 0;
    
    // read single char until buffer length. 
    for (i = 0; i < length - 1; i++) {
        
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
