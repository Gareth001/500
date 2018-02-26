#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"

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
    
    // add joker. Default to 4 for now, this will mean something is wrong later
    deck[j].value = 15;
    deck[j].suite = 4;
    
    // shuffle here
    
    // return this deck
    return deck;
    
}

// returns a human readable representation of the card
char* return_card(Card* card) {
    
    // get rid of joker case first
    if (card->value == 15) {
        return "JO";
    }
    
    char* ret = malloc(2 * sizeof(char));
     
    // get card value
    if (card->value <= 9) {
        ret[0] = card->value + 48;
        
    } else if (card->value == 10) {
        ret[0] = 48;
        
    } else {
        switch (card->value) {
            case 11:
                ret[0] = 'J';
                break;
            case 12:
                ret[0] = 'Q';
                break;
            case 13:
                ret[0] = 'K';
                break;
            case 14:
                ret[0] = 'A';
                break;

        }
        
    }

    // get card suite and return our result
    ret[1] = return_trump_char(card->suite);
    return ret;
    
}

// given a string representation of a card, this returns a card representing 
// that string, or a card with 0 value and 0 suite if unsuccessful 
Card return_card_from_string(char* card) {
    
    // default values
    Card ret;
    ret.value = 0;
    ret.suite = 0;
    
    // ensure valid length
    if (strlen(card) != 3) {
        return ret;
        
    }
    
    // case for joker first
    if (strcmp(card, "JO\n") == 0) {
        ret.value = 15;
        return ret;
        
    }
    
    // check valid suite
    if (return_trump(card[1]) == 4 || return_trump(card[1]) == -1) {
        return ret;
    }
    
    // check valid value
    if (card[0] < '9' && card[0] > '3') {
        
        // case for black 4's not existing
        if (card[0] == '4' && (return_trump(card[1]) == 0 ||
                return_trump(card[1]) == 1)) {
            return ret;
        } 
        
        ret.value = card[0] - 48;
        
    } else if (card[0] == '0' || card[0] == 'J' || card[0] == 'Q' ||
            card[0] == 'K' || card[0] == 'A') {
                
        // special cases beside the joker
        switch (card[0]) {
            case '0':
                ret.value = 10;
                break;
            case 'J':
                ret.value = 11;
                break;
            case 'Q':
                ret.value = 12;
                break;
            case 'K':
                ret.value = 13;
                break;
            case 'A':
                ret.value = 14;
            
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
        strcat(message, return_card(&cards[i]));
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

// checks if a bet is valid, and sets the new highest bet if it is, 
// returns 0 if it is a valid bet, 1 otherwise
int valid_bet(int* highestBet, int* suite, char* msg) {
    
    // ensure length of message is correct
    if (strlen(msg) != 3) {
        return 1;
    }
    
    int newBet = 0;
    Trump newSuite = 0;
                    
    // read value, with special case '0' = 10
    newBet = (msg[0] == '0') ? 10 : msg[0] - 48;
                        
    // make sure value is valid
    if (newBet < 6 || newBet > 10) {
        return 1;
    }
    
    newSuite = return_trump(msg[1]);
    
    // check for default case from return_trump
    if (newSuite == -1) {
        return 1;
    }

    // so the input is valid, now check if the bet is higher 
    // than the last one. higher number guarantees a higher bet.
    if (newBet > *highestBet || (newBet == *highestBet &&
            newSuite > *suite)) {
    
        *highestBet = newBet;
        *suite = newSuite;
                
    } else {
        return 1;
        
    }
    
    return 0;
    
}

// Compares 2 given cards with the current trump.
// return -1 if card a < card b 
// return 1 if card a > card b
int compare_cards(Card* a, Card* b, Trump trump) {
    // no trump case is handled here because the given trump will be the 
    // suite of the first card played.
    
    // first, cases where one card is the trump and the other isnt.
    // the other case is when either both or none are trump, and the highest 
    // card wins this time.
    if (a->suite == trump && b->suite != trump) {
        return 1;
        
    } else if (a->suite != trump && b->suite == trump) {
        return -1; 
        
    } else if (a->value > b->value) {
        return 1;
        
    } else {
        return -1; 
        
    }
    
}