// server specific deps
#include <time.h>

// header files
// bot.h -> server.h -> cards.h -> shared.h
#include "bot.h"

// Exit Statuses
// 1: Wrong arguments
// 2: Password too long
// 3: Invalid port
// 4: Failed listen
// 5: Unexpected exit
// 6: Bad playertypes string

// server
int open_listen(int port); // returns fd for listening
void process_connections(GameInfo* game);

// sending, no returns
void send_to_all(char* message, GameInfo* game);
void send_to_all_except(char* message, GameInfo* game, int except);
void send_deck_to_all(int cards, GameInfo* game);
void send_to_player(int player, GameInfo* game, char* message);

// responses from user
char* get_valid_bet_from_player(GameInfo* game, char* send, int* pPassed);
void get_message_from_player(char* buffer, GameInfo* game);
Card get_valid_card_from_player(GameInfo* game, int cards);
bool valid_bet(GameInfo* game, char* msg);

// input related
int check_valid_username(char* user, GameInfo* game);
int get_winning_tricks(GameInfo* game);
void deal_cards(Card* kitty, GameInfo* game);
int get_points_from_bet(GameInfo* game);

// rounds - no return values
void send_player_details(GameInfo* game);
void bet_round(GameInfo* game);
void kitty_round(GameInfo* game);
void joker_round(GameInfo* game);
void play_round(GameInfo* game);
void end_round(GameInfo* game);

int main(int argc, char** argv) {

    // check arg count
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: server port password [playertypes]\n");
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

    // case for playertypes given
    if (argc == 4) {

        // ensure string has 4 characters in length
        if (strlen(argv[3]) != 4) {
            fprintf(stderr, "Bad playertypes string\n");
            return 6;

        }

        // check string is valid. each char is more than ASCII_NUMBER_OFFSET
        // and less than BOT_MAX_DIFFICULTY + ASCII_NUMBER_OFFSET
        for (int i = 0; i < 4; i++) {

            if (argv[3][i] < ASCII_NUMBER_OFFSET ||
                    argv[3][i] > ASCII_NUMBER_OFFSET + BOT_MAX_DIFFICULTY) {

                fprintf(stderr, "Bad playertypes string (max bot diff. %d)\n",
                        BOT_MAX_DIFFICULTY);
                return 6;

            }

        }

    }

    // init socket
    socket_init();
    
    // attempt to listen (exits here if fail) and save the fd if successful
    int fdServer = open_listen(port);

    // create game info struct and populate
    GameInfo game;
    game.fd = fdServer;
    game.password = argv[2];
    game.p = 0;
    game.start = 0;
    game.player = malloc(NUM_PLAYERS * sizeof(Player));
    game.teamPoints = malloc(2 * sizeof(int));
    game.teamPoints[0] = 0; // set these values!! 
    game.teamPoints[1] = 0;
    game.kitty = malloc(3 * sizeof(Card));


    // get playertypes string
    if (argc == 4) {
        game.playerTypes = argv[3];

    } else {
        game.playerTypes = "0000"; // default, all players

    }

    // process connections
    process_connections(&game);

}

// main game loop
void game_loop(GameInfo* game) {
    srand(time(NULL)); // random seed

    // loop until game exits (from end_round function or bad read)
    while (1) {
        // shuffle and deal
        fprintf(stdout, "Shuffling and Dealing\n");
        deal_cards(game->kitty, game); // kitty and dealing, debug info too

        // betting round
        fprintf(stdout, "Betting round starting\n");
        bet_round(game);
        fprintf(stdout, "Betting round ending\n");

        // kitty round
        fprintf(stdout, "Dealing kitty\n");
        kitty_round(game);
        fprintf(stdout, "Kitty finished\n");

        // joker suit round. note this only occurs if there are no trumps.
        joker_round(game);

        // play round
        fprintf(stdout, "Game Begins\n");
        play_round(game);

        // end of round actions
        end_round(game);

    }

}

