// contains all our card value defines
#include "cards.h"

// returns an array of 43 cards, shuffled.
Card* create_deck() {
    Card* deck = malloc(43 * sizeof(Card));

    // counter
    int card = 0;

    for (int i = 4; i < 15; i++) {
        // deal with case for 4's, we only want red ones.
        if (i != 4) {
            deck[card].value = i;
            deck[card++].suit = SPADES;
            deck[card].value = i;
            deck[card++].suit = CLUBS;

        }

        deck[card].value = i;
        deck[card++].suit = DIAMONDS;
        deck[card].value = i;
        deck[card++].suit = HEARTS;

    }

    // add joker. default to 4
    deck[card].value = JOKER_VALUE;
    deck[card].suit = NOTRUMPS;

    // shuffle
    for (int i = 42; i > 0; i--) {
        swap_cards(deck, i, rand() % (i+1));

    }

    // return this deck
    return deck;

}

// swaps two cards in the given positions in the given deck
void swap_cards(Card* deck, int origpos, int newpos) {
    // save original card
    Card temp = deck[origpos];

    // move the card from newpos to origpos
    deck[origpos] = deck[newpos];

    // move the saved card to newpos
    deck[newpos] = temp;

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
        ret[0] = card.value + ASCII_NUMBER_OFFSET;

    } else {
        switch (card.value) {
            case 10:
                ret[0] = '1';
                ret[1] = '0';
                ret[2] = return_trump_char(card.suit);
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

            default: // invalid card
                ret[0] = '0';
                break;

        }

    }

    // get card suit and return our result
    ret[1] = return_trump_char(card.suit);
    return ret;

}

// given a string representation of a card, this returns a card representing
// that string, or a card with 0 value and 0 suit if unsuccessful
Card return_card_from_string(char* card) {

    // default values
    Card ret;
    ret.value = 0;
    ret.suit = 0;

    // case for JOKER
    if (strcmp(card, "JOKER\n") == 0) {
        ret.value = JOKER_VALUE;
        return ret;

    }

    // case for 10's
    if (strlen(card) == 4 && card[0] == '1' && card[1] == '0') {
        // check valid suit
        if (return_trump(card[2]) != 4 && return_trump(card[2]) != -1) {
            ret.value = 10;
            ret.suit = return_trump(card[2]);

        }

        return ret;

    }

    // we've got rid of all special cases, all the rest are 3 long
    if (strlen(card) != 3) {
        return ret;

    }

    // check valid suit
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

        ret.value = card[0] - ASCII_NUMBER_OFFSET;

    } else {

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
                break;

            default:
                // card invalid, reutrn default
                return ret;

        }

    }

    // value and suit is valid, so set suit and return
    ret.suit = return_trump(card[1]);
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
        case SPADES:
            return 'S';

        case CLUBS:
            return 'C';

        case DIAMONDS:
            return 'D';

        case HEARTS:
            return 'H';

        default: // No trumps
            return 'N';

    }

}

