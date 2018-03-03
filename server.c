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

#define NUM_ROUNDS 10
#define NUM_PLAYERS 4

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
    int wins;
    
} Player;

// stores game info
typedef struct GameInfo {
    
    // player and card array
    Player* player;
    Card* kitty;
    
    // highest bet info
    int highestBet;
    Trump suite;
    int betWinner;
    
    int players; // current player count
    int p; // current player selected
    
    // permanent stats
    char* password;
    unsigned int timeout;
    int fd; 

} GameInfo;

// function declaration
int open_listen(int port);
void process_connections(GameInfo* games);
int send_to_all(char* message, GameInfo* gameInfo);
int check_valid_username(char* user, GameInfo* gameInfo);
int read_from_all(char* message, GameInfo* gameInfo);
int send_to_all_except(char* message, GameInfo* gameInfo, int except);
bool send_deck_to_all(int cards, GameInfo* gameInfo);
Card* deal_cards(GameInfo* gameInfo);
int get_winning_tricks(GameInfo* gameInfo);
char* get_card(GameInfo* gameInfo);
void kitty_round(GameInfo* gameInfo);
void bet_round(GameInfo* gameInfo);
void send_player_details(GameInfo* gameInfo);
void play_round(GameInfo* gameInfo);

int main(int argc, char** argv) {

    // check arg count
    if (argc != 3) {
        fprintf(stderr, "server.exe port password");
        return 1;
    }

    // check valid password (correct length)
    if (strlen(argv[2]) > MAX_PASS_LENGTH) {
        fprintf(stderr, "password too long");
        return 2;
    }
    
    int port = atoi(argv[1]); // read port

    // check valid port. 
    if (port < 1 || port > 65535 || (port == 0 && strcmp(argv[1], "0") != 0) ||
            !check_input_digits(port, argv[1])) {
        fprintf(stderr, "Bad port\n");
        return 3;
    }
    
    // attempt to listen (exits here if fail) and save the fd if successful
    int fdServer = open_listen(port);
    
    // create game info struct and populate
    GameInfo gameInfo;
    gameInfo.fd = fdServer;
    gameInfo.password = argv[2];
    gameInfo.players = 0;
    gameInfo.player = malloc(NUM_PLAYERS * sizeof(Player));
    
    // process connections
    process_connections(&gameInfo);

}

// main game loop
void game_loop(GameInfo* gameInfo) {
    fprintf(stdout, "Game starting\n");
    
    send_player_details(gameInfo);
        
    // shuffle and deal
    fprintf(stdout, "Shuffling and Dealing\n");
    srand(time(NULL)); // random seed
    gameInfo->kitty = deal_cards(gameInfo); // kitty and dealing, debug info too

    // betting round
    fprintf(stdout, "Betting round starting\n");
    bet_round(gameInfo);
    fprintf(stdout, "Betting round ending\n");

    // kitty round
    fprintf(stdout, "Dealing kitty\n");
    kitty_round(gameInfo);
    fprintf(stdout, "Kitty finished\n");
    
    // play round
    fprintf(stdout, "Game Begins\n");
    play_round(gameInfo);
    
    // tell the users if the betting team has won or not
    // number of tricks the betting team has won
    char* result = malloc(BUFFER_LENGTH * sizeof(char));
    
    if (gameInfo->highestBet > get_winning_tricks(gameInfo)) {
        result = "Betting team lost!\n";
        
    } else {
        result = "Betting team won!\n";
        
    }
    
    send_to_all(result, gameInfo);
    fprintf(stdout, "%s", result);
    
    // game is over. exit
    exit(0);
}

