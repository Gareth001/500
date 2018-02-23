#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

        }
        
    }

    // get card suite and return our result
    ret[1] = return_trump_char(card->suite);
    return ret;
    
}

// returns a string representation of a users hand, each card seperated
// by a single space
char* return_hand(Card* cards) {
    // VERY temporary
    char* message = malloc(20 * sizeof(char));
        
    sprintf(message, "%s %s %s %s %s %s %s %s %s %s", return_card(&cards[0]),
            return_card(&cards[1]), return_card(&cards[2]), return_card(&cards[3]),
            return_card(&cards[4]), return_card(&cards[5]), return_card(&cards[6]),
            return_card(&cards[7]), return_card(&cards[8]), return_card(&cards[9])
            );
            
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