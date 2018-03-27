#ifndef CARDS_H 
#define CARDS_H 

// include packages 
#include "shared.h"

// picture card values
#define JACK_VALUE 11
#define QUEEN_VALUE 12
#define KING_VALUE 13
#define ACE_VALUE 14
#define LEFT_BOWER_VALUE 15 // jack of trump colour
#define RIGHT_BOWER_VALUE 16 // jack of trump suite
#define JOKER_VALUE 17

// suite definitions
#define SPADES 0
#define CLUBS 1
#define DIAMONDS 2
#define HEARTS 3
#define NOTRUMPS 4
#define DEFAULT_SUITE -1

// points for winning various bets
#define OPEN_MISERE_POINTS 500
#define MISERE_POINTS 250

// cards. We store a value (4, 7, king, ace etc) and a suite (spades etc).
// In the case of the joker, the value is set to 15 and the suite is set as 
// the trump in the case of a normal game and the chosen suite if no trumps is
// chosen.

// Suite is the same as Trump below except that the Trump of a card cannot be
// no trumps.

// card struct, contains the suite
typedef struct Card {
    int value;
    int suite;
    
} Card;

// trumps. see suite defeinitions above
typedef int Trump;


// returns a human readable representation of the card
char* return_card(Card card);

// returns a character representing the trump.
char return_trump_char(Trump trump);

// returns a number representing the trump's character.
Trump return_trump(char trump);

// returns a string representation of a users hand, each card seperated
// by a single space
char* return_hand(Card* cards, int num);

// compares 2 given cards with the current trump.
// return -1 if card a < card b 
// return 1 if card a > card b
// return 0 if card a == card b
int compare_cards(Card a, Card b, Trump trump);

// returns an array of 43 cards, shuffled.
Card* create_deck();

// checks if a bet is valid, and sets the new highest bet if it is, 
// returns true if it is a valid bet, false otherwise
bool valid_bet(int* highestBet, int* suite, char* msg);

// given a string representation of a card, this returns a card representing 
// that string, or a card with 0 value and 0 suite if unsuccessful 
Card return_card_from_string(char* card);

// removes the given card from the given deck, with given number of cards.
// returns true if successful, false otherwise
bool remove_card_from_deck(Card card, Card** deck, int cards);

// returns true if deck has the joker, false otherwise.
bool deck_has_joker(Card* deck);

// returns a new card with the changed value and suite of
// the card if it is a left or right bower.
Card handle_bower(Card card, Trump trump);

// returns false if the deck contains a card of suite lead and card isn't of 
// suite lead. returns true otherwise. trump is the games trump
bool correct_suite_player(Card card, Card* deck, Trump lead, Trump trump,
        int rounds);

// swaps two cards in the given positions in the given deck
void swap_cards(Card* deck, int origpos, int newpos);

// sorts deck by trump, use NOTRUMPS for no suite chosen
void sort_deck(Card** deck, int cards, Trump trump, Trump jokerSuite);

#endif /* CARDS_H */ 
