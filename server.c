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
    Trump jokerSuite;
    
    int players; // current player count
    int p; // current player selected
    
    // permanent stats
    char* password;
    unsigned int timeout;
    int fd; 

} GameInfo;

// function declaration
// server related functions
int open_listen(int port);
void process_connections(GameInfo* games);
int send_to_all(char* message, GameInfo* g);
int read_from_all(char* message, GameInfo* g);
int send_to_all_except(char* message, GameInfo* g, int except);
bool send_deck_to_all(int cards, GameInfo* g);
int send_to_player(int player, GameInfo* g, char* message);
char* get_message_from_player(GameInfo* g);

// input related functions
int check_valid_username(char* user, GameInfo* g);
Card* deal_cards(GameInfo* g);
int get_winning_tricks(GameInfo* g);

// rounds
void kitty_round(GameInfo* g);
void bet_round(GameInfo* g);
void send_player_details(GameInfo* g);
void play_round(GameInfo* g);
void joker_round(GameInfo* g);

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
    GameInfo g;
    g.fd = fdServer;
    g.password = argv[2];
    g.players = 0;
    g.player = malloc(NUM_PLAYERS * sizeof(Player));
    
    // process connections
    process_connections(&g);

}

// main game loop
void game_loop(GameInfo* g) {
    fprintf(stdout, "Game starting\n");
    
    send_player_details(g);
        
    // shuffle and deal
    fprintf(stdout, "Shuffling and Dealing\n");
    srand(time(NULL)); // random seed
    g->kitty = deal_cards(g); // kitty and dealing, debug info too
    
    // betting round
    fprintf(stdout, "Betting round starting\n");
    bet_round(g);
    fprintf(stdout, "Betting round ending\n");

    // kitty round
    fprintf(stdout, "Dealing kitty\n");
    kitty_round(g);
    fprintf(stdout, "Kitty finished\n");
    
    // joker suite round. note this only occurs if there are no trumps.
    joker_round(g);
    
    // play round
    fprintf(stdout, "Game Begins\n");
    play_round(g);
    
    // tell the users if the betting team has won or not
    // number of tricks the betting team has won
    char* result = malloc(BUFFER_LENGTH * sizeof(char));
    
    if (g->highestBet > get_winning_tricks(g)) {
        result = "Betting team lost!\n";
        
    } else {
        result = "Betting team won!\n";
        
    }
    
    send_to_all(result, g);
    fprintf(stdout, "%s", result);
    
    // game is over. exit
    exit(0);
}

// process connections (from the lecture with my additions)
void process_connections(GameInfo* g) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // loop until we have 4 players waiting to start a game
    while (g->players != NUM_PLAYERS) {

        fromAddrSize = sizeof(struct sockaddr_in);
        // block, waiting for a new connection (fromAddr will be populated
        // with address of the client. Accept        
                
        fd = accept(g->fd, (struct sockaddr*)&fromAddr,  &fromAddrSize);

        // check password is the same as ours
        if (strcmp(read_from_fd(fd, MAX_PASS_LENGTH), g->password) != 0) {
        
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
        if (check_valid_username(userBuffer, g)) {
            // send no to client for user name, and forget this connection
            write(fd, "no\n", 3);
            close(fd);
            continue;
        }
        
        // send yes to client for user name
        write(fd, "yes\n", 4);

        // server message
        fprintf(stdout, "Player %d connected with user name '%s'\n",
            g->players, userBuffer);
        
        // add name
        g->player[g->players].name = strdup(userBuffer);
        g->player[g->players].fd = fd;
        g->players++;
            
    }
    
    // we now have 4 players. send start to all
    send_to_all("start\n", g);

    // we want a yes from all players to ensure they are still here
    for (int i = 0; i < NUM_PLAYERS; i++) {
        
        // read 4 characters, just make sure that it works
        if (read(g->player[i].fd, malloc(4 * sizeof(char)), 4) <= 0) {
            
            // if we don't get a yes, then send a message that game 
            // is not starting and we exit
            fprintf(stderr, "Unexpected exit\n");
            send_to_all_except("nostart\n", g, i);
            exit(5);
            
        }
        
    }

    // all players are here, start the game!
    send_to_all("start\n", g);
    game_loop(g);

}

