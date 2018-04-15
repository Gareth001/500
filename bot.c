#include "bot.h"

// updates GameInfo with the bots bet
void get_bet_from_bot(GameInfo* game) {
    
    // pass by default for now
    game->player[game->p].hasPassed = true;

    
    /* any non passing bet must contain
    game->misere = false;
    game->open = false;
    */
    
}

// updates GameInfo by removing the bots extra 3 cards
void get_kitty_from_bot(GameInfo* game) {
    
    // remove 3 cards
    
}

// updates GameInfo by setting joker suit
void get_joker_suit_from_bot(GameInfo* game) {
    
    // spades by default 
    game->jokerSuite = SPADES;
    
}

// returns card that this bot has played
Card get_card_from_bot(GameInfo* game, Trump lead, int cards) {
    
    // we note that our deck is sorted already, split deck into suits
    
    // malloc deckSplit
    Card** deckSplit = malloc(4 * sizeof(Card*));
    
    // store how many in each deckSplit
    int* suitCount = malloc(4 * sizeof(Card));

    for (Trump i = 0; i < 4; i++) {
        deckSplit[i] = malloc(cards * sizeof(Card));
        suitCount[i] = 0;
        
    }
    
    for (int k = 0; k < cards; k++) {
        
        // move joker safely
        if (game->player[game->p].deck[k].value == JOKER_VALUE) {
            
            // handle joker here so correct_suit_player works
            if (game->suit == NOTRUMPS) {
                
                deckSplit[game->jokerSuite][suitCount[game->jokerSuite]++] =
                        game->player[game->p].deck[k];
                
            } else {
                
                deckSplit[game->suit][suitCount[game->suit]++] =
                        game->player[game->p].deck[k];

            }

            continue;
            
        }
               
        Card c = handle_bower(game->player[game->p].deck[k], game->suit); 
        deckSplit[c.suit][suitCount[c.suit]++] = game->player[game->p].deck[k];
        
    }
    
    // card to return
    Card card;
    
    // choosing a card
    if (lead == DEFAULT_SUITE) {
        
        // we are leading
        // leading with our first card of a suit
        for (Trump i = 0; i < 4; i++) {
            if (suitCount[i] > 0) {
                card = deckSplit[i][0];
                break;
                
            }
            
        }
        
    } else if (suitCount[lead] > 0) {
        
        // must play lead suit
        // play highest lead suit card
        card = deckSplit[lead][0];
        
    } else if (lead != game->suit && suitCount[game->suit] > 0) {
            
        // we can short suit
        // play highest of trump
        card = deckSplit[game->suit][0];
            
    } else {
        
        // none of our cards are high enough
        // discard the first lowest card of a suit
        for (Trump i = 0; i < 4; i++) {
            if (suitCount[i] > 0) {
                card = deckSplit[i][suitCount[i - 1]];
                break;
                
            }
            
        }
        
    }
            
    // remove this card from deck
    remove_card_from_deck(card, &game->player[game->p].deck, cards);
    
    // change joker suite in no trumps
    if (card.value == JOKER_VALUE && game->suit == NOTRUMPS) {
        card.suit = game->jokerSuite;

    } else if (card.value == JOKER_VALUE) {
        card.suit = game->suit;

    }
    
    return card;
    
}
