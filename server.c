// server specific deps
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

// header files
#include "shared.h"
#include "cards.h"

// server specific defines
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

    Card* deck; // players hand
    char* name;
    int fd;
    bool hasPassed;
    int wins; // number of tricks this player has won

} Player;

// stores game info
typedef struct GameInfo {

    // player and card array
    Player* player;
    Card* kitty;

    int highestBet; // highest bet number
    Trump suite; // highest bet suite
    int betWinner; // person who won the betting round (and did the kitty)
    Trump jokerSuite; // suite of the joker for no trumps

    int p; // current player selected

    char* password;
    unsigned int timeout; // currently unimplemented
    int fd; // file descriptor of server

} GameInfo;

// server
int open_listen(int port); // returns fd for listening
void process_connections(GameInfo* game);

// sending, no returns
void send_to_all(char* message, GameInfo* game);
void send_to_all_except(char* message, GameInfo* game, int except);
void send_deck_to_all(int cards, GameInfo* game);
void send_to_player(int player, GameInfo* game, char* message);

// reading
char* get_message_from_player(GameInfo* game);

// input related
int check_valid_username(char* user, GameInfo* game);
int get_winning_tricks(GameInfo* game);
Card* deal_cards(GameInfo* game); // returns the kitty (Card* with 3 entries)

// rounds - no returns
void send_player_details(GameInfo* game);
void bet_round(GameInfo* game);
void kitty_round(GameInfo* game);
void joker_round(GameInfo* game);
void play_round(GameInfo* game);

int main(int argc, char** argv) {

    // check arg count
    if (argc != 3) {
        fprintf(stderr, "Usage: server port password\n");
        return 1;
    }

    // check valid password (correct length)
    if (strlen(argv[2]) > MAX_PASS_LENGTH) {
        fprintf(stderr, "password too long\n");
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
    GameInfo game;
    game.fd = fdServer;
    game.password = argv[2];
    game.p = 0;
    game.player = malloc(NUM_PLAYERS * sizeof(Player));

    // process connections
    process_connections(&game);

}

// main game loop
void game_loop(GameInfo* game) {
    fprintf(stdout, "Game starting\n");

    send_player_details(game);

    // shuffle and deal
    fprintf(stdout, "Shuffling and Dealing\n");
    srand(time(NULL)); // random seed
    game->kitty = deal_cards(game); // kitty and dealing, debug info too

    // betting round
    fprintf(stdout, "Betting round starting\n");
    bet_round(game);
    fprintf(stdout, "Betting round ending\n");

    // kitty round
    fprintf(stdout, "Dealing kitty\n");
    kitty_round(game);
    fprintf(stdout, "Kitty finished\n");

    // joker suite round. note this only occurs if there are no trumps.
    joker_round(game);

    // play round
    fprintf(stdout, "Game Begins\n");
    play_round(game);

    // tell the users if the betting team has won or not
    // number of tricks the betting team has won
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    if (game->highestBet > get_winning_tricks(game)) {
        result = "Betting team lost!\n";

    } else {
        result = "Betting team won!\n";

    }

    send_to_all(result, game);
    fprintf(stdout, "%s", result);

    // game is over. exit
    exit(0);
}

// process connections (from the lecture with my additions)
void process_connections(GameInfo* game) {
    int fileDes;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;

    // loop until we have 4 players waiting to start a game
    while (game->p != NUM_PLAYERS) {

        fromAddrSize = sizeof(struct sockaddr_in);
        // block, waiting for a new connection (fromAddr will be populated
        // with address of the client. Accept

        fileDes = accept(game->fd, (struct sockaddr*)&fromAddr,  &fromAddrSize);

        // check password is the same as ours
        if (strcmp(read_from_fd(fileDes, MAX_PASS_LENGTH),
                game->password) != 0) {

            // otherwise we got illegal input. Send no.
            write(fileDes, "no\n", 3);
            close(fileDes);
            continue;

        }

        // password was valid, send yes to client for password
        write(fileDes, "yes\n", 4);

        // get user name from client
        char* userBuffer = read_from_fd(fileDes, MAX_NAME_LENGTH);

        // check username is valid, i.e. not taken before
        if (check_valid_username(userBuffer, game)) {
            // send no to client for user name, and forget this connection
            write(fileDes, "no\n", 3);
            close(fileDes);
            continue;
        }

        // send yes to client for user name
        write(fileDes, "yes\n", 4);

        // server message
        fprintf(stdout, "Player %d connected with user name '%s'\n",
            game->p, userBuffer);

        // add name
        game->player[game->p].name = strdup(userBuffer);
        game->player[game->p].fd = fileDes;
        game->p++;

    }

    // we now have 4 players. send start to all
    send_to_all("start\n", game);

    // we want a yes from all players to ensure they are still here
    for (int i = 0; i < NUM_PLAYERS; i++) {

        // read 4 characters, just make sure that it works
        if (read(game->player[i].fd, malloc(4 * sizeof(char)), 4) <= 0) {

            // if we don't get a yes, then send a message that game
            // is not starting and we exit
            fprintf(stderr, "Unexpected exit\n");
            send_to_all_except("nostart\n", game, i);
            exit(5);

        }

    }

    // all players are here, start the game!
    send_to_all("start\n", game);
    game_loop(game);

}

// sends all players their details, including who's on their team and
// all username and player numbers.
void send_player_details(GameInfo* game) {
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
            sprintf(message, "Player %d, '%s'%s\n", j,
                    game->player[j].name, str);
            send_to_player(i, game, message);

            // await confirmation from user
            read_from_fd(game->player[i].fd, BUFFER_LENGTH);

        }

    }
}

