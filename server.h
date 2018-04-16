#ifndef SERVER_H
#define SERVER_H

#include "cards.h"

// server specific defines
#define NUM_ROUNDS 10
#define NUM_PLAYERS 4

// stores player info
typedef struct Player {

    Card* deck; // players hand
    char* name;
    int fd;
    bool hasPassed;
    int wins; // number of tricks this player has won
    int bot; // 0 if human

} Player;

// stores game info
typedef struct GameInfo {

    // player and card array
    Player* player;
    Card* kitty;

    int highestBet; // highest bet number
    Trump suit; // highest bet suit
    int betWinner; // person who won the betting round (and did the kitty)
    Trump jokerSuit; // suit of the joker for no trumps
    bool misere; // true if misere
    bool open; // true if open misere

    int* teamPoints; // points for each team, index 0 is player 0's team,
    // 1 is player 1's team

    int start; // betting player
    int p; // current player selected

    char* password;
    char* playerTypes; // string of playertypes
    unsigned int timeout; // currently unimplemented
    int fd; // file descriptor of server

} GameInfo;

#endif /* SERVER_H */