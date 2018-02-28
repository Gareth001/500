#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"

#define JOKER_VALUE 17
#define JACK_VALUE 11
#define QUEEN_VALUE 12
#define KING_VALUE 13
#define ACE_VALUE 14
#define LEFT_BOWER_VALUE 15 // jack of trump colour
#define RIGHT_BOWER_VALUE 16 // jack of trump suite

// returns an array of 43 cards, not shuffled.
Card* create_deck() {
    Card* deck = malloc(43 * sizeof(Card));
    
    // counter 
    int j = 0;
    
    for (int i = 4; i < 15; i++) {
        // deal with case for 4's, we only want red ones.
        if (i != 4) {
            deck[j].value = i;
            deck[j++].suite = 0; // spades
            deck[j].value = i;
            deck[j++].suite = 1; // clubs
            
        }
        
        deck[j].value = i;
        deck[j++].suite = 2; // diamonds
        deck[j].value = i;
        deck[j++].suite = 3; // hearts
        
    }
    
    // add joker. default to 4
    deck[j].value = JOKER_VALUE;
    deck[j].suite = 4;
    
    // shuffle here
    
    // return this deck
    return deck;
    
}

// returns a human readable representation of the card
char* return_card(Card card) {
    
    // get rid of joker case first
    if (card.value == JOKER_VALUE) {
        return "JOKER";
    }
    
    char* ret = malloc(3 * sizeof(char));
     
    // get card value
    if (card.value <= 9) {
        ret[0] = card.value + 48;
        
    } else {
        switch (card.value) {
            case 10:
                ret[0] = '1';
                ret[1] = '0';
                ret[2] = return_trump_char(card.suite);
                return ret;
                
            case JACK_VALUE:
                ret[0] = 'J';
                break;
                
            case QUEEN_VALUE:
                ret[0] = 'Q';
                break;
                
            case KING_VALUE:
                ret[0] = 'K';
                break;
                
            case ACE_VALUE:
                ret[0] = 'A';
                break;

        }
        
    }

    // get card suite and return our result
    ret[1] = return_trump_char(card.suite);
    return ret;
    
}

// given a string representation of a card, this returns a card representing 
// that string, or a card with 0 value and 0 suite if unsuccessful 
Card return_card_from_string(char* card) {
    
    // default values
    Card ret;
    ret.value = 0;
    ret.suite = 0;
    
    // ensure valid length and get rid of special cases
    if (strlen(card) != 3) {
        
        // case for JOKER
        if (strcmp(card, "JOKER\n") == 0) {
            ret.value = JOKER_VALUE;
            return ret;
        
        }

        // case for 10's
        if (strlen(card) == 4 && card[0] == '1' && card[1] == '0') {
            // check valid suite
            if (return_trump(card[2]) == 4 || return_trump(card[2]) == -1) {
                return ret;
                
            }
            
            ret.value = 10;
            ret.suite = return_trump(card[2]);
            
        }
        
        return ret;
        
    }
    
    
    // check valid suite
    if (return_trump(card[1]) == 4 || return_trump(card[1]) == -1) {
        return ret;
    }
    
    // check valid value
    if (card[0] <= '9' && card[0] > '3') {
        
        // case for black 4's not existing
        if (card[0] == '4' && (return_trump(card[1]) == 0 ||
                return_trump(card[1]) == 1)) {
            return ret;
        } 
        
        ret.value = card[0] - 48;
        
    } else if (card[0] == 'J' || card[0] == 'Q' ||
            card[0] == 'K' || card[0] == 'A') {
                
        // special cases beside the joker
        switch (card[0]) {
            case 'J':
                ret.value = JACK_VALUE;
                break;
                
            case 'Q':
                ret.value = QUEEN_VALUE;
                break;
                
            case 'K':
                ret.value = KING_VALUE;
                break;
                
            case 'A':
                ret.value = ACE_VALUE;
            
        }        
                
    } else {
        // otherwise it's not valid
        return ret;
        
    }

    // value and suite is valid, so set suite and return
    ret.suite = return_trump(card[1]);
    return ret;
    
}