// sends all players their details, including who's on their team and 
// all username and player numbers.
void send_player_details(GameInfo* g) {
    // send all players the player details. 
    for (int i = 0; i < NUM_PLAYERS; i++) {
        for (int j = 0; j < NUM_PLAYERS; j++) {
            // string that tells the user if the player is them or a teammate
            char* str = "";
            if (i == j) {
                str = " (You)";
                
            } else if ((i + 2) % NUM_PLAYERS == j) {
                str = " (Teammate)";
                
            } 
                   
            // send this users info to the player
            char* message = malloc(BUFFER_LENGTH * sizeof(char));
            sprintf(message, "Player %d, '%s'%s\n", j, g->player[j].name, str);
            send_to_player(i, g, message);
            
            // await confirmation from user
            read_from_fd(g->player[i].fd, BUFFER_LENGTH);
            
        }
        
    }
}

// handles the kitty round with all players
void kitty_round(GameInfo* g) {
    // send the winner of the betting round the 13 cards they have to
    // choose from, the player will send the three they don't want
      
    // add kitty to the winners hand
    for (int i = 0; i < 3; i++) {
        g->player[g->p].deck[10 + i] = g->kitty[i];
        
    }
  
    // prepare string to send to the winner and send it 
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "You won! Pick 3 cards to discard: %s\n",
            return_hand(g->player[g->p].deck, NUM_ROUNDS + 3));
    send_to_player(g->p, g, msg);
    
    // receive 3 cards the user wants to discard
    for (int c = 0; c != 3; ) {
        // ask for some input
        send_to_player(g->p, g, "send\n");

        // send user a string of how many cards we still need
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Pick %d more card(s)\n", 3 - c);
        send_to_player(g->p, g, message);

        // get card, and check if card is valid and then
        // remove it from the players deck
        Card card = return_card_from_string(get_message_from_player(g));
        
        // check if the card was valid
        if (card.value == 0) {
            send_to_player(g->p, g, "Not a card\n");
            continue;
            
        } else if (remove_card_from_deck(card,
                &g->player[g->p].deck, NUM_ROUNDS + 3 - c) == false) {
            // try and remove the card from the deck
            send_to_player(g->p, g, "You don't have that card\n");
            continue;
            
        } else {
            // otherwise it was a success, we need 1 less card now
            c++;
            
        }

        
    }
    
    // send kitty done to all
    send_to_all("kittyend\n", g);

}

// handles betting round with all players.
void bet_round(GameInfo* g) {
    // ignore misere case for now
    
    // set highest bet info
    g->highestBet = 0;
    g->suite = 0;
    g->betWinner = 0;
    g->p = 0; // player counter

    // loop until NUM_PLAYERS have passed
    for (int pPassed = 0; pPassed != NUM_PLAYERS; ) {
            
        while (g->player[g->p].hasPassed == false) {
            
            // begin with player 0
            send_to_player(g->p, g, "bet\n");
            
            // get bet from player 0
            char* msg = get_message_from_player(g);
            char* send = malloc(BUFFER_LENGTH * sizeof(char));

            // check if it's a pass
            if (strcmp(msg, "PA\n") == 0) {
                // set passed to true, change send msg
                g->player[g->p].hasPassed = true;
                sprintf(send, "Player %d passed\n", g->p);
                pPassed++;
                
            } else {
                
                // check if the bet was valid
                if (valid_bet(&g->highestBet, &g->suite, msg) == false) {
                            
                    send_to_player(g->p, g, "Bad input\n");
                    continue;
                    
                }
                                 
                // string to send to all players
                sprintf(send, "Player %d bet %d%c\n", g->p, g->highestBet,
                        return_trump_char(g->suite)); 
                
            }
            
            fprintf(stdout, "%s", send);
            send_to_all(send, g);
            break;
            
        }
        
        // increase current player counter
        g->p++;
        g->p %= NUM_PLAYERS;
        
    }
    
    // case where everyone passed, we want to redeal 
    if (g->highestBet == 0) {
        send_to_all("redeal\n", g);
        fprintf(stdout, "Everyone passed. Game restarting.\n");
        game_loop(g);
        
    }
    
    // send bet over to all players. Betting round finished
    send_to_all("betover\n", g);

    // get actual winning player
    g->p += 3;
    g->p %= NUM_PLAYERS;
    g->betWinner = g->p;
    
    // send winning bet to all players
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "Player %d won the bet with %d%c!\n", g->p,
            g->highestBet, return_trump_char(g->suite));
    fprintf(stdout, "%s", msg);
    send_to_all(msg, g);
    
}