// deals cards to the players decks. returns the kitty
Card* deal_cards(GameInfo* game) {

    // create deck
    Card* deck = create_deck();
    Card* kitty = malloc(3 * sizeof(Card));

    // malloc all players decks
    for (int i = 0; i < NUM_PLAYERS; i++) {
        game->player[i].deck = malloc(13 * sizeof(Card)); // 13 for kitty
        game->player[i].hasPassed = false; // set this property too
        game->player[i].wins = 0; // bonus properties

    }

    int kittyCount = 0; // kitty counter

    // send cards to users decks
    for(int i = 0; i < 43; i++) {
        // give to kitty
        if (i == 12 || i == 28 || i == 42) {
            kitty[kittyCount++] = deck[i];
            continue;
        }

        // give the card to the players deck
        if (i < 12) {
            // record the card in the players deck
            game->player[i % NUM_PLAYERS].deck[i / NUM_PLAYERS] = deck[i];

        } else if (i < 28) {
            // record the card in the players deck
            game->player[(i - 1) % NUM_PLAYERS].deck[(i - 1)
                    / NUM_PLAYERS] = deck[i];

        } else {
            // record the card in the players deck
            game->player[(i - 2) % NUM_PLAYERS].deck[(i - 2)
                    / NUM_PLAYERS] = deck[i];

        }

    }

    // sort each deck by suite here!

    // send each player their deck and return the kitty
    send_deck_to_all(10, game);
    return kitty;

}

// handles betting round with all players.
void bet_round(GameInfo* game) {
    // ignore misere case for now

    // set highest bet info
    game->highestBet = 0;
    game->suite = 0;
    game->betWinner = 0;
    game->p = 0; // reset player counter

    // loop until NUM_PLAYERS have passed
    for (int pPassed = 0; pPassed != NUM_PLAYERS; ) {

        // string to send to all users
        char* send = malloc(BUFFER_LENGTH * sizeof(char));

        // loop until we get a valid bet
        while (game->player[game->p].hasPassed == false) {

            // send message to player
            send_to_player(game->p, game, "bet\n");

            // get bet from player 0
            char* msg = get_message_from_player(game);

            // check if it's a pass
            if (strcmp(msg, "PA\n") == 0) {
                // set passed to true, change send msg
                game->player[game->p].hasPassed = true;
                sprintf(send, "Player %d passed\n", game->p);
                pPassed++;
                break;

            } else if (valid_bet(&game->highestBet,
                    &game->suite, msg) == true) {
                // update string
                sprintf(send, "Player %d bet %d%c\n", game->p, game->highestBet,
                        return_trump_char(game->suite));
                break;

            }

            send_to_player(game->p, game, "Invalid bet\n");

        }

        // send the bet to everyone
        fprintf(stdout, "%s", send);
        send_to_all(send, game);

        // increase current player counter
        game->p++;
        game->p %= NUM_PLAYERS;

    }

    // case where everyone passed, we want to redeal
    if (game->highestBet == 0) {
        send_to_all("redeal\n", game);
        fprintf(stdout, "Everyone passed. Game restarting.\n");
        game_loop(game);

    }

    // send bet over to all players. Betting round finished
    send_to_all("betover\n", game);

    // get actual winning player
    game->p += 3;
    game->p %= NUM_PLAYERS;
    game->betWinner = game->p;

    // send winning bet to all players
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "Player %d won the bet with %d%c!\n", game->p,
            game->highestBet, return_trump_char(game->suite));
    fprintf(stdout, "%s", msg);
    send_to_all(msg, game);

}