// returns a number representing the trump's character.
Trump return_trump(char trump) {
    switch (trump) {
        case 'S':
            return SPADES;

        case 'C':
            return CLUBS;

        case 'D':
            return DIAMONDS;

        case 'H':
            return HEARTS;

        case 'N':
            return NOTRUMPS;

        default: // Bad input
            return DEFAULT_Suit;

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

// sorts deck by trump, use NOTRUMPS for no suit chosen
void sort_deck(Card** deck, int cards, Trump trump, Trump jokerSuit) {

    // selection sort, O(n^2) is fine for these small sets
    for (int i = 0; i < cards; i++) {

        // index of highest card
        int max = i;
        Card maxCard = (*deck)[max];

        // handle bowers if we need to
        if (trump != NOTRUMPS) {
            maxCard = handle_bower(maxCard, trump);

        }

        // loop until we have the highest card
        for (int j = i; j < cards; j++) {

            Card card = (*deck)[j];

            // handle bowers if we need to
            if (trump != NOTRUMPS) {
                card = handle_bower(card, trump);

            }

            // make sure joker is in the right place
            if (card.value == JOKER_VALUE) {

                // if we're in NOTRUMPS, handle joker differently
                if (trump == NOTRUMPS) {

                    // if trump is notrumps then joker suit is also no trumps
                    // so this works even if suit not chosen
                    card.suit = jokerSuit;

                } else {
                    // we are in a valid suit, set joker to be this suit
                    card.suit = trump;

                }

            }

            // this will sort it fine if we are hearts or no trumps,
            // but we want the trump suit to be sorted to the front
            if (card.suit == trump) {
                // give the card a higher suit than the other trumps!
                card.suit = NOTRUMPS;

            }

            // we did the case i == j to ensure joker is passed on first loop
            // with our special cases
            if (i == j) {
                maxCard = card;
                continue;

            }

            // compare card to the max index, update max if card is higher
            if (card.suit > maxCard.suit) {
                max = j;
                maxCard = card;

            } else if ((card.suit == maxCard.suit) &&
                    (card.value > maxCard.value)) {
                max = j;
                maxCard = card;

            }

        }

        // swap card at max with card at i
        swap_cards(*deck, i, max);

    }

}

// returns true if deck has the joker, false otherwise.
bool deck_has_joker(Card* deck) {
    for (int i = 0; i < 13; i++) {
        if (deck[i].value == JOKER_VALUE) {
            return true;

        }

    }

    return false;

}

// Compares 2 given cards with the current trump, where the second card is
// the one that is currently winning the bet
// return -1 if card a < card b
// return 1 if card a > card b
// return 0 if card a == card b
int compare_cards(Card a, Card b, Trump trump) {
    // no trump case is handled here because the given trump will be the
    // suit of the first card played.

    // first, get rid of case for equal (and joker equal)
    if ((a.value == b.value && a.suit == b.suit) || (a.value == JOKER_VALUE &&
            b.value == JOKER_VALUE)) {
        return 0;

    }

    Card first = a;
    Card second = b;

    // deal with bowers now. only in no trumps
    if (trump != NOTRUMPS) {
        first = handle_bower(a, trump);
        second = handle_bower(b, trump);

    }

    // first, cases where one card is the trump and the other isnt.
    // the other case is when either both or none are trump, and the highest
    // card wins this time.
    if (first.suit == trump && second.suit != trump) {
        return 1;

    } else if (first.suit != trump && second.suit == trump) {
        return -1;

    } else if (first.value > second.value && first.suit == second.suit) {
        // this assumes second is the card that is currently winning the bet
        return 1;

    }

    // otherwise a is smaller
    return -1;

}

// returns false if the deck contains a card of suit lead and card isn't of
// suit lead. returns true otherwise. trump is the games trump
bool correct_suit_player(Card card, Card* deck, Trump lead, Trump trump,
        int rounds) {
    // if they are playing a card from the leading suit, it's fine
    if (handle_bower(card, trump).suit == lead) {
        return true;

    }

    // the only other valid case is if the user has no cards of leading suit
    for (int i = 0; i < rounds; i++) {
        // if we have a card of the leading suit that we aren't playing
        // return false. also change it into the bower if necessary
        if (handle_bower(deck[i], trump).suit == lead) {
            return false;

        }

    }

    // otherwise it's valid
    return true;

}


// changes the value and suit of the card if it is a left or right bower.
Card handle_bower(Card card, Trump trump) {
    Card ret;
    ret.value = card.value;
    ret.suit = card.suit;

    // must be a jack
    if (ret.value != JACK_VALUE) {
        return ret;

    }

    // check for right bower
    if (ret.suit == trump) {
        ret.value = RIGHT_BOWER_VALUE;
        return ret;
    }

    // if ret.suit black and trump black, set to left bower value and suit
    if ((ret.suit == SPADES || ret.suit == CLUBS) &&
            (trump == SPADES || trump == CLUBS)) {
        ret.value = LEFT_BOWER_VALUE;
        ret.suit = trump;

    }

    // same with red jacks
    if ((ret.suit == DIAMONDS || ret.suit == HEARTS) &&
            (trump == DIAMONDS || trump == HEARTS)) {
        ret.value = LEFT_BOWER_VALUE;
        ret.suit = trump;

    }

    return ret;

}