// handles choosing the joker suite, if it is no trumps
void joker_round(GameInfo* g) {
    g->jokerSuite = DEFAULT_SUITE; // default joker suite

    // choose joker suite round
    if (g->suite == NOTRUMPS) {
        fprintf(stdout, "Choosing joker suite\n");
        
        // find which player has the joker. loop through each players deck 
        int playerWithJoker = 0;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (deck_has_joker(g->player[i].deck) == true) {
                playerWithJoker = i;
                break;
                
            }
                
        }
        
        // get suite from user
        while (1) {
            // ask user for suite
            send_to_player(playerWithJoker, g, "jokerwant\n");

            // get suite
            char* msg = get_message_from_player(g);
            
            // check length valid
            if (strlen(msg) != 2) { 
                send_to_player(g->p, g, "Bad length\n");
                continue;
                
            }
            
            // check suite valid
            if (return_trump(msg[0]) != DEFAULT_SUITE && 
                    return_trump(msg[0]) != NOTRUMPS) {
                // set the jokers suite in the g, and we're done
                g->jokerSuite = return_trump(msg[0]);
                break;
                
            }
            
            send_to_player(g->p, g, "Not a suite\n");

        }        
        
    }
    
    // send joker suite if it was chosen
    if (g->jokerSuite != DEFAULT_SUITE) {
        // prepare string
        char* send = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(send, "Joker suite chosen. It is a %c\n",
                return_trump_char(g->jokerSuite));
        send_to_all(send, g);
        fprintf(stdout, "%s", send);
        
    }
    
    send_to_all("jokerdone\n", g);

}

// handles the play rounds. 
void play_round(GameInfo* g) {
    // loop until 10 rounds have passed
    for (int rounds = 0; rounds != NUM_ROUNDS; ) {
        // send each player their deck now that it's started
        send_deck_to_all(NUM_ROUNDS - rounds, g);
        
        // send round to all
        char* buff = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(buff, "Round %d\n", rounds);
        send_to_all(buff, g);
        
        // winning card
        Card winner;
        winner.value = 0;
        winner.suite = 0;
        
        // leading trump
        Trump lead = DEFAULT_SUITE;
        
        // Winning player of this round
        int win = g->p;
        
        // loop until we get to NUM_PLAYERS plays
        for (int plays = 0; rounds != NUM_PLAYERS; ) {
            
            // current card
            Card card;
            
            // loop until we get a valid card from the user
            while (1) {
                
                // tell user to send card
                send_to_player(g->p, g, "send\n");
                
                // get card and check validity and remove from deck
                card = return_card_from_string(get_message_from_player(g));
                if (card.value == 0) {
                    send_to_player(g->p, g, "Not a card\n");
                    continue;

                }
                
                // handle joker here so correct_suite_player works
                if (card.value == JOKER_VALUE) {
                    if (g->suite == NOTRUMPS) {
                        card.suite = g->jokerSuite;
                        
                    } else {
                        card.suite = g->suite;
                        
                    }
                    
                }
            
                // check that the player doesn't have another card they 
                // must be playing, and check they have the card in their deck
                if (correct_suite_player(card, g->player[g->p].deck, lead,
                        g->suite, NUM_ROUNDS - rounds) == false) {
                    send_to_player(g->p, g, "You must play lead suite\n");
                    continue;
                    
                } else if (remove_card_from_deck(card, &g->player[g->p].deck,
                        NUM_ROUNDS - rounds) == false) {
                    send_to_player(g->p, g, "You don't have that card\n");
                    continue;
                    
                } else {
                    break;
                    
                }
                
            }
            
            // if we are the first to play, our card is automatically winning
            if (plays == 0) {
                winner = card;
                lead = handle_bower(card, g->suite).suite;
                
            } else if (compare_cards(card, winner, g->suite) == 1) {
                // we have a valid card. check if it's higher than winner
                winner = card;
                win = g->p;
                
            }
            
            // send all the details of the move, who's winning / won.
            char* message = malloc(BUFFER_LENGTH * sizeof(char));
            if (++plays == NUM_PLAYERS) {
                sprintf(message, "Player %d played %s. Player %d won with %s\n",
                        g->p, return_card(card), win, return_card(winner));
                        
            } else {
                sprintf(message, 
                        "Player %d played %s. Player %d winning with %s\n",
                        g->p, return_card(card), win, return_card(winner));
                        
            }
            
            send_to_all(message, g);
            fprintf(stdout, "%s", message);
            
            // increase plays and player number
            g->p++;
            g->p %= NUM_PLAYERS;
            
        }

        // round has finished
        send_to_all("roundover\n", g);
        rounds++; // add to round
        g->p = win; // winning player plays next
        g->player[g->p].wins++; // increment wins
        
        // send players how many tricks the winning team has won
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Betting team has won %d tricks\n",
                get_winning_tricks(g));
        
        send_to_all(message, g);
        fprintf(stdout, "%s", message);
        
    }
    
}

