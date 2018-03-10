500 Card Game Online
=======

## Features
* Command line implementation of 500
* Online gameplay, play with your friends across the internet
* Very lightweight with only a 15kb download
* Fully* implemented 500 functionality, such as enforcing following suite (*Misere TBA)


## Installation
Requires at least gcc and make.
1. Download zip of source through github
2. Extract and change directory
3. Compile with make

## Running
Server: ./server port password
Client: ./client ipaddress port password username

## Input
Suites and Trumps are represented with the starting character of each trump, so **S**pades, **C**lubs, **D**iamonds, **H**earts, **N**o trumps in order from lowest to highest. 
When betting, send a number between 6 and 10 followed by a character representing the trump. E.g. 7 No trumps is bet by sending 7N. Passing is done by PA, betting finishes when all players pass (including the winner).
When choosing the joker suite if you won with a no trumps bet, send a single character representing the suite of the card.

Number cards are represented by their numerical value followed by the suite of the card. E.g. 8 of Spades is 8S.
Picture cards are represented by the starting character of the rank (**J**ack, **Q**ueen, **K**ing, **A**ce) followd by the suite, e.g. Ace of Spaces is AS.
Joker is represented by JOKER.
For the kitty and play rounds, just send a card by typing the card exactly as it appears in your deck.

Note: Server requires no input after starting.
## Rules
Australian rules, see https://www.pagat.com/euchre/500.html#oz4hand