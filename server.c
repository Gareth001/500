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

// Exit Statuses
// 1: Wrong arguments
// 2: Password too long
// 3: Invalid port
// 4: Failed listen
// 5: Unexpected exit

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
int send_to_all_except(char* message, GameInfo* gameInfo, int except);
bool send_deck_to_all(int cards, GameInfo* gameInfo);

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
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            char* message = malloc(BUFFER_LENGTH * sizeof(char));

            // string that tells the user if the player is them or a teammate
            char* str;
            if (i == j) {
                str = " (You)";
                
            } else if ((i + 2) % 4 == j) {
                str = " (Teammate)";
                
            } else {
                str = "";
                
            }
                   
            // send this users info to the player
            sprintf(message, "Player %d, '%s'%s\n", j,
                    gameInfo->player[j].name, str);
            write(gameInfo->player[i].fd, message, strlen(message));
            
            // await yes from user (maybe check?)
            read_from_fd(gameInfo->player[i].fd, BUFFER_LENGTH);
            
        }
        
    }
    
    fprintf(stdout, "Shuffling\n");
    srand(time(NULL)); // random seed
    Card* deck = create_deck();
    Card* kitty = malloc(3 * sizeof(Card));
    
    fprintf(stdout, "Dealing\n");
    
    // malloc all players decks
    for (int i = 0; i < 4; i++) {
        gameInfo->player[i].deck = malloc(13 * sizeof(Card)); // 13 for kitty
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

        // give the card to the players deck
        if (i < 12) {
            // record the card in the players deck
            gameInfo->player[i % 4].deck[i / 4] = deck[i];            
            
        } else if (i < 28) {
            // record the card in the players deck
            gameInfo->player[(i - 1) % 4].deck[(i - 1) / 4] = deck[i];
            
        } else {
            // record the card in the players deck
            gameInfo->player[(i - 2) % 4].deck[(i - 2) / 4] = deck[i];
            
        }    
    
    }
    
    // sort each deck by suite here!
    
    
    // send each player their deck
    send_deck_to_all(10, gameInfo);

    // debugging, print deck
    fprintf(stdout, "Deck: %s\n", return_hand(deck, 43));
    
    // print each players hand
    for (int i = 0; i < 4; i++) {
        fprintf(stdout, "Player %d: %s\n", i,
                return_hand(gameInfo->player[i].deck, 10)); 
        
        
    }
    
    // print kitty 
    fprintf(stdout, "kitty: %s\n", return_hand(kitty, 3));
    
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
            char* msg = malloc(BUFFER_LENGTH * sizeof(char));
            if (read(gameInfo->player[p].fd, msg, BUFFER_LENGTH) == -1) {
                // player left early
                fprintf(stderr, "Unexpected exit\n");
                send_to_all_except("betexit\n", gameInfo, p);
                exit(5);
                
            }
            
            char* send = malloc(BUFFER_LENGTH * sizeof(char));

            // check if it's a pass
            if (strcmp(msg, "PA\n") == 0) {
                // set passed to true, change send msg
                gameInfo->player[p].hasPassed = true;
                sprintf(send, "Player %d passed\n", p);
                pPassed++;
                
            } else {
                
                // check if the bet was valid
                if (valid_bet(&highestBet, &suite, msg) == false) {
                    continue;
                    
                }
                                 
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
    
    if (highestBet == 0) {
        // case where everyone passed, we want to redeal 
        // TBA
        
    }
    
    // send winning bet to all players
    {
        char* msg = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(msg, "Player %d won the bet with %d%c!\n", p, highestBet, 
                return_trump_char(suite));
        fprintf(stdout, "%s", msg);
        send_to_all(msg, gameInfo);
    }
    
    fprintf(stdout, "Dealing kitty\n");
    
    // send the winner of the betting round the 13 cards they have to
    // choose from, the player will send the three they don't want
    
    // send message to winner that they are to receive kitty data
    write(gameInfo->player[p].fd, "kittywin\n", 9);
    
    // make sure player is ready for input. should be yes\n
    read_from_fd(gameInfo->player[p].fd, BUFFER_LENGTH);
  
    // add kitty to the winners hand
    for (int i = 0; i < 3; i++) {
        gameInfo->player[p].deck[10 + i] = kitty[i];
        
    }
  
    // prepare string to send to the winner
    {
        char* msg = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(msg, "You won! Pick 3 cards to discard: %s\n",
                return_hand(gameInfo->player[p].deck, 13));
        write(gameInfo->player[p].fd, msg, strlen(msg));
    }
    
    // receive 3 cards the user wants to discard
    int c = 0;
    while (c != 3) {

        // send user a string of how many cards we still need
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Pick %d more cards!\n", 3 - c);
        
        write(gameInfo->player[p].fd, message, strlen(message)); 
        
        // get card from this player
        char* msg = malloc(BUFFER_LENGTH * sizeof(char));
        if (read(gameInfo->player[p].fd, msg, BUFFER_LENGTH) == -1) {
            // player left early
            fprintf(stderr, "Unexpected exit\n");
            send_to_all_except("kittybad\n", gameInfo, p);
            exit(5);
            
        }
        
        // check if card is valid and then remove it from the players deck
        
        // move this to a function!
        // ensure the card is valid
        Card card = return_card_from_string(msg);
        
        // check if the card was valid
        if (card.value == 0) {
            // don't add to c
            
        } else if (remove_card_from_deck(card, &gameInfo->player[p].deck,
                13 - c) == false) {
            
            // try and remove the card from the deck, 
            
        } else {
            // otherwise it was a success, we need 1 less card now
            c++;
            
        }

        
    }
    
    // send kitty done to all
    send_to_all("kittyend\n", gameInfo);
    
    // kitty is over now
    fprintf(stdout, "Kitty finished\nGame Begins\n");
    
    // send each player their deck now that it's started
    send_deck_to_all(10, gameInfo);
    

    
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
                // send start to all players
                send_to_all("start\n", gameInfo);
                
                // we want a yes from all players to ensure they are still here
                for (int i = 0; i < 4; i++) {
                    
                    char* buf = malloc(4 * sizeof(char));
                    if (read(gameInfo->player[i].fd, buf, 4) == -1) {
                        
                        // if we don't get a yes, then send a message that game 
                        // is not starting and we exit.
                        fprintf(stderr, "Unexpected exit\n");
                        send_to_all_except("nostr\n", gameInfo, i);
                        exit(5);
                        
                    }
                    
                }

                // all players are here, start the game
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
    return send_to_all_except(message, gameInfo, -1);
    
}

// sends a message to all players, except the given one
// this exists so the server doesn't send any messages on closing 
// to fd's that have been closed after a failed read
int send_to_all_except(char* message, GameInfo* gameInfo, int except) {
    for (int i = 0; i < 4; i++) {
        if (i != except) {
            write(gameInfo->player[i].fd, message, strlen(message));
            
        }
    }
    
    return 0;
    
}

// send each player their deck
bool send_deck_to_all(int cards, GameInfo* gameInfo) {
    // send each player their deck
    for (int i = 0; i < 4; i++) {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", return_hand(gameInfo->player[i].deck, cards));
        write(gameInfo->player[i].fd, message, strlen(message));
        
    }
    
    return true;
    
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
        exit(4);
    }
    
    return fd;
}