// process connections (from the lecture with my additions)
void process_connections(GameInfo* game) {
    int fileDes;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    char* message = malloc(BUFFER_LENGTH * sizeof(char));

    // loop until we have 4 players waiting to start a game
    while (game->p != NUM_PLAYERS) {

        // check if bot or not
        if (game->playerTypes[game->p] != '0') {

            // add bot stats
            game->player[game->p].name = "bot";
            game->player[game->p].fd = fileDes;
            game->player[game->p].bot = game->playerTypes[game->p] -
                    ASCII_NUMBER_OFFSET;

            // server message
            fprintf(stdout, "Player %d connected (lvl %d bot)\n", game->p,
                    game->player[game->p].bot);

            game->p++;
            continue;

        }

        fromAddrSize = sizeof(struct sockaddr_in);
        // block, waiting for a new connection (fromAddr will be populated
        // with address of the client. Accept

        fileDes = accept(game->fd, (struct sockaddr*)&fromAddr,  &fromAddrSize);

        // check password is the same as ours
        read_from_fd(fileDes, message, MAX_PASS_LENGTH, false);
        if (strcmp(message, game->password) != 0) {

            // otherwise we got illegal input. Send no.
            write_new(fileDes, "no\n", 3);
            close(fileDes);
            continue;

        }

        // password was valid, send yes to client for password
        write_new(fileDes, "yes\n", 4);

        // get user name from client
        read_from_fd(fileDes, message, MAX_NAME_LENGTH, false);

        // check username is valid, i.e. not taken before
        if (check_valid_username(message, game)) {
            // send no to client for user name, and forget this connection
            write_new(fileDes, "no\n", 3);
            close(fileDes);
            continue;
        }

        // send yes to client for user name
        write_new(fileDes, "yes\n", 4);

        // server message
        fprintf(stdout, "Player %d connected with user name '%s'\n",
            game->p, message);

        // add name
        game->player[game->p].name = strdup(message);
        game->player[game->p].fd = fileDes;
        game->player[game->p].bot = 0;
        game->p++;

    }

    // we now have 4 players. send start to all
    send_to_all("start\n", game);

    // we want a yes from all players to ensure they are still here
    for (int i = 0; i < NUM_PLAYERS; i++) {

        // 13 for kitty, only want to malloc this once
        game->player[i].deck = malloc(13 * sizeof(Card)); 

        // bot is always here
        if (game->player[i].bot != 0) {
            continue;

        }

        // read 4 characters, just make sure that it works
        read_from_fd(game->player[i].fd, message, BUFFER_LENGTH, true);
        if (strcmp(message, "yes\n") != 0) {
            send_to_all_except("nostart\n", game, i);
            fprintf(stderr, "Unexpected exit\n");
            exit(5);

        }

    }

    // all players are here, start the game!
    send_to_all("start\n", game);
    free(message);

    // send player details here, we only want to do this once per run
    fprintf(stdout, "Game starting\n");
    send_player_details(game);

    game_loop(game);

}

// sends all players their details, including who's on their team and
// all username and player numbers.
void send_player_details(GameInfo* game) {
    char* message = malloc(BUFFER_LENGTH * sizeof(char));
    // send all players the player details.
    for (int i = 0; i < NUM_PLAYERS; i++) {

        if (game->player[i].bot != 0) {
            continue;

        }

        for (int j = 0; j < NUM_PLAYERS; j++) {

            // string that tells the user if the player is them or a teammate
            char* str = "";
            if (i == j) {
                str = " (You)";

            } else if ((i + 2) % NUM_PLAYERS == j) {
                str = " (Teammate)";

            }

            // print message, note special string for bots
            if (game->player[j].bot == 0) {
                sprintf(message, "Player %d, '%s'%s\n", j,
                        game->player[j].name, str);

            } else {
                sprintf(message, "Player %d, (lvl %d bot)%s\n", j,
                        game->player[j].bot, str);

            }

            send_to_player(i, game, message);

        }

    }

    free(message);

}

