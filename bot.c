#include "bot.h"

// populate split deck and suitCount
void split_deck(GameInfo* game, Card*** deckSplit, int** suitCount, int cards);

// returns the lowest non trump card (lowest trump if only trumps)
Card get_lowest_non_trump_card(GameInfo* game, int cards);

// given suitCount, gives you total cards.
int sum_suitCount(int* suitCount);

// gets this players highest bet (for difficulty 2 bot)
void generate_max_bet(GameInfo* game, int* maxBet, Trump* maxSuit);

// updates GameInfo with the bots bet
void get_bet_from_bot(GameInfo* game) {

    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            // always pass
            game->player[game->p].hasPassed = true;
            break;

        case 2:
            ; // magic semicolon
            // the max bet and suite of max bet
            int maxBet = 0;
            Trump maxSuit = 0;

            // get the highest bet we should do
            generate_max_bet(game, &maxBet, &maxSuit);

            // pass if this bet is < 6 or smaller than highest bet
            if (maxBet < 6 || maxBet < game->highestBet ||
                    (maxBet == game->highestBet && maxSuit <= game->suit)) {
                game->player[game->p].hasPassed = true;
                break;

            }

            // check if we are lead bet, then bet 6 of our max suit
            if (game->highestBet == 0) {
                game->highestBet = 6;
                game->suit = maxSuit;
                break;

            }

            // check if we are the only one still betting
            bool lastBetting = true;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (i != game->p && game->player[i].hasPassed == false) {
                    lastBetting = false;
                    break;

                }

            }

            // pass if we are last betting
            if (lastBetting == true) {
                game->player[game->p].hasPassed = true;
                break;

            }

            // otherwise bet the smallest larger bet
            if (maxSuit > game->suit) {
                // game highest bet remains unchanged, just the suit changes
                game->suit = maxSuit;

            } else {
                // bet 1 higher and change suit
                game->highestBet++;
                game->suit = maxSuit;

            }

            break;

    }

}

// updates GameInfo by removing the bots extra 3 cards
void get_kitty_from_bot(GameInfo* game) {

    // act differently based on bot difficulty
    switch (game->player[game->p].bot) {
        case 1:
            // lvl 1 bot cannot win bet
            break;

        case 2:
            // we have 13 cards currently, we need to remove 3

            for (int i = 0; i < 3; i++) {

                // choose card
                Card card = get_lowest_non_trump_card(game,
                        NUM_ROUNDS + 3 - i);

                // remove this card
                remove_card_from_deck(card,
                        &game->player[game->p].deck, NUM_ROUNDS + 3 - i);

                // sort the deck by suit
                sort_deck(&game->player[game->p].deck, NUM_ROUNDS + 3 - i,
                        game->suit, NOTRUMPS);

                // note that if the player has more than 10 trumps,
                // card will equal 0S and then remove card will not move but
                // since deck is sorted, this will just remove the rightmost
                // card until there are 10 cards, which is the lowest few trumps

            }

            break;

    }

}

// updates GameInfo by setting joker suit
void get_joker_suit_from_bot(GameInfo* game, int playerWithJoker) {

    // act differently based on bot difficulty
    switch (game->player[playerWithJoker].bot) {
        case 1:
            // spades by default
            game->jokerSuit = SPADES;
            break;

        case 2:
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

        case 2:
            ; // magic semicolon
            // play our highest card!
            card.suit = 0;
            card.value = 0;

            // find the highest card we have
            for (Trump i = 0; i < 4; i++) {
                if (suitCount[i] > 0 && handle_bower(deckSplit[i][0],
                        game->suit).value >=
                        handle_bower(card, game->suit).value) {

                    // compare this card to the current card
                    card = deckSplit[i][0];

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

        case 2:

            // check if teammate is winning
            if (game->p == (game->win + 2) % NUM_PLAYERS) {

                // we don't want to play on top of them if they are winning
                // play lowest of lead
                card = possibleCards[count - 1];
                break;

            }

            // check if we can beat the winning card
            if (possibleCards[0].value > game->winner.value) {

                // play our highest card to secure the win
                card = possibleCards[0];

                // will want to do this differently if we are the last to play

            } else {

                // otherwise play lowest of lead
                card = possibleCards[count - 1];

            }

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

        case 2:

            // check if teammate is winning
            if (game->p == (game->win + 2) % NUM_PLAYERS) {

                // we don't want to play on top of them if they are winning
                // play lowest possible card
                card = get_lowest_non_trump_card(game, sum_suitCount(suitCount));
                break;

            }

            // otherwise we want to trump it

            // check if someone else has trumped it
            if (game->winner.suit == game->suit) {

                // check if we can beat the winner
                if (deckSplit[game->suit][0].value < game->winner.value) {

                    // play lowest possible card
                    card = get_lowest_non_trump_card(game,
                            sum_suitCount(suitCount));

                    break;

                }

                // play smallest card we have higher than winner
                for (int i = suitCount[game->suit] - 1; i > 0; i--) {

                    if (deckSplit[game->suit][i].value > game->winner.value) {
                        card = deckSplit[game->suit][i];
                        break;

                    }

                }

            } else {

                // play smallest trump
                card = deckSplit[game->suit][suitCount[game->suit] - 1];

            }

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
                    card = deckSplit[i][suitCount[i] - 1];
                    break;

                }

            }

            break;

        case 2:
            ; // magic semicolon

            // choose lowest non trump card
            card = get_lowest_non_trump_card(game, sum_suitCount(suitCount));

            break;

    }

    return card;

}

