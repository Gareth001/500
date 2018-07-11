
500 Card Game Online
=======

## Features
* Command line implementation of 500
* Online gameplay, play with your friends across the internet
* Very lightweight with only a 23kb download for command line, or 2MB with gui.
* Fully implemented 500 functionality, such as enforcing following suit, misere and scoring
* Play with or against bots! Or even watch 4 bots battle it out! Similar in difficulty to an average 500 player
* 500 is now multiplatform!
* You can now play 500 with a GUI! Internally uses the command line interface with pretty cards and nice layouts, created in Qt

## Playing with the GUI
![alt text](https://github.com/Gareth001/500/blob/master/example.jpg "Sample in game screenshot")

To play with the GUI, you need to have python3.5 or python3.6 (tested on Windows and Linux).  Required dependencies are [PyQt5](https://pypi.org/project/PyQt5/) and [aenum](https://pypi.org/project/aenum/), which can be installed with `pip3 install aenum` followed by `pip3 install PyQt5`.

To use precompiled binaries on Windows, download `precompiled-windows.zip` from the [releases page](https://github.com/Gareth001). Extract and run gui.py with python3 to play. To compile yourself, see Compiling below.

To play on linux or mac, clone the repository, compile client and server (See compiling) and then use `python3 gui.py`.

## Compiling
Requires at least `gcc` and `make`. Can be compiled on Windows with MinGW + MSYS.
1. Download and unzip the latest release from the [releases page](https://github.com/Gareth001/500/releases)
3. Compile in 500 directory with `$ make`

## Running with the Command Line
Server: `$ ./server port password [playertypes]` (or the cmd equivalent in Windows)

To play with bots, supply the optional playertypes string. This is a string of 4 integers, one for each of the 4 players. If the integer is 0, then a human will be put in that player slot. If the integer is larger than 0, a bot of that difficulty will be put in that slot. Max bot difficulty is currently 2, with level 2 being average player difficulty and 1 being a very bad player. For example, a playertypes string of `0110` means the first and fourth players will be humans, and the 2nd and 3rd players will be level 1 bots. A playertypes string of `2222` will play a full game of all level 2 bots.

Client: `$ ./client ipaddress port password username`

To connect to localhost, use ip address of `0.0.0.0` (in Windows, `127.0.0.1`).

## Command line Input
Suits and Trumps are represented with the starting character of each trump, so **S**pades, **C**lubs, **D**iamonds, **H**earts, **N**o trumps in order from lowest to highest.
When betting, send a number between 6 and 10 followed by a character representing the trump. E.g. 7 No trumps is bet by sending 7N. Misere is bet by sending **MISERE** when the current bet is 7. Open Misere is bet by **OPENMISERE**, and can be bet any time.

Passing is done by **PASS**, betting finishes when all players pass (including the winner).

When choosing the joker suit if you won with a no trumps bet, send a single character representing the suit of the card.

Number cards are represented by their numerical value followed by the suit of the card. E.g. 8 of Spades is 8S.
Picture cards are represented by the starting character of the rank (**J**ack, **Q**ueen, **K**ing, **A**ce) followed by the suit, e.g. Ace of Spaces is AS.
Joker is represented by JOKER.
For the kitty and play rounds, just send a card by typing the card exactly as it appears in your deck.

Note: Server requires no input after starting.
## Rules
Australian rules, see https://www.pagat.com/euchre/500.html#oz4hand
## Credit
GUI uses cards minified versions of cards obtained from https://code.google.com/archive/p/vector-playing-cards/.
