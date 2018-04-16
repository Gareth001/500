#include "bot.h"

// updates GameInfo with the bots bet
void get_bet_from_bot(GameInfo* game) {
    
    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            // always pass 
            game->player[game->p].hasPassed = true;
            break;
       
        case 2:
            // the max bet and suite of max bet
            ; // magic semicolon
            //int maxBet;
            //Trump maxSuite;
            
            // count how many estimated tricks we can win of each suit
            // estimated number  = 1 + number of cards of this suit
            // + number of Kings or higher of other suits
            int* suitCount = malloc(4 * sizeof(Card));
            
            // loop through each suite
            for (Trump i = 0; i < 4; i++) {
                // begin count at 1, assumes 1 from kitty
                suitCount[i] = 1;

                // loop through each card, check how many we have of this
                for (int k = 0; k < NUM_ROUNDS; k++) {
                
                    // handle joker safely
                    if (game->player[game->p].deck[k].value == JOKER_VALUE) {
                        
                        suitCount[i]++;
                        continue;
                        
                    }
                    
                    Card c = handle_bower(game->player[game->p].deck[k], i); 

                    // add 1 to count if we are of this trump
                    if (c.suit == i) {
                        suitCount[i]++;
                                                
                    } else if (c.value >= KING_VALUE){
                        
                        // add 1 to count if we have a king or higher value
                        suitCount[i]++;
                        
                    }
                    
                }
                
            }
            
            // choose the largest suite to bet
            // check if it's >= to 6
            // check if it's >= to highest bet
            // pass if it's not, otherwise this is out bet
            
            break;
        
    }
    
    /* note any non passing bet must contain
    game->misere = false;
    game->open = false;
    */
    
}

// updates GameInfo by removing the bots extra 3 cards
void get_kitty_from_bot(GameInfo* game) {
    
    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            // lvl 1 bot cannot win bet
            break;
       
        
    }
    
}

// updates GameInfo by setting joker suit
void get_joker_suit_from_bot(GameInfo* game) {

    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            // spades by default 
            game->jokerSuit = SPADES;
            break;
        
        
    }

    
}

// bot is leading
// see get_card_from_bot for argument details
Card bot_leading(GameInfo* game, Card** deckSplit, int* suitCount) {
    Card card;

    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            
            // find the first highest card of a suit we have
            for (Trump i = 0; i < 4; i++) {
                if (suitCount[i] > 0) {
                    card = deckSplit[i][0];
                    break;
                    
                }
                
            }
            
            break;
            
    }

    return card;
   
}

// bot must play lead suit
Card bot_must_play_lead_suit(GameInfo* game, Card* possibleCards, int count) {
    Card card;

    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
        
            // play highest lead suit card
            card = possibleCards[0];
            
            break;
            
    }

    return card;
    
}

// bot can short suit
Card bot_can_short_suit(GameInfo* game, Card** deckSplit, int* suitCount) {
    Card card;
    
    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
        
            // play highest of trump
            card = deckSplit[game->suit][0];
            
            break;

    }
    
    return card;
}

// bot has no trumps or can't beat the led suit - must get rid of a small card
Card bot_no_trumps_or_lead(GameInfo* game, Card** deckSplit, int* suitCount) {
    Card card;
    
    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:

            // none of our cards are high enough
            // discard the first lowest card of a suit
            for (Trump i = 0; i < 4; i++) {
                if (suitCount[i] > 0) {
                    card = deckSplit[i][suitCount[i - 1]];
                    break;
                               
                }
                
            }
            
            break;
            
    }
    
    return card;

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
    
    // populate deckSplit
    for (int k = 0; k < cards; k++) {
        
        // move joker safely
        if (game->player[game->p].deck[k].value == JOKER_VALUE) {
            
            // handle joker here so that we can remove this card later
            if (game->suit == NOTRUMPS) {
                
                deckSplit[game->jokerSuit][suitCount[game->jokerSuit]++] =
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
    
    // choosing a card - split functionality into 4 cases
    if (lead == DEFAULT_Suit) {
        card = bot_leading(game, deckSplit, suitCount);
                
    } else if (suitCount[lead] > 0) {
        card = bot_must_play_lead_suit(game, deckSplit[lead], suitCount[lead]);
                
    } else if (lead != game->suit && suitCount[game->suit] > 0) {
        card = bot_can_short_suit(game, deckSplit, suitCount);
            
    } else {
        card = bot_no_trumps_or_lead(game, deckSplit, suitCount);
        
    }
            
    // remove this card from deck
    remove_card_from_deck(card, &game->player[game->p].deck, cards);
    
    // change joker suit in no trumps
    // note we must do this after we remove otherwise it's a different card
    if (card.value == JOKER_VALUE && game->suit == NOTRUMPS) {
        card.suit = game->jokerSuit;

    } else if (card.value == JOKER_VALUE) {
        card.suit = game->suit;

    }
    
    return card;
    
}

