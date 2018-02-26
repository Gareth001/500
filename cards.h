#ifndef CARDS_H 
#define CARDS_H 

// Cards. We store a value (4, 7, king, ace etc) and a suite (spades etc).
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

// Trumps. an integer between 0 and 4
// Spades 0
// Clubs 1
// Diamonds 2
// Hearts 3
// No Trumps 4

typedef int Trump;


// returns a human readable representation of the card
char* return_card(Card* card);

// returns a character representing the trump.
char return_trump_char(Trump trump);

// returns a number representing the trump's character.
Trump return_trump(char trump);

// returns a string representation of a users hand, each card seperated
// by a single space
char* return_hand(Card* cards, int num);

// Compares 2 given cards with the current trump.
// return -1 if card a < card b 
// return 1 if card a > card b
int compare_cards(Card* a, Card* b, Trump trump);

// returns an array of 43 cards, not shuffled.
Card* create_deck();

// checks if a bet is valid, and sets the new highest bet if it is, 
// returns 0 if it is a valid bet, 1 otherwise
int valid_bet(int* highestBet, int* suite, char* msg);

// given a string representation of a card, this returns a card representing 
// that string, or a card with 0 value and 0 suite if unsuccessful 
Card return_card_from_string(char* card);

#endif /* CARDS_H */ 