// process connections (from the lecture with my additions)
void process_connections(GameInfo* gameInfo) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // loop until we have 4 players waiting to start a game
    while (gameInfo->players != NUM_PLAYERS) {

        fromAddrSize = sizeof(struct sockaddr_in);
        // block, waiting for a new connection (fromAddr will be populated
        // with address of the client. Accept        
                
        fd = accept(gameInfo->fd, (struct sockaddr*)&fromAddr,  &fromAddrSize);

        // check password is the same as ours
        if (strcmp(read_from_fd(fd, MAX_PASS_LENGTH),
                gameInfo->password) != 0) {
        
            // otherwise we got illegal input. Send no.
            write(fd, "no\n", 3);
            close(fd);
            continue;
        
        }
                    
        // password was valid, send yes to client for password
        write(fd, "yes\n", 4);
        
        // get user name from client
        char* userBuffer = read_from_fd(fd, MAX_NAME_LENGTH);
        
        // check username is valid, i.e. not taken before            
        if (check_valid_username(userBuffer, gameInfo)) {
            // send no to client for user name, and forget this connection
            write(fd, "no\n", 3);
            close(fd);
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
            
    }
    
    // we now have 4 players. send start to all
    send_to_all("start\n", gameInfo);

    // we want a yes from all players to ensure they are still here
    for (int i = 0; i < NUM_PLAYERS; i++) {
        
        // read 4 characters, just make sure that it works
        if (read(gameInfo->player[i].fd, malloc(4 * sizeof(char)),
                4) == -1) {
            
            // if we don't get a yes, then send a message that game 
            // is not starting and we exit
            fprintf(stderr, "Unexpected exit\n");
            send_to_all_except("nostart\n", gameInfo, i);
            exit(5);
            
        }
        
    }

    // all players are here, start the game!
    send_to_all("start\n", gameInfo);
    game_loop(gameInfo);

}

// handles the kitty round with all players
void kitty_round(GameInfo* gameInfo) {
    
    // send the winner of the betting round the 13 cards they have to
    // choose from, the player will send the three they don't want
    
    // send message to winner that they are to receive kitty data
    write(gameInfo->player[gameInfo->p].fd, "kittywin\n", 9);
    
    // make sure player is ready for input. should be yes\n
    read_from_fd(gameInfo->player[gameInfo->p].fd, BUFFER_LENGTH);
  
    // add kitty to the winners hand
    for (int i = 0; i < 3; i++) {
        gameInfo->player[gameInfo->p].deck[10 + i] = gameInfo->kitty[i];
        
    }
  
    // prepare string to send to the winner
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "You won! Pick 3 cards to discard: %s\n",
            return_hand(gameInfo->player[gameInfo->p].deck, 13));
    write(gameInfo->player[gameInfo->p].fd, msg, strlen(msg));
    
    // receive 3 cards the user wants to discard
    int c = 0;
    while (c != 3) {

        // send user a string of how many cards we still need
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Pick %d more cards!\n", 3 - c);
        
        write(gameInfo->player[gameInfo->p].fd, message, strlen(message)); 
                
        // get card, and check if card is valid and then
        // remove it from the players deck
        Card card = return_card_from_string(get_card(gameInfo));
        
        // check if the card was valid
        if (card.value == 0) {
            continue;
            
        } else if (remove_card_from_deck(card,
                &gameInfo->player[gameInfo->p].deck, 13 - c) == false) {
            // try and remove the card from the deck
            continue;
            
        } else {
            // otherwise it was a success, we need 1 less card now
            c++;
            
        }

        
    }
    
    // send kitty done to all
    send_to_all("kittyend\n", gameInfo);

}

// handles betting round with all players.
void bet_round(GameInfo* gameInfo) {
    
    // ignore misere case for now
    
    // set highest bet info
    gameInfo->highestBet = 0;
    gameInfo->suite = 0;
    gameInfo->betWinner = 0;
    
    gameInfo->p = 0; // player counter
    int pPassed = 0; // players passed counter
    
    // loop until winner
    while (pPassed != NUM_PLAYERS) {
            
        while (gameInfo->player[gameInfo->p].hasPassed == false) {
            
            // begin with player 0
            write(gameInfo->player[gameInfo->p].fd, "bet\n", 4);
            
            // get bet from player 0
            char* msg = get_card(gameInfo);
            
            char* send = malloc(BUFFER_LENGTH * sizeof(char));

            // check if it's a pass
            if (strcmp(msg, "PA\n") == 0) {
                // set passed to true, change send msg
                gameInfo->player[gameInfo->p].hasPassed = true;
                sprintf(send, "Player %d passed\n", gameInfo->p);
                pPassed++;
                
            } else {
                
                // check if the bet was valid
                if (valid_bet(&gameInfo->highestBet, 
                        &gameInfo->suite, msg) == false) {
                    continue;
                    
                }
                                 
                // string to send to all players
                sprintf(send, "Player %d bet %d%c\n", gameInfo->p,
                        gameInfo->highestBet,
                        return_trump_char(gameInfo->suite)); 
                
            }
            
            fprintf(stdout, "%s", send);
            send_to_all(send, gameInfo);
            break;
            
        }
        
        gameInfo->p++;
        gameInfo->p %= NUM_PLAYERS;
        
    }

    // send bet over to all players. Betting round finished
    send_to_all("betover\n", gameInfo);

    // get actual winning player
    gameInfo->p += 3;
    gameInfo->p %= NUM_PLAYERS;
    gameInfo->betWinner = gameInfo->p;
    
    if (gameInfo->highestBet == 0) {
        // case where everyone passed, we want to redeal 
        // TBA
        
    }
    
    // send winning bet to all players
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "Player %d won the bet with %d%c!\n", gameInfo->p,
            gameInfo->highestBet, return_trump_char(gameInfo->suite));
    fprintf(stdout, "%s", msg);
    send_to_all(msg, gameInfo);
    
}