// returns card that this bot has played
Card get_card_from_bot(GameInfo* game, int cards) {

    // we note that our deck is sorted already, split deck into suits
    // malloc deckSplit and suitCount
    Card** deckSplit = malloc(4 * sizeof(Card*));
    int* suitCount = malloc(4 * sizeof(Card));

    // populate split deck and suitCount
    split_deck(game, &deckSplit, &suitCount, cards);

    // card to return
    Card card;

    // choosing a card - split functionality into 4 cases
    if (game->lead == DEFAULT_SUIT) {
        card = bot_leading(game, deckSplit, suitCount);

    } else if (suitCount[game->lead] > 0) {
        card = bot_must_play_lead_suit(game,
                deckSplit[game->lead], suitCount[game->lead]);

    } else if (game->lead != game->suit && game->suit != NOTRUMPS
            && suitCount[game->suit] > 0) {
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

    free(deckSplit);
    free(suitCount);
    
    return card;

}

// populate split deck and suitCount - splits deck by suits
void split_deck(GameInfo* game, Card*** deckSplit, int** suitCount, int cards) {

    for (Trump i = 0; i < 4; i++) {
        (*deckSplit)[i] = malloc(cards * sizeof(Card));
        (*suitCount)[i] = 0;

    }

    // populate deckSplit
    for (int k = 0; k < cards; k++) {

        // move joker safely
        if (game->player[game->p].deck[k].value == JOKER_VALUE) {

            // handle joker here so that we can remove this card later
            if (game->suit == NOTRUMPS) {

                (*deckSplit)[game->jokerSuit][(*suitCount)[game->jokerSuit]++] =
                        game->player[game->p].deck[k];

            } else {

                (*deckSplit)[game->suit][(*suitCount)[game->suit]++] =
                        game->player[game->p].deck[k];

            }

            continue;

        }

        Card crd = handle_bower(game->player[game->p].deck[k], game->suit);
        (*deckSplit)[crd.suit][(*suitCount)[crd.suit]++] =
                game->player[game->p].deck[k];

    }

}

// given suitCount, gives you total cards.
int sum_suitCount(int* suitCount) {
    return suitCount[0] + suitCount[1] + suitCount[2] + suitCount[3];

}

// returns the lowest non trump card (lowest trump if only trumps)
Card get_lowest_non_trump_card(GameInfo* game, int cards) {
    Card card;

    // default card to joker value, all other values are smaller
    card.value = JOKER_VALUE + 1;
    card.suit = 0;

    // we note that our deck is sorted already
    // split deck into suits
    // malloc deckSplit and suitCount
    Card** deckSplit = malloc(4 * sizeof(Card*));
    int* suitCount = malloc(4 * sizeof(Card));

    // populate split deck and suitCount
    split_deck(game, &deckSplit, &suitCount, cards);

    // find lowest card of non trump suit
    for (Trump i = 0; i < 4; i++) {
        if (i == game->suit || suitCount[i] == 0) {
            continue;

        }

        // check if lowest card of this suit is lower than min
        if (deckSplit[i][suitCount[i] - 1].value < card.value) {
            card = deckSplit[i][suitCount[i] - 1];

        }

    }

    // case where we only have trumps casuses issues here.
    // instead of checking for empty card on return, ensure this always returns
    // a valid card
    if (card.value == JOKER_VALUE + 1) {
        card = deckSplit[game->suit][suitCount[game->suit] - 1];

    }

    free(deckSplit);
    free(suitCount);

    return card;

}

// count how many estimated tricks we can win of each suit
// and bet based on that.
// note, cannot bet no trumps
// estimated number = 1 + number of cards of this suit
// + number of Kings or higher of other suits
void generate_max_bet(GameInfo* game, int* maxBet, Trump* maxSuit) {

    // loop through each suite
    for (Trump i = 0; i < 4; i++) {
        // begin count at 1, assumes 1 from kitty
        int suitCount = 1;

        // loop through each card, check how many we have of this
        for (int k = 0; k < NUM_ROUNDS; k++) {

            // handle joker safely
            if (game->player[game->p].deck[k].value == JOKER_VALUE) {
                suitCount++;
                continue;

            }

            Card crd = handle_bower(game->player[game->p].deck[k], i);

            // add 1 to count if we are of this trump
            if (crd.suit == i) {
                suitCount++;

            } else if (crd.value >= KING_VALUE){

                // add 1 to count if we have a king or higher value
                suitCount++;

            }

        }

        // if we have more of this than the other (less valued) suits,
        // use this as our max bet
        if (suitCount >= *maxBet) {
            *maxBet = suitCount;
            *maxSuit = i;

        }

    }

    // ensure maxBet is <= 10
    if (*maxBet > 10) {
        *maxBet = 10;

    }

}