// returns a string representation of a users hand, each card seperated
// by a single space
char* return_hand(Card* cards, int num) {
    
    char* message = malloc(num * 3 * sizeof(char));
    
    for (int i = 0; i < num; i++) {
        strcat(message, return_card(cards[i]));
        if (i < num - 1) {
            strcat(message, " ");
            
        }
        
    }
        
    return message;
    
}

// returns a character representing the trump.
char return_trump_char(Trump trump) {
    switch (trump) {
        case 0: // Spades
            return 'S'; 
        case 1: // Clubs
            return 'C';
        case 2: // Diamonds
            return 'D';
        case 3: // Hearts
            return 'H';
        default: // No trumps
            return 'N';

    }
    
}

// returns a number representing the trump's character.
Trump return_trump(char trump) {
    switch (trump) {
        case 'S': // Spades
            return 0; 
        case 'C': // Clubs
            return 1;
        case 'D': // Diamonds
            return 2;
        case 'H': // Hearts
            return 3;
        case 'N': // No trumps
            return 4;
        default: // Bad input
            return -1;

    }
    
}

// removes the given card from the given deck. Returns true if successful, 
// false otherwise
bool remove_card_from_deck(Card card, Card** deck, int cards) {    
    for (int i = 0; i < cards; i++) {
        if (compare_cards(card, (*deck)[i], 0) == 0) {
            // cards are equal - remove this card from the deck
            for (int j = i + 1; j < cards; j++) {
                (*deck)[i] = (*deck)[j];
                
            }
            
            // removal success
            return true;
            
        }
        
    }
    
    return false;
    
}

// checks if a bet is valid, and sets the new highest bet if it is, 
// returns true if it is a valid bet, false otherwise
bool valid_bet(int* highestBet, int* suite, char* msg) {
    
    // set the new bet
    int newBet = 0;
    Trump newSuite = 0;
    
    // read values for case when not 10
    newBet = msg[0] - 48;
    newSuite = return_trump(msg[1]);
    
    // ensure length of message is correct
    if (strlen(msg) != 3) {
        
        // case for 10 of anything
        if (strlen(msg) == 4 && msg[0] == '1' && msg[1] == '0') {
            newBet = 10;
            newSuite = return_trump(msg[2]);
            
        } else {
            return false;
            
        }
        
    }
                        
    // make sure value is valid
    if (newBet < 6 || newBet > 10) {
        return false;
    }
        
    // check for default case from return_trump
    if (newSuite == -1) {
        return false;
    }

    // so the input is valid, now check if the bet is higher 
    // than the last one. higher number guarantees a higher bet.
    if (newBet > *highestBet || (newBet == *highestBet &&
            newSuite > *suite)) {
    
        *highestBet = newBet;
        *suite = newSuite;
                
    } else {
        return false;
        
    }
    
    return true;
    
}

// Compares 2 given cards with the current trump, where the second card is
// the one that is currently winning the bet
// return -1 if card a < card b 
// return 1 if card a > card b
// return 0 if card a == card b
int compare_cards(Card a, Card b, Trump trump) {
    // no trump case is handled here because the given trump will be the 
    // suite of the first card played.
    
    // first, get rid of case for equal (and joker equal)
    if ((a.value == b.value && a.suite == b.suite) || (a.value == JOKER_VALUE && 
            b.value == JOKER_VALUE)) {
        return 0;
        
    }
    
    // deal with bowers now
    
    
    // first, cases where one card is the trump and the other isnt.
    // the other case is when either both or none are trump, and the highest 
    // card wins this time.
    if (a.suite == trump && b.suite != trump) {
        return 1;
        
    } else if (a.suite != trump && b.suite == trump) {
        return -1; 
        
    } else if (a.value > b.value && a.suite == b.suite) {
        // this assumes b is the card that is currently winning the bet
        return 1;
        
    } else {
        return -1; 
        
    }
    
}