// deals cards to the players decks. returns the kitty
Card* deal_cards(GameInfo* g) {

    // create deck
    Card* deck = create_deck();
    Card* kitty = malloc(3 * sizeof(Card));

    // malloc all players decks
    for (int i = 0; i < NUM_PLAYERS; i++) {
        g->player[i].deck = malloc(13 * sizeof(Card)); // 13 for kitty
        g->player[i].hasPassed = false; // set this property too
        g->player[i].wins = 0; // bonus properties
        
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
            g->player[i % NUM_PLAYERS].deck[i / NUM_PLAYERS] = deck[i];            
            
        } else if (i < 28) {
            // record the card in the players deck
            g->player[(i - 1) % NUM_PLAYERS].deck[(i - 1)
                    / NUM_PLAYERS] = deck[i];
            
        } else {
            // record the card in the players deck
            g->player[(i - 2) % NUM_PLAYERS].deck[(i - 2)
                    / NUM_PLAYERS] = deck[i];
            
        }    
    
    }
    
    // sort each deck by suite here!
    
    
    // send each player their deck
    send_deck_to_all(10, g);

    return kitty;

}

// return a string representing a card read from the current player, doubles up 
// as getting bet too. exists if player has left
char* get_message_from_player(GameInfo* g) {
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    if (read(g->player[g->p].fd, msg, BUFFER_LENGTH) <= 0) {
        // player left early
        fprintf(stderr, "Unexpected exit\n");
        send_to_all_except("badinput\n", g, g->p);
        exit(5);
        
    }

    return msg;
    
}

// number of tricks the betting team has won
int get_winning_tricks(GameInfo* g) {
    return g->player[g->betWinner].wins + 
        g->player[(g->betWinner + 2) % NUM_PLAYERS].wins;
    
}

// returns 0 if the username has not been taken before, 1 otherwise
int check_valid_username(char* user, GameInfo* g) {
    for (int i = 0; i < g->players; i++) {        
        if (strcmp(user, g->player[i].name) == 0) {
            return 1;
            
        }
        
    }
    
    return 0;
    
}

// sends the given message to the specified player
int send_to_player(int player, GameInfo* g, char* message) {
    write(g->player[player].fd, message, strlen(message));
    return 0;
    
}

// sends a message to all players
int send_to_all(char* message, GameInfo* g) {
    return send_to_all_except(message, g, -1);
    
}

// sends a message to all players, except the given one
// this exists so the server doesn't send any messages on closing 
// to fd's that have been closed after a failed read
int send_to_all_except(char* message, GameInfo* g, int except) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (i != except) {
            send_to_player(i, g, message);
            
        }
    }
    
    return 0;
    
}

// send each player their deck
bool send_deck_to_all(int cards, GameInfo* g) {
    // send each player their deck
    for (int i = 0; i < NUM_PLAYERS; i++) {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", return_hand(g->player[i].deck, cards));
        send_to_player(i, g, message);
        
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