// handles the kitty round with all players.
// sends the winner of the betting round the 13 cards they have to choose
// from, the player will send the three they don't want and we remove them
void kitty_round(GameInfo* game) {

    // add kitty to the winners hand
    for (int i = 0; i < 3; i++) {
        game->player[game->p].deck[10 + i] = game->kitty[i];

    }

    // prepare string to send to the winner and send it
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    sprintf(msg, "You won! Pick 3 cards to discard: %s\n",
            return_hand(game->player[game->p].deck, NUM_ROUNDS + 3));
    send_to_player(game->p, game, msg);

    // receive 3 cards the user wants to discard
    for (int c = 0; c != 3; ) {
        // ask for some input
        send_to_player(game->p, game, "send\n");

        // send user a string of how many cards we still need
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Pick %d more card(s)\n", 3 - c);
        send_to_player(game->p, game, message);

        // get card, and check if valid then remove it from the players deck
        Card card = return_card_from_string(get_message_from_player(game));

        // check if the card was valid
        if (card.value == 0) {
            send_to_player(game->p, game, "Not a card\n");
            continue;

        } else if (remove_card_from_deck(card,
                &game->player[game->p].deck, NUM_ROUNDS + 3 - c) == false) {
            // try and remove the card from the deck
            send_to_player(game->p, game, "You don't have that card\n");
            continue;

        } else {
            // otherwise it was a success, we need 1 less card now
            c++;

        }

    }

    // send kitty done to all
    send_to_all("kittyend\n", game);

}

// handles choosing the joker suite, if it is no trumps
void joker_round(GameInfo* game) {
    game->jokerSuite = DEFAULT_SUITE; // default joker suite

    // choose joker suite round
    if (game->suite == NOTRUMPS) {
        fprintf(stdout, "Choosing joker suite\n");

        // find which player has the joker. loop through each players deck
        int playerWithJoker = -1;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (deck_has_joker(game->player[i].deck) == true) {
                playerWithJoker = i;
                break;

            }

        }

        // get suite from user, assuming a player has the joker
        while (playerWithJoker != -1) {
            // ask user for suite
            send_to_player(playerWithJoker, game, "jokerwant\n");

            // get suite of player with joker
            game->p = playerWithJoker;
            char* msg = get_message_from_player(game);

            // check length valid
            if (strlen(msg) != 2) {
                send_to_player(game->p, game, "Bad length\n");
                continue;

            }

            // check suite valid
            if (return_trump(msg[0]) != DEFAULT_SUITE &&
                    return_trump(msg[0]) != NOTRUMPS) {
                // set the jokers suite in the game, and we're done
                game->jokerSuite = return_trump(msg[0]);
                break;

            }

            send_to_player(game->p, game, "Not a suite\n");

        }

    }

    // send joker suite if it was chosen
    if (game->jokerSuite != DEFAULT_SUITE) {
        // prepare string
        char* send = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(send, "Joker suite chosen. It is a %c\n",
                return_trump_char(game->jokerSuite));
        send_to_all(send, game);
        fprintf(stdout, "%s", send);

    }

    send_to_all("jokerdone\n", game);

}

// handles the play rounds.
void play_round(GameInfo* game) {
    // reset current player
    game->p = game->betWinner;

    // loop until 10 rounds have passed
    for (int rounds = 0; rounds != NUM_ROUNDS; ) {
        // send each player their deck now that it's started
        send_deck_to_all(NUM_ROUNDS - rounds, game);

        // send round to all
        char* buff = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(buff, "Round %d\n", rounds);
        send_to_all(buff, game);

        // winning card
        Card winner;
        winner.value = 0;
        winner.suite = 0;

        // leading trump
        Trump lead = DEFAULT_SUITE;

        // Winning player of this round
        int win = game->p;

        // loop until we get to NUM_PLAYERS plays
        for (int plays = 0; plays != NUM_PLAYERS; ) {

            // current card
            Card card;

            // loop until we get a valid card from the user
            while (1) {

                // tell user to send card
                send_to_player(game->p, game, "send\n");

                // get card and check validity and remove from deck
                card = return_card_from_string(get_message_from_player(game));
                if (card.value == 0) {
                    send_to_player(game->p, game, "Not a card\n");
                    continue;

                }

                // handle joker here so correct_suite_player works
                if (card.value == JOKER_VALUE && game->suite == NOTRUMPS) {
                    card.suite = game->jokerSuite;

                } else if (card.value == JOKER_VALUE) {
                    card.suite = game->suite;

                }

                // check that the player doesn't have another card they
                // must be playing, and check they have the card in their deck
                if (correct_suite_player(card, game->player[game->p].deck, lead,
                        game->suite, NUM_ROUNDS - rounds) == false) {
                    send_to_player(game->p, game, "You must play lead suite\n");
                    continue;

                } else if (remove_card_from_deck(card,
                        &game->player[game->p].deck,
                        NUM_ROUNDS - rounds) == false) {
                    send_to_player(game->p, game, "You don't have that card\n");
                    continue;

                } else {
                    break; // card is good

                }

            }

            // if we are the first to play, our card is automatically winning
            if (plays == 0) {
                winner = card;
                lead = handle_bower(card, game->suite).suite;

            } else if (compare_cards(card, winner, game->suite) == 1) {
                // we have a valid card. check if it's higher than winner
                winner = card;
                win = game->p;

            }

            // send all the details of the move, who's winning / won.
            char* message = malloc(BUFFER_LENGTH * sizeof(char));
            if (++plays == NUM_PLAYERS) {
                sprintf(message, "Player %d played %s. Player %d won with %s\n",
                        game->p, return_card(card), win, return_card(winner));

            } else {
                sprintf(message,
                        "Player %d played %s. Player %d winning with %s\n",
                        game->p, return_card(card), win, return_card(winner));

            }

            send_to_all(message, game);
            fprintf(stdout, "%s", message);

            // increase plays and player number
            game->p++;
            game->p %= NUM_PLAYERS;

        }

        // round has finished
        send_to_all("roundover\n", game);
        rounds++; // add to round
        game->p = win; // winning player plays next
        game->player[game->p].wins++; // increment wins

        // send players how many tricks the winning team has won
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "Betting team has won %d tricks\n",
                get_winning_tricks(game));

        send_to_all(message, game);
        fprintf(stdout, "%s", message);

    }

}