// deals cards to the players decks. returns the kitty
void deal_cards(Card* kitty, GameInfo* game) {

    // create deck
    Card* deck = create_deck();

    // reset player properties
    for (int i = 0; i < NUM_PLAYERS; i++) {
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

    // sort each deck by suit here!
    for (int i = 0; i < NUM_PLAYERS; i++) {
        sort_deck(&game->player[i].deck, NUM_ROUNDS, NOTRUMPS, NOTRUMPS);

    }

    // send each player their deck and return the kitty
    send_deck_to_all(NUM_ROUNDS, game);
    free(deck);

}

// handles betting round with all players.
void bet_round(GameInfo* game) {
    // set highest bet info
    game->highestBet = 0;
    game->suit = 0;
    game->betWinner = 0;
    game->p = game->start; // reset player counter (to starting better)
    game->misere = false;
    game->open = false;

    // loop until NUM_PLAYERS have passed
    for (int pPassed = 0; pPassed != NUM_PLAYERS; ) {

        // get bet from player (or bot) if no pass yet
        if (game->player[game->p].hasPassed == false) {
            char* send = malloc(BUFFER_LENGTH * sizeof(char));
            get_valid_bet_from_player(game, send, &pPassed);

            // send the bet to everyone
            fprintf(stdout, "%s", send);
            send_to_all(send, game);

        }


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
    msg[0] = '\0';
    if (game->open == true) {
        sprintf(msg, "Player %d won the bet with openmisere!\n", game->p);
        game->suit = NOTRUMPS; // set suit of gameplay to no trumps

    } else if (game->misere == true) {
        sprintf(msg, "Player %d won the bet with misere!\n", game->p);

    } else {
        sprintf(msg, "Player %d won the bet with %d%c!\n", game->p,
                game->highestBet, return_trump_char(game->suit));

    }

    fprintf(stdout, "%s", msg);
    send_to_all(msg, game);
    free(msg);

}

// handles the kitty round with all players.
// sends the winner of the betting round the 13 cards they have to choose
// from, the player will send the three they don't want and we remove them
void kitty_round(GameInfo* game) {

    // add kitty to the winners hand
    for (int i = 0; i < 3; i++) {
        game->player[game->p].deck[NUM_ROUNDS + i] = game->kitty[i];

    }

    // sort the deck by suit here!
    sort_deck(&game->player[game->betWinner].deck, NUM_ROUNDS + 3,
            game->suit, NOTRUMPS);

    // prepare string to send to the winner and send it
    char* message = malloc(BUFFER_LENGTH * sizeof(char));
    message[0] = '\0';
    sprintf(message, "You won! Pick 3 cards to discard: %s\n",
            return_hand(game->player[game->p].deck, NUM_ROUNDS + 3));
    send_to_player(game->p, game, message);

    // get rid of the extra 3 cards
    if (game->player[game->p].bot == 0) {

        // receive 3 cards the user wants to discard
        for (int c = 0; c != 3; ) {
            // ask for some input
            send_to_player(game->p, game, "send\n");

            // send user a string of how many cards we still need
            message[0] = '\0';
            sprintf(message, "Pick %d more card(s)\n", 3 - c);
            send_to_player(game->p, game, message);

            // get card, and check if valid then remove it from the players deck
            get_message_from_player(message, game);
            Card card = return_card_from_string(message);

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

    } else {

        // remove cards from bots hand
        get_kitty_from_bot(game);

    }

    // send kitty done to all
    send_to_all("kittyend\n", game);
    free(message);

}

// handles choosing the joker suit, if it is no trumps
void joker_round(GameInfo* game) {
    game->jokerSuit = DEFAULT_SUIT; // default joker suit
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));

    // choose joker suit round
    if (game->suit == NOTRUMPS) {
        fprintf(stdout, "Choosing joker suit\n");

        // find which player has the joker. loop through each players deck
        int playerWithJoker = -1;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (deck_has_joker(game->player[i].deck) == true) {
                playerWithJoker = i;
                break;

            }

        }

        // get suit from user, assuming a player has the joker
        while (playerWithJoker != -1) {

            if (game->player[playerWithJoker].bot == 0) {
                // ask user for suit
                send_to_player(playerWithJoker, game, "jokerwant\n");

                // get suit of player with joker
                game->p = playerWithJoker;
                get_message_from_player(msg, game);

                // check length valid
                if (strlen(msg) != 2) {
                    send_to_player(game->p, game, "Bad length\n");
                    continue;

                }

                // check suit valid
                if (return_trump(msg[0]) != DEFAULT_SUIT &&
                        return_trump(msg[0]) != NOTRUMPS) {
                    // set the jokers suit in the game, and we're done
                    game->jokerSuit = return_trump(msg[0]);
                    break;

                }

                send_to_player(game->p, game, "Not a suit\n");

            } else {
                // get joker suit from bot
                get_joker_suit_from_bot(game, playerWithJoker);
                break;

            }

        }

    }

    // send joker suit if it was chosen
    if (game->jokerSuit != DEFAULT_SUIT) {
        // prepare string
        sprintf(msg, "Joker suit chosen. It is a %c\n",
                return_trump_char(game->jokerSuit));
        send_to_all(msg, game);
        fprintf(stdout, "%s", msg);

    }

    send_to_all("jokerdone\n", game);
    free(msg);

}