// handles the play rounds. 
void play_round(GameInfo* gameInfo) {
    int rounds = 0;
    while (rounds != NUM_ROUNDS) {
        // send each player their deck now that it's started
        send_deck_to_all(NUM_ROUNDS - rounds, gameInfo);
        
        // send round to all
        char* buff = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(buff, "Round %d\n", rounds);
        send_to_all(buff, gameInfo);
        
        // winning card
        Card winner;
        winner.value = 0;
        winner.suite = 0;
        
        // number of successful plays so far and winning player
        int plays = 0;
        int win = gameInfo->p;
        
        while (plays != NUM_PLAYERS) {
            
            Card card;
            
            // loop until we get a valid card from the user
            while (1) {
                
                // tell user to send card
                write(gameInfo->player[gameInfo->p].fd, "send\n", 5);
                
                // get card and check validity and remove from deck
                card = return_card_from_string(get_card(gameInfo));
                if (card.value == 0) {
                    continue;
                    
                } else if (remove_card_from_deck(card,
                        &gameInfo->player[gameInfo->p].deck,
                        NUM_ROUNDS - rounds) == false) {
                    continue;
                    
                } else {
                    break;
                    
                }
                
            }
            
            // if we are the first to play, our card is automatically winning
            if (plays == 0) {
                winner = card;
                
            } else if (compare_cards(card, winner, gameInfo->suite) == 1) {
                // we have a valid card. check if it's higher than winner
                winner = card;
                win = gameInfo->p;
                
            }
            
            // send all the details of the move, different depending
            // on what play we are up to
            char* message = malloc(BUFFER_LENGTH * sizeof(char));
            if (++plays == NUM_PLAYERS) {
                sprintf(message, "Player %d played %s. Player %d won with %s\n",
                        gameInfo->p, return_card(card),
                        win, return_card(winner));
                        
            } else {
                sprintf(message, 
                        "Player %d played %s. Player %d winning with %s\n",
                        gameInfo->p, return_card(card),
                        win, return_card(winner));
            }
            
            send_to_all(message, gameInfo);
            fprintf(stdout, "%s", message);
            
            // increase plays and player number
            gameInfo->p++;
            gameInfo->p %= NUM_PLAYERS;
            
        }

        // round has finished
        send_to_all("roundover\n", gameInfo);
        rounds++; // add to round
        gameInfo->p = win; // winning player plays next
        gameInfo->player[gameInfo->p].wins++; // increment wins
        
        // send players how many tricks the winning team has won
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Betting team has won %d tricks\n",
                get_winning_tricks(gameInfo));
        
        send_to_all(message, gameInfo);
        fprintf(stdout, "%s", message);
        
    }
    
}

// sends all players their details, including who's on their team and 
// all username and player numbers.
void send_player_details(GameInfo* gameInfo) {
    // send all players the player details. 
    for (int i = 0; i < NUM_PLAYERS; i++) {
        for (int j = 0; j < NUM_PLAYERS; j++) {
            char* message = malloc(BUFFER_LENGTH * sizeof(char));

            // string that tells the user if the player is them or a teammate
            char* str = "";
            if (i == j) {
                str = " (You)";
                
            } else if ((i + 2) % NUM_PLAYERS == j) {
                str = " (Teammate)";
                
            } 
                   
            // send this users info to the player
            sprintf(message, "Player %d, '%s'%s\n", j,
                    gameInfo->player[j].name, str);
            write(gameInfo->player[i].fd, message, strlen(message));
            
            // await confirmation from user
            read_from_fd(gameInfo->player[i].fd, BUFFER_LENGTH);
            
        }
        
    }
}