// return a string representing a card read from the current player, doubles up
// as getting bet too. exists if player has left
char* get_message_from_player(GameInfo* game) {
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));
    if (read(game->player[game->p].fd, msg, BUFFER_LENGTH) <= 0) {
        // player left early
        fprintf(stderr, "Unexpected exit\n");
        send_to_all_except("badinput\n", game, game->p);
        exit(5);

    }

    return msg;

}

// number of tricks the betting team has won
int get_winning_tricks(GameInfo* game) {
    return game->player[game->betWinner].wins +
        game->player[(game->betWinner + 2) % NUM_PLAYERS].wins;

}

// returns 0 if the username has not been taken before, 1 otherwise
int check_valid_username(char* user, GameInfo* game) {
    for (int i = 0; i < game->p; i++) {
        if (strcmp(user, game->player[i].name) == 0) {
            return 1;

        }

    }

    return 0;

}

// sends the given message to the specified player
void send_to_player(int player, GameInfo* game, char* message) {
    write(game->player[player].fd, message, strlen(message));

}

// sends a message to all players
void send_to_all(char* message, GameInfo* game) {
    send_to_all_except(message, game, -1);

}

// sends a message to all players, except the given one
// this exists so the server doesn't send any messages on closing
// to fd's that have been closed after a failed read
void send_to_all_except(char* message, GameInfo* game, int except) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (i != except) {
            send_to_player(i, game, message);

        }
    }

}

// send each player their deck
void send_deck_to_all(int cards, GameInfo* game) {
    // send each player their deck
    for (int i = 0; i < NUM_PLAYERS; i++) {
        char* message = malloc(BUFFER_LENGTH * sizeof(char));
        sprintf(message, "%s\n", return_hand(game->player[i].deck, cards));
        send_to_player(i, game, message);

    }

}

// opens the port for listening, exits with error if didn't work
int open_listen(int port) {
    struct sockaddr_in serverAddr;
    bool error = false;

    // create socket - IPv4 TCP socket
    int fileDes = socket(AF_INET, SOCK_STREAM, 0);
    if(fileDes < 0) {
        error = true;
    }

    // allow address (port number) to be reused immediately
    int optVal = 1;
    if (setsockopt(fileDes, SOL_SOCKET, SO_REUSEADDR,
            &optVal, sizeof(int)) < 0) {
        error = true;
    }

    // populate server address structure to indicate the local address(es)
    serverAddr.sin_family = AF_INET; // IPv4
    serverAddr.sin_port = htons(port); // port number - in network byte order
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // any IP address

    // bind our socket to this address (IP addresses + port number)
    if (bind(fileDes, (struct sockaddr*)&serverAddr,
            sizeof(struct sockaddr_in)) < 0) {
        error = true;
    }

    socklen_t sa_len = sizeof(struct sockaddr_in);
    getsockname(fileDes, (struct sockaddr*)&serverAddr, &sa_len);
    int listenPort = (unsigned int) ntohs(serverAddr.sin_port);

    // indicate our willingness to accept connections. queue length is SOMAXCONN
    // (128 by default) - max length of queue of connections
    if (listen(fileDes, SOMAXCONN) < 0) {
        error = true;

    }

    // check if listen was successful
    if (error == true) {
        fprintf(stderr, "Failed listen\n");
        exit(4);
    }

    // print message that we are listening
    fprintf(stdout, "Listening on %d\n", listenPort);

    return fileDes;
}
