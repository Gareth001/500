// see server for dependency graph
#include "server.h"

// max bot difficulty
#define BOT_MAX_DIFFICULTY 2

// updates GameInfo with the bots bet
void get_bet_from_bot(GameInfo* game);

// updates GameInfo by removing the bots extra 3 cards
void get_kitty_from_bot(GameInfo* game);

// updates GameInfo by setting joker suit
void get_joker_suit_from_bot(GameInfo* game, int playerWithJoker);

// updates GameInfo by setting joker suit
Card get_card_from_bot(GameInfo* game, int cards);