// deals cards to the players decks. returns the kitty
Card* deal_cards(GameInfo* gameInfo) {

    // create deck
    Card* deck = create_deck();
    Card* kitty = malloc(3 * sizeof(Card));

    // malloc all players decks
    for (int i = 0; i < NUM_PLAYERS; i++) {
        gameInfo->player[i].deck = malloc(13 * sizeof(Card)); // 13 for kitty
        gameInfo->player[i].hasPassed = false; // set this property too
        gameInfo->player[i].wins = 0; // bonus properties
        
    }
    
    int j = 0; // kitty counter
        
    // send cards to users decks
    for(int i = 0; i < 43; i++) {
        // give to kitty
        if (i == 12 || i == 28 || i == 42) {
            kitty[j++] = deck[i];
            continue;
        }

        // give the card to the players deck
        if (i < 12) {
            // record the card in the players deck
            gameInfo->player[i % NUM_PLAYERS].deck[i / NUM_PLAYERS] = deck[i];            
            
        } else if (i < 28) {
            // record the card in the players deck
            gameInfo->player[(i - 1) % NUM_PLAYERS].deck[(i - 1)
                    / NUM_PLAYERS] = deck[i];
            
        } else {
            // record the card in the players deck
            gameInfo->player[(i - 2) % NUM_PLAYERS].deck[(i - 2)
                    / NUM_PLAYERS] = deck[i];
            
        }    
    
    }
    
    // sort each deck by suite here!
    
    
    // send each player their deck
    send_deck_to_all(10, gameInfo);

    // debugging, print deck
    fprintf(stdout, "Deck: %s\n", return_hand(deck, 43));
    
    // print each players hand
    for (int i = 0; i < NUM_PLAYERS; i++) {
        fprintf(stdout, "Player %d: %s\n", i,
                return_hand(gameInfo->player[i].deck, 10)); 
        
        
    }
    
    // print kitty 
    fprintf(stdout, "kitty: %s\n", return_hand(kitty, 3));
    
    return kitty;

}

// return a string representing a card read from the user, doubls up as getting
// bet too. exists if player has left
char* get_card(GameInfo* gameInfo) {
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    if (read(gameInfo->player[gameInfo->p].fd, msg,
            BUFFER_LENGTH) == -1) {
        // player left early
        fprintf(stderr, "Unexpected exit\n");
        send_to_all_except("badinput\n", gameInfo, gameInfo->p);
        exit(5);
        
    }

    return msg;
    
}

// number of tricks the betting team has won
int get_winning_tricks(GameInfo* gameInfo) {
    return gameInfo->player[gameInfo->betWinner].wins + 
        gameInfo->player[(gameInfo->betWinner + 2) % NUM_PLAYERS].wins;
    
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
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (i != except) {
            write(gameInfo->player[i].fd, message, strlen(message));
            
        }
    }
    
    return 0;
    
}

// send each player their deck
bool send_deck_to_all(int cards, GameInfo* gameInfo) {
    // send each player their deck
    for (int i = 0; i < NUM_PLAYERS; i++) {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", return_hand(gameInfo->player[i].deck, cards));
        write(gameInfo->player[i].fd, message, strlen(message));
        
    }
    
    return true;
    
}
    
// opens the port for listening, exits with error if didn't work
int open_listen(int port) {
    struct sockaddr_in serverAddr;
    bool error = false;
    
    // create socket - IPv4 TCP socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        error = true;
    }

    // allow address (port number) to be reused immediately
    int optVal = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(int)) < 0) {
        error = true;
    }

    // populate server address structure to indicate the local address(es)
    serverAddr.sin_family = AF_INET; // IPv4
    
    serverAddr.sin_port = htons(port); // port number - in network byte order
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // any IP address 

    // bind our socket to this address (IP addresses + port number)
    if (bind(fd, (struct sockaddr*)&serverAddr,
            sizeof(struct sockaddr_in)) < 0) {
        error = true;
    }
    
    socklen_t sa_len = sizeof(struct sockaddr_in);
    getsockname(fd, (struct sockaddr*)&serverAddr, &sa_len);
    int listenPort = (unsigned int) ntohs(serverAddr.sin_port);

    // indicate our willingness to accept connections. queue length is SOMAXCONN
    // (128 by default) - max length of queue of connections
    if (listen(fd, SOMAXCONN) < 0) {
        error = true;
        
    }
    
    // check if listen was successful
    if (error == true) {
        fprintf(stderr, "Failed listen\n");
        exit(4);
    }
    
    // print message that we are listening
    fprintf(stdout, "Listening on %d\n", listenPort);

    return fd;
}