// handles the play rounds.
void play_round(GameInfo* game) {
    // reset current player
    game->p = game->betWinner;
    char* message = malloc(BUFFER_LENGTH * sizeof(char));

    // loop until 10 rounds have passed
    for (int rounds = 0; rounds != NUM_ROUNDS; ) {

        // send something here to mark start of play
        send_to_all("roundstart\n", game);

        // sort each players deck
        for (int i = 0; i < NUM_PLAYERS; i++) {
            sort_deck(&game->player[i].deck, NUM_ROUNDS - rounds,
                    game->suit, game->jokerSuit);

        }

        // send each player their deck now that it's started
        send_deck_to_all(NUM_ROUNDS - rounds, game);

        // check if the game is open misere
        // if so send all players the bet winners hand
        if (game->open == true) {
            message[0] = '\0';
            sprintf(message, "Player %d's hand: %s\n", game->betWinner,
                    return_hand(game->player[game->betWinner].deck,
                    NUM_ROUNDS - rounds));
            send_to_all_except(message, game, game->betWinner);

        }

        // send round to all
        sprintf(message, "Round %d\n", rounds);
        send_to_all(message, game);

        // winning card
        game->winner.value = 0;
        game->winner.suit = 0;

        // leading trump
        game->lead = DEFAULT_SUIT;

        // Winning player of this round
        game->win = game->p;

        // loop until we get to NUM_PLAYERS plays
        for (int plays = 0; plays != NUM_PLAYERS; ) {

            // check if we are teammate of misere player. if so skip us
            if (game->misere == true &&
                    game->p == (game->betWinner + 2) % NUM_PLAYERS) {

                plays++;
                game->p++;
                game->p %= NUM_PLAYERS;
                continue;

            }

            // current card from player (or bot)
            Card card = get_valid_card_from_player(game, NUM_ROUNDS - rounds);

            // if we are the first to play, our card is automatically winning
            if (plays == 0) {
                game->winner = card;
                game->lead = handle_bower(card, game->suit).suit;

            } else if (compare_cards(card, game->winner, game->suit) == 1) {
                // we have a valid card. check if it's higher than winner
                game->winner = card;
                game->win = game->p;

            }

            // send all the details of the move, who's winning / won.
            if (++plays == NUM_PLAYERS) {
                sprintf(message, "Player %d played %s. Player %d won with %s\n",
                        game->p, return_card(card), game->win,
                        return_card(game->winner));

            } else {
                sprintf(message,
                        "Player %d played %s. Player %d winning with %s\n",
                        game->p, return_card(card), game->win,
                        return_card(game->winner));

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
        game->p = game->win; // winning player plays next
        game->player[game->p].wins++; // increment wins

        // send players how many tricks the winning team has won
        sprintf(message, "Betting team has won %d trick(s)\n",
                get_winning_tricks(game));

        send_to_all(message, game);
        fprintf(stdout, "%s", message);

    }

    // send something here to mark end of play
    send_to_all("gameover\n", game);
    free(message);

}

// handles end of round actions
void end_round(GameInfo* game) {
    // tell the users if the betting team has won or not
    // number of tricks the betting team has won
    char* result; // string literal

    // misere special case first
    if (game->misere == true && get_winning_tricks(game) == 0) {
        // betting team won a misere!
        result = "Betting team won!\n";

        // add points to user depending on misere type
        if (game->open == true) {
            game->teamPoints[game->betWinner % 2] += OPEN_MISERE_POINTS;

        } else {
            game->teamPoints[game->betWinner % 2] += MISERE_POINTS;

        }

    } else if (game->misere == true) {
        // betting team lost a misere!
        result = "Betting team lost!\n";

        // take away points depending on misere type
        if (game->open == true) {
            game->teamPoints[game->betWinner % 2] -= OPEN_MISERE_POINTS;

        } else {
            game->teamPoints[game->betWinner % 2] -= MISERE_POINTS;

        }

    } else if (game->highestBet > get_winning_tricks(game)) {
        result = "Betting team lost!\n";

        // take away points depending on how much they bet
        // points formula from table on rules web page
        game->teamPoints[game->betWinner % 2] -= get_points_from_bet(game);

    } else {
        result = "Betting team won!\n";

        // give points depending on how much they bet
        game->teamPoints[game->betWinner % 2] += get_points_from_bet(game);

    }

    // give opponents 10 points for every trick they win (only if not misere)
    if (game->misere == false) {
        game->teamPoints[(game->betWinner + 1) % 2] +=
                (NUM_ROUNDS - get_winning_tricks(game)) * 10;

    }
    // send the games result
    send_to_all(result, game);
    fprintf(stdout, "%s", result);

    // generate each teams points string
    char** team = malloc(2 * sizeof(char*));
    team[0] = malloc(BUFFER_LENGTH * sizeof(char));
    team[1] = malloc(BUFFER_LENGTH * sizeof(char));

    sprintf(team[0], "%d\n", game->teamPoints[0]);
    sprintf(team[1], "%d\n", game->teamPoints[1]);

    // send each player their and their opponents scores
    for (int i = 0; i < NUM_PLAYERS; i++) {
        send_to_player(i, game, team[i % 2]);
        send_to_player(i, game, team[(i + 1) % 2]);

    }

    fprintf(stdout, "Team 0 points: %d\n", game->teamPoints[0]);
    fprintf(stdout, "Team 1 points: %d\n", game->teamPoints[1]);

    free(team[0]);
    free(team[1]);
    free(team);

    bool over = false;

    // check if a team has won (or lost)
    for (int i = 0; i < 2; i++) {
        if (game->teamPoints[i] >= WIN_POINTS) {
            // team i won
            over = true;

        } else if (game->teamPoints[i] <= -WIN_POINTS) {
            // team i lost lost
            over = true;

        }

    }

    // send message if game is ending or not
    if (over == true) {
        // game is over. exit and tell clients to exit too
        send_to_all("endgame\n", game);
        exit(0);

    } else {
        // otherwise game restarts, change bet clockwise
        send_to_all("gamenotnover\n", game);
        game->start++;
        game->start %= NUM_PLAYERS;

    }

}

// checks if a bet is valid, and sets the new highest bet if it is,
// returns true if it is a valid bet, false otherwise
// note, passing and misere and handled before here
bool valid_bet(GameInfo* game, char* msg) {

    // set the new bet
    int newBet = 0;
    Trump newSuit = 0;

    // read values for case when not 10
    newBet = msg[0] - ASCII_NUMBER_OFFSET;
    newSuit = return_trump(msg[1]);

    // ensure length of message is correct
    if (strlen(msg) != 3) {

        // case for 10 of anything
        if (strlen(msg) == 4 && msg[0] == '1' && msg[1] == '0') {
            newBet = 10;
            newSuit = return_trump(msg[2]);

        } else {
            send_to_player(game->p, game, "Invalid bet\n");
            return false;

        }

    }

    // make sure value and suit are valid
    if (newBet < 6 || newBet > NUM_ROUNDS || newSuit == -1) {
        send_to_player(game->p, game, "Invalid bet\n");
        return false;

    }

    // so the input is valid, now check if the bet is higher
    // than the last one. higher number guarantees a higher bet.
    if (newBet > game->highestBet || (newBet == game->highestBet &&
            newSuit > game->suit)) {

        game->highestBet = newBet;
        game->suit = newSuit;

    } else {
        send_to_player(game->p, game, "Bet not high enough\n");
        return false;

    }

    return true;

}

// loop until we get a valid bet from the user, returns string of this bet
char* get_valid_bet_from_player(GameInfo* game, char* send, int* pPassed) {
    // bet string
    char* msg = malloc(BUFFER_LENGTH * sizeof(char));

    // loop until we get a valid bet
    while (game->player[game->p].hasPassed == false) {

        // send message to player
        send_to_player(game->p, game, "bet\n");

        // get bet from player
        if (game->player[game->p].bot == 0) {
            get_message_from_player(msg, game);

        } else {
            // get bet from bot
            get_bet_from_bot(game);

            // set misere to false if bot bet (they cannot misere)
            if (game->player[game->p].hasPassed == false) {
                game->misere = false;
                game->open = false;

            }

            // we know this will be valid, so break
            break;

        }

        // check if it's a pass
        if (strcmp(msg, "PASS\n") == 0) {
            // set passed to true, change send msg
            game->player[game->p].hasPassed = true;
            break;

        } else if (strcmp(msg, "MISERE\n") == 0) { // misere
            // check misere conditions are met, must be 7 exactly
            if (game->highestBet == 7 && game->misere == false) {
                // it's valid. set bet to be equal to 7 no trumps,
                // that way any 8 can beat this bet
                game->highestBet = 7;
                game->suit = NOTRUMPS;
                game->misere = true;
                sprintf(send, "Player %d bet misere\n", game->p);
                break;

            } else if (game->misere == true) {
                send_to_player(game->p, game,
                        "A player has already bet misere\n");

            } else {
                send_to_player(game->p, game,
                        "Bet must be exactly 7 to bet misere\n");

            }

        } else if (strcmp(msg, "OPENMISERE\n") == 0) { // open misere
            // check misere conditions are met, must be <= 10D
            if ((game->highestBet < 10 || (game->highestBet == 10 &&
                    game->suit <= DIAMONDS)) && game->open == false) {
                // it's valid. set bet to be equal to 10 diamonds,
                // that way 10H or higher can beat it
                game->highestBet = 10;
                game->suit = DIAMONDS;
                game->misere = true;
                game->open = true;
                sprintf(send, "Player %d bet openmisere\n", game->p);
                break;

            } else if (game->open == true) {
                send_to_player(game->p, game,
                        "A player has already bet openmisere\n");

            } else {
                send_to_player(game->p, game, "Bet not high enough\n");

            }

        } else if (valid_bet(game, msg) == true) {
            // update game stats
            game->misere = false;
            game->open = false;
            break;

        }

        // error message is sent in valid_bet method

    }

    free(msg);

    if (game->player[game->p].hasPassed == true) {
        // send string if player or bot passed
        sprintf(send, "Player %d passed\n", game->p);
        (*pPassed)++;

    } else if (game->misere == false) {
        // send string if bot or player makes normal bet
        sprintf(send, "Player %d bet %d%c\n", game->p, game->highestBet,
                return_trump_char(game->suit));

    }

    // assumes bots will not be able to misere

    return send;

}

// loop until we get a valid card from the user or bot, returns this card
Card get_valid_card_from_player(GameInfo* game, int cards) {
    char* buffer = malloc(BUFFER_LENGTH * sizeof(char));

    while (1) {

        Card card;
        // bot and player
        if (game->player[game->p].bot == 0) {
            // tell user to send card
            send_to_player(game->p, game, "send\n");

            // get card from user
            get_message_from_player(buffer, game);
            card = return_card_from_string(buffer);

        } else {
            // get card from bot
            //card = get_card_from_bot(game);
            return get_card_from_bot(game, cards);

        }

        if (card.value == 0) {
            send_to_player(game->p, game, "Not a card\n");
            continue;

        }

        // handle joker here so correct_suit_player works
        if (card.value == JOKER_VALUE && game->suit == NOTRUMPS) {
            card.suit = game->jokerSuit;

        } else if (card.value == JOKER_VALUE) {
            card.suit = game->suit;

        }

        // check that the player doesn't have another card they
        // must be playing, and check they have the card in their deck
        if (correct_suit_player(card, game->player[game->p].deck, game->lead,
                game->suit, cards) == false) {
            send_to_player(game->p, game, "You must play lead suit\n");
            continue;

        } else if (remove_card_from_deck(card,
                &game->player[game->p].deck, cards) == false) {
            send_to_player(game->p, game, "You don't have that card\n");
            continue;

        } else {
            // card is good
            return card;

        }

    }

}

// return a string representing a card read from the current player, doubles up
// as getting bet too. exits if player has left
void get_message_from_player(char* buffer, GameInfo* game) {
    read_from_fd(game->player[game->p].fd, buffer, BUFFER_LENGTH, true);

}

// number of tricks the betting team has won
int get_winning_tricks(GameInfo* game) {
    return game->player[game->betWinner].wins +
        game->player[(game->betWinner + 2) % NUM_PLAYERS].wins;

}

// returns the amount of points from winning the bet
int get_points_from_bet(GameInfo* game) {
    return (4 + game->suit * 2) * 10 + (game->highestBet - 6) * 100;

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
    if (game->player[player].bot == 0) {
        write_new(game->player[player].fd, message, string_length(message));

    }

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
        if (game->player[i].bot == 0 && i != except) {
            send_to_player(i, game, message);

        }

    }

}

// send each player their deck
void send_deck_to_all(int cards, GameInfo* game) {
    // send each player their deck
    char* message = malloc(BUFFER_LENGTH * sizeof(char));
    for (int i = 0; i < NUM_PLAYERS; i++) {        
        if (game->player[i].bot == 0) {
            sprintf(message, "%s\n", return_hand(game->player[i].deck, cards));
            send_to_player(i, game, message);

        }

    }
    free(message);

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
            (char*)&optVal, sizeof(int)) < 0) {
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
