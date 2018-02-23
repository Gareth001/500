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
#include <time.h>

#include "shared.h"
#include "cards.h"

#define BUFFER_LENGTH 100
#define MAX_PASS_LENGTH 20
#define MAX_NAME_LENGTH 20

// ERRORS
// 1: Wrong arguments
// 2: Password too long
// 3: Invalid port

// stores player info
typedef struct Player {
    
    Card* deck;
    char* name;
    int fd;
    bool hasPassed;
    
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
int open_listen(int port, int* listenPort);
void process_connections(int fdServer, GameInfo* games);
int send_to_all(char* message, GameInfo* gameInfo);
int check_valid_username(char* user, GameInfo* gameInfo);
int read_from_all(char* message, GameInfo* gameInfo);


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
    fprintf(stdout, "Listening on %d\n", listenPort);
    
    // create game info struct and populate
    GameInfo gameInfo;
    gameInfo.fdServer = fdServer;
    gameInfo.password = password;
    gameInfo.players = 0;
    gameInfo.player = malloc(4 * sizeof(Player));
    
    // process connections
    process_connections(fdServer, &gameInfo);

}

// main game loop
void game_loop(GameInfo* gameInfo) {
    fprintf(stdout, "Game starting\n");
    
    // send all players the player details.
    char* message = malloc(MAX_NAME_LENGTH * sizeof(char));
    for (int i = 0; i < 4; i++) {
        sprintf(message, "%s\n", gameInfo->player[i].name);
        send_to_all(message, gameInfo);
        
        // now read from all to ensure correct order is read by client
        read_from_all("yes\n", gameInfo); // do something with the return?

    }
    
    free(message);
    
    fprintf(stdout, "Shuffling\n");
    srand(time(NULL)); // random seed
    Card* deck = create_deck();
    Card* kitty = malloc(3 * sizeof(Card));
    
    fprintf(stdout, "Dealing\n");
    
    // malloc all players decks
    for (int i = 0; i < 4; i++) {
        gameInfo->player[i].deck = malloc(10 * sizeof(Card));
        gameInfo->player[i].hasPassed = false; // set this property too
        
    }
    
    int j = 0; // kitty counter
        
    // send cards to users
    for(int i = 0; i < 43; i++) {
        // give to kitty
        if (i == 12 || i == 28 || i == 42) {
            kitty[j++] = deck[i];
            continue;
        }
        
        // record which player we send to
        int k;
        
        // send card value with newline 
        char* c = malloc(3 * sizeof(char));
        sprintf(c, "%s\n", return_card(&deck[i]));
        
        if (i < 12) {
            // record the card in the players deck
            gameInfo->player[k = (i % 4)].deck[i / 4] = deck[i];

            // send ith card to Player k
            write(gameInfo->player[k].fd, c, 3);
            
            
        } else if (i < 28) {
            // record the card in the players deck
            gameInfo->player[k = ((i - 1) % 4)].deck[(i - 1) / 4] = deck[i];

            // send ith card to Player k
            write(gameInfo->player[k].fd, c, 3);
            
        } else {
            // record the card in the players deck
            gameInfo->player[k = ((i - 2) % 4)].deck[(i - 2) / 4] = deck[i];

            // send ith card to Player k
            write(gameInfo->player[k].fd, c, 3);
            
        }
                
        // wait for yes from player k
        char* buffer = malloc(4 * sizeof(char));
        read(gameInfo->player[k].fd, buffer, 4);
        if (strcmp(buffer, "yes\n") != 0) {
            exit(1);
            
        }
        
    }
    
    // debugging, print deck
    fprintf(stdout, "Deck: ");
    for (int i = 0; i < 43; i++) {
        fprintf(stdout, "%s ", return_card(&deck[i]));
    }
    fprintf(stdout, "\n");

    // print each players deck
    for (int i = 0; i < 4; i++) {
        fprintf(stdout, "Player %d: ", i);
        
        for (int j = 0; j < 10; j++) {
            fprintf(stdout, "%s ", return_card(&gameInfo->player[i].deck[j]));
            
        }
        fprintf(stdout, "\n");
        
    }
    
    // print kitty for testing
    fprintf(stdout, "kitty: ");
    for (int i = 0; i < 3; i++) {
        fprintf(stdout, "%s ", return_card(&kitty[i]));
        
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "Betting round starting\n");

    // ignore misere case for now
    int highestBet = 0;
    Trump suite = 0;
    
    int p = 0; // player counter
    int pPassed = 0; // players passed counter
    
    // loop until winner
    while (pPassed != 4) {
            
        while (gameInfo->player[p].hasPassed == false) {
            
            // begin with player 0
            write(gameInfo->player[p].fd, "bet\n", 4);
            
            // get bet from player 0
            char* msg = malloc(3 * sizeof(char));
            read(gameInfo->player[p].fd, msg, 3);
            
            char* send = malloc(BUFFER_LENGTH * sizeof(char));

            // check if it's a pass
            if (strcmp(msg, "PA\n") == 0) {
                // set passed to true, change send msg
                gameInfo->player[p].hasPassed = true;
                sprintf(send, "Player %d passed\n", p);
                pPassed++;
                
            } else {
                // ensure length of message is correct
                if (strlen(msg) != 3) {
                    continue;
                }
                
                int newBet = 0;
                Trump newSuite = 0;
                
                // read value, with special case '0' = 10
                newBet = (msg[0] == '0') ? 10 : msg[0] - 48;
                                    
                // make sure value is valid
                if (newBet < 6 || newBet > 10) {
                    continue;
                }
                
                newSuite = return_trump(msg[1]);
                
                // check for default case
                if (newSuite == -1) {
                    continue;
                }

                // so the input is valid, now check if the bet is higher than the last one.
                // TBA
                
                highestBet = newBet;
                suite = newSuite;
                
                // string to send to all players
                sprintf(send, "Player %d bet %d%c\n", p, highestBet,
                        return_trump_char(suite)); 
                
            }
            
            fprintf(stdout, "%s", send);
            send_to_all(send, gameInfo);
            break;
            
        }
        
        
        p++;
        p %= 4;
        
    }

    // send bet over to all players. Betting round finished
    send_to_all("betover\n", gameInfo);

    // get actual winning player
    p += 3;
    p %= 4;
    
    // send winning bet to all players
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "Player %d won the bet with %d%c!\n", p, highestBet, 
            return_trump_char(suite));
    fprintf(stdout, "%s", msg);
    send_to_all(msg, gameInfo);

    
    // game is over. exit
    exit(0);
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
        char* passBuffer = read_from_fd(fd, MAX_PASS_LENGTH);
            
        if (strcmp(passBuffer, gameInfo->password) == 0) {
            // password was valid
            free(passBuffer);

            // send yes to client for password
            write(fd, "yes\n", 4);
            
            // get user name from client
            char* userBuffer = read_from_fd(fd, MAX_NAME_LENGTH);
            
            // check username is valid, i.e. not taken before            
            if (check_valid_username(userBuffer, gameInfo)) {
                // send no to client for user name, and forget this connection
                write(fd, "no\n", 3);
                continue;
            }
            
            // send yes to client for user name
            write(fd, "yes\n", 4);

            // server message
            fprintf(stdout, "Player %d connected with user name '%s'\n",
                gameInfo->players, userBuffer);
            
            // add name
            gameInfo->player[gameInfo->players].name = strdup(userBuffer);
            gameInfo->player[gameInfo->players].fd = fd;
            gameInfo->players++;
            
            // check if we are able to start the game, or have 4 players
            if (gameInfo->players == 4) {
                // send yes to all players
                send_to_all("start\n", gameInfo);
                                
                // game starting
                game_loop(gameInfo);
                
            }

        } else {
            // otherwise we got illegal input. Send no.
            write(fd, "no\n", 3);
            close(fd);

        }

        free(passBuffer);
    
    }
    
}

// returns 0 if the username has not been taken before, 1 otherwise
int check_valid_username(char* user, GameInfo* gameInfo) {
    for (int i = 0; i < gameInfo->players; i++) {        
        if (strcmp(user, gameInfo->player[i].name) == 0) {
            return 1;
            
        }
        
    }
    
    return 0;
    
}

// sends a message to all players
int send_to_all(char* message, GameInfo* gameInfo) {
    for (int i = 0; i < 4; i++) {
        write(gameInfo->player[i].fd, message, strlen(message));
    }
    
    return 0;
    
}

// reads a message from all players, returns 0 if successful, 1 otherwise
int read_from_all(char* message, GameInfo* gameInfo) {
    char* msg = malloc(strlen(message) * sizeof(char));
    for (int i = 0; i < strlen(message); i++) {
        read(gameInfo->player[i].fd, msg, strlen(message));
        
        if (strcmp(msg, message) != 0) {
            return 1;
        }
        
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
