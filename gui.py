import os
import re
import sys
import enum
from aenum import Enum, AutoNumberEnum # pip3 install aenum
import subprocess
from multiprocessing import Process, Pipe
from PyQt5.QtWidgets import (QApplication, QWidget, QPushButton, QLayout,
        QStackedWidget, QFormLayout, QHBoxLayout, QComboBox, QLabel, QVBoxLayout,
        QMessageBox, QSpinBox, QLineEdit)
from PyQt5 import QtCore, QtSvg # pip3 install pyqt5

# 500 GUI created with PyQt. Acts as a wrapper for the client and server
# previously created in c. Creates a server and runs the client
# on a different process (since reading stdout from client is blocking).
# Communicates between this subprocess and the controller to update the GUI
# and get input from the user.

WIDTH = 830
HEIGHT = 900
CARD_WIDTH = 50 * 1.5 # width of card as it appears on the screen
CARD_HEIGHT = 73 * 1.5
BUFFER_LENGTH = 100 # length of buffer for client process
NEWLINE = os.linesep.encode('utf-8') # newline for this operating system (bytes)
POLL_DELAY = 50 # milliseconds to wait until checking for new data
NUMBER_PLAYERS = 4
MIN_TIME_BETWEEN_MOVES = 1000 # min number of milliseconds to wait until a move is completed

# returns string representation of a bet
# TODO: Misere
def string_to_bet(string):
    if string == 'PASS':
        return 'Pass'

    ret = []
    if len(string) == 2 or len(string) == 3:
        # put first 1 or 2 chars in first element
        ret.append(string[0:-1])

        ret.append(letter_to_suit(string[-1]))
        
    return str(ret[0]) + ' ' + str(ret[1])

# returns readable form of the lettered suit 
def letter_to_suit(letter):
    if letter == 'S':
        return "Spades"
    elif letter == 'C':
        return "Clubs"
    elif letter == 'D':
        return "Diamonds"
    elif letter == 'H':
        return "Hearts"
    elif letter == 'N':
        return "No Trumps"

# returns lettered suit of the readable bet 
def suit_to_letter(suit):
    if suit == 'Spades':
        return "S"
    elif suit == 'Clubs':
        return "C"
    elif suit == 'Diamonds':
        return "D"
    elif suit == 'Hearts':
        return "H"
    elif suit == 'No Trumps':
        return "N"

# communication between Client and Controller, data sent depends on "type" property
# this can take the following values, where there are the corresponding keys in {}
class MsgType(AutoNumberEnum):
    PLAYER = () # player details {players, player}
    BETINFO = () # bet info {player, bet}
    BETOURS = () # our bet {}
    BETFAILED = () # bet failed {message}
    BETWON = () # a player has won the bet {player, bet}
    KITTYDECK = () # new deck including the kitty {deck} TODO
    KITTYCHOOSE = () # choose the kitty cards {remaining} TODO
    KITTYEND = () # kitty end {} TODO
    JOKERSTART = () # waiting for joker suit {}
    JOKERCHOOSE = () #TODO
    JOKERBAD = () #TODO
    JOKERCHOSEN = () #TODO
    GAMESTART = () # game is starting {deck}
    ROUNDNEW = () # new round starting {round}
    ROUNDCARDPLAYED = () # card played {player, card, winningplayer, winningcard}
    ROUNDCHOOSE = () # send a card please {}
    ROUNDWON = () # round has been won {player, card, winningplayer, winningcard, bettingteamroundswon}
    ROUNDBAD = () # bad card sent {message}

# returns a QHBoxLayout containing text and widget
def create_menu_entry(text, widget):
    sublayout = QHBoxLayout()

    # create text label
    text_label = QLabel()
    text_label.setText(text)
    sublayout.addWidget(text_label)

    # add widget
    sublayout.addWidget(widget)

    return sublayout

# returns the localhost ip address for this system 
def get_localhost_ip():
    if os.name == "nt":
        return "127.0.0.1"
    
    return "0.0.0.0"

# creates client subprocess and interacts with parent (Controller) process
# through the given conn pipe
# args are arguments for running client
class Client():
    def __init__(self, args, conn):

        # connection
        self._conn = conn

        # game details
        self._players = []
        self._player = 0

        # create client args, note optional client argument given (the "1")
        clargs = ["./client"]
        clargs.extend(args)
        clargs.append("1")

        # create server, piping stdinin and stdout
        self._client = subprocess.Popen(clargs, stdin = subprocess.PIPE, stdout = subprocess.PIPE)
        print("Client Created.")

        # enter game loop
        self.game_loop()

    # sends data to the parent process (non-blocking)
    def send_to_parent(self, data):
        self._conn.send(data)

    # gets data from the parent process (blocking)
    def get_from_parent(self):
        return self._conn.recv()

    # returns string of line from client (blocking)
    def read_line(self):
        ret = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]
        ret.decode(sys.stdout.encoding)
        # remove b'' from string and return
        return str(ret)[2:-1]

    def send_to_client(self, line):
        self._client.stdin.write(line + NEWLINE)
        self._client.stdin.flush()

    # gets game data from client and sends it to parent
    def get_game_data(self):

        # these four lines are the players names
        for i in range(0, NUMBER_PLAYERS):
            # read ith players name, removing newline
            name = self.read_line()

            # remove text before first comma and newline
            name = ', '.join(name.split(', ')[1:])
        
            # check if this player is us
            if re.search(r' \(You\)$', name) != None:
                name = re.sub(r' \(You\)$', '', name)
                self._player = i

            # remove teammate tag
            name = re.sub(r' \(Teammate\)$', '', name)

            self._players.append({"name" : name})

        # next line is always 'Game starting!'
        self.read_line()
        
        # our deck is next
        deck = self.read_line()

        # remove beginning text
        deck = re.sub(r'^.*: ', '', deck)

        # split into cards and save as our deck
        self._players[self._player]["deck"] = deck.split(' ')

        # send details to parent
        self.send_to_parent({"players" : self._players, "player" : self._player,
                "type" : MsgType.PLAYER})

    # completes bet round for the client
    def bet_round(self):
        # loop until betting round over
        while True:
            bet = self.read_line()
            
            # case bet ending
            if re.search(r'won', bet):

                # getting winning player and their bet
                bet = re.sub(r'^Player ', '', bet)
                bet = bet.split(' ')

                bet_winner = int(bet[0])
                winning_bet = bet[5][:-1] # remove exclamation mark

                # send bet winner to parent
                self.send_to_parent({"bet" : winning_bet,
                        "player" : bet_winner, "type" : MsgType.BETWON})

                break

            # case a player bet
            elif re.search(r'^Player ', bet):
                # get our players bet
                bet = re.sub(r'^Player ', '', bet)
                bet = bet.split(' ')
                bet[0] = int(bet[0])

                if len(bet) == 3:
                    # player bet something or misere or openmisere
                    # send the bet to parent
                    self.send_to_parent({"type" : MsgType.BETINFO, "player" : bet[0], 
                            "bet" : bet[2]})

                elif len(bet) == 2: 
                    # player must have passed
                    # send the bet to parent
                    self.send_to_parent({"type" : MsgType.BETINFO, "player" : bet[0], 
                            "bet" : "PASS"})

            # case our bet
            elif bet == 'Your bet: ':

                # tell parent we're waiting on their bet
                self.send_to_parent({"type" : MsgType.BETOURS})

                # encode our bet to bytes
                ourbet = self.get_from_parent().encode('utf-8')
                print("sending bet: " + str(ourbet))
                
                # send message to our client
                self.send_to_client(ourbet)

            elif bet == 'Everyone passed. Game restarting.':
                # TODO case everyone passed
                print('everyone passed, disaster!')

            # bet was a failure (i.e. bet too low)
            else:
                # tell parent bet failed and send it to them
                self.send_to_parent({"type" : MsgType.BETFAILED, "message" : bet})
                print(bet)

    # kitty loop
    def kitty(self):
        while True:
            line = self.read_line()

            # line is either 'Kitty finished' or like '^You won!' or like '^Pick x more'
            # or an error message (which should be impossible!)
            if line == 'Kitty finished':
                # tell parent kitty is finished
                self.send_to_parent({"type" : MsgType.KITTYEND})
                break
                
            elif re.search(r'^You Won!', line):

                # remove everything before the ': '
                line = re.sub(r'^.*: ', '', line)

                # this line contains our new hand
                # send the server this new deck (note it's sorted so send it all)
                self.send_to_parent({"type" : MsgType.KITTYDECK,
                        "deck" : line.split()})

            elif re.search(r'^Pick ', line):

                # this line contains the amount of cards we still have to discard
                # ask parent for a card for kitty and send amount remaining
                self.send_to_parent({"type" : MsgType.KITTYCHOOSE,
                        "remaining" : int(line.split(' ')[1])})

                # get card from parent and send it 
                ourbet = self.get_from_parent().encode('utf-8')
                self.send_to_client(ourbet)

    # joker round
    def joker(self):
        # tell parent we are waiting for joker suit (potentially)

        while True:
            line = self.read_line()

            # TODO functionality for No Trumps

            # joker round ends, goto game loop
            if line == "Game Begins":
                self.play()

    # play round
    def play(self):

        # the first line has our new deck, note this one is sorted
        line = self.read_line()
        self.send_to_parent({"type" : MsgType.GAMESTART,
                "deck" : re.sub(r'^.*: ', '', line).split(' ')})

        while True:
            line = self.read_line()

            # note that we don't need to send the hand repeatedly, parent will handle
            if re.search(r'Your hand: ', line):
                continue
            
            # new round
            elif re.search(r'Round ', line):
                self.send_to_parent({"type" : MsgType.ROUNDNEW,
                        "round" : int(line.split(' ')[1])})

            # player won the bet
            elif re.search(r'won', line):
                # remove player
                line = re.sub(r'Player ', '', line)

                # split around '. '
                line = line.split('. ')

                # next line has how many tricks have been won by the betting team
                line2 = self.read_line()
                
                self.send_to_parent({"type" : MsgType.ROUNDWON, 
                        "player" : int(line[0].split(' ')[0]), "card" : line[0].split(' ')[-1],
                        "winningplayer" : int(line[1].split(' ')[0]),
                        "winningcard" : line[1].split(' ')[-1], 
                        "bettingteamroundswon" : int(line2.split(' ')[4])})   

            # player played a card
            elif re.search(r'Player ', line):

                # remove player
                line = re.sub(r'Player ', '', line)

                # split around '. '
                line = line.split('. ')

                self.send_to_parent({"type" : MsgType.ROUNDCARDPLAYED, 
                        "player" : int(line[0].split(' ')[0]), "card" : line[0].split(' ')[-1],
                        "winningplayer" : int(line[1].split(' ')[0]),
                        "winningcard" : line[1].split(' ')[-1]})   


            elif line == "Choose card to play: ":
                self.send_to_parent({"type" : MsgType.ROUNDCHOOSE})

                # get card from parent and send it 
                ourbet = self.get_from_parent().encode('utf-8')
                self.send_to_client(ourbet)

            # blank line is just a seperator
            elif line == '':
                continue

            # TODO cases for restarting game

            # error betting
            else:
                self.send_to_parent({"type" : MsgType.ROUNDBAD, 
                        "message" : line})


    # loop 
    def game_loop(self):
        while True:
            line = self.read_line()
            print(line)

            # early exit
            if line == '':
                break

            # waiting for others
            elif line == 'Connected successfully! Waiting for others':
                print("waiting for others")

            # giving all player details now
            elif line == 'Players Connected:':
                self.get_game_data()

            # betting round
            elif line == 'Betting round starting':
                self.bet_round()

            # kitty choosing round
            elif line == 'Waiting for Kitty':
                self.kitty()

            # choose joker suit (note this leads to play)
            elif line == "Choosing joker suit":
                self.joker()

# not a traditional MVC application, the model is run on a different process and 
# interacts with the controller through a pipe
# this must be done since interacting with the client subprocess requires blocking calls
# and the UI must be kept responsive
class Controller(QWidget):
    def __init__(self):
        super().__init__()

        # game details
        self._player = 0
        self._players = None
        self._round = None
        self._cards_played = 0

        # child processes
        self._server = None
        self._client = None
        self._parent_conn = None # connection to child

        # set graphics properties
        self.setWindowTitle('500')
        self.setGeometry(50, 50, WIDTH, HEIGHT)
        self.setFixedSize(self.size())

        # create main menu
        self._main_menu = QWidget()
        self.create_main_menu_view()

        # create game options view
        self._options_type = None # which options screen to display
        self._port = None
        self._username = None
        self._ip = None
        self._password = None
        self._player_types = [None] * 3
        self._game_options_view = QWidget()
        self.create_game_options_view()

        # create game view
        self._game_view = QWidget()
        self._card_layout = [None] * NUMBER_PLAYERS # array of players cards
        self._played_cards = None # four cards in center of screen
        self._player_info = [None] * NUMBER_PLAYERS # info label for each player
        self._bet_controls = []
        self._bet_label = None
        self.create_game_view()

        # create stacked layout
        self._stacked_layout = QStackedWidget(self)
        self._stacked_layout.addWidget(self._main_menu)
        self._stacked_layout.addWidget(self._game_options_view)
        self._stacked_layout.addWidget(self._game_view)

        self.show()

    # create main menu and add it to _main_menu layout
    def create_main_menu_view(self):
        # create layout
        layout = QVBoxLayout()
        layout.setAlignment(QtCore.Qt.AlignCenter)

        # bots button
        button = QPushButton('Play against bots', self)
        button.clicked.connect(lambda: self.goto_game_options(0))
        layout.addWidget(button)

        # join server
        button = QPushButton('Join a server', self)
        button.clicked.connect(lambda: self.goto_game_options(1))
        layout.addWidget(button)

        # host and play
        button = QPushButton('Host and play', self)
        button.clicked.connect(lambda: self.goto_game_options(2))
        layout.addWidget(button)

        # options
        button = QPushButton('Options', self)
        layout.addWidget(button)

        # exit
        button = QPushButton('Exit', self)
        button.clicked.connect(self.close)
        layout.addWidget(button)

        # add to layout
        self._main_menu.setLayout(layout)

    # go to the game options screen
    # type is which screen button you came from:
    # 0 for bots
    # 1 for join server
    # 2 for host and play
    def goto_game_options(self, options_type):
        self._options_type = options_type
        self._stacked_layout.setCurrentIndex(1)
        self.reset_game_options()

        # hide layouts depending on which options type we are
        if options_type == 0:
            self._ip.hide()
            self._password.hide()
            for index in range(0,3):
                self._player_types[index].hide()

        elif options_type == 1:
            for index in range(0,3):
                self._player_types[index].hide()

        elif options_type == 2:
            self._ip.hide()

    # unhide all game options (note port and username are never hidden)
    def reset_game_options(self):
        self._ip.show()
        self._password.show()
        for index in range(0,3):
            self._player_types[index].show()

    # create the options for joining game menu
    def create_game_options_view(self):
        # create layout
        layout = QVBoxLayout()
        layout.setAlignment(QtCore.Qt.AlignCenter)

        # username
        edit = QLineEdit()
        edit.setText("Jimmy")
        edit.setMaximumWidth(200)
        self._username = QWidget()
        self._username.setLayout(create_menu_entry("Username: ", edit))
        layout.addWidget(self._username)

        # port
        port = QSpinBox()
        port.setMaximum(65535)
        port.setValue(2222)
        port.setMaximumWidth(200)
        self._port = QWidget()
        self._port.setLayout(create_menu_entry("Port: ", port))
        layout.addWidget(self._port)

        # ip 
        ip = QLineEdit()
        ip.setMaximumWidth(200)
        self._ip = QWidget()
        self._ip.setLayout(create_menu_entry("Ip Address: ", ip))
        layout.addWidget(self._ip)

        # password
        password = QLineEdit()
        password.setMaximumWidth(200)
        self._password = QWidget()
        self._password.setLayout(create_menu_entry("Password: ", password))
        layout.addWidget(self._password)

        # player options
        for index in range(0,3):
            player_type = QComboBox()
            player_type.addItems(["Human", "lvl 1 bot", "lvl 2 bot"])
            player_type.setMaximumWidth(200)
            self._player_types[index] = QWidget()
            self._player_types[index].setLayout(create_menu_entry(
                    "Player " + str(index + 2) + " is a :", player_type))
            layout.addWidget(self._player_types[index])

        # join button
        button = QPushButton('Go', self)
        button.clicked.connect(self.create_game)
        layout.addWidget(button)

        # back button
        button = QPushButton('Back to Menu', self)
        button.clicked.connect(lambda: self._stacked_layout.setCurrentIndex(0))
        layout.addWidget(button)

        # create layout with stretch to force center the controls
        hcentered_layout = QHBoxLayout()
        hcentered_layout.addStretch(1)
        hcentered_layout.addLayout(layout)
        hcentered_layout.addStretch(1)

        self._game_options_view.setLayout(hcentered_layout)

    # returns username from the game options screen
    def get_username(self):
        return self._username.layout().itemAt(1).widget().text()

    # returns port from the game options screen
    def get_port(self):
        return self._port.layout().itemAt(1).widget().value()

    # returns password from the game options screen
    def get_password(self):
        return self._password.layout().itemAt(1).widget().text()

    # returns password from the game options screen
    def get_ip(self):
        return self._ip.layout().itemAt(1).widget().text()

    # returns player type from the game options screen for given player
    def get_player_type(self, index):
        return self._player_types[index].layout().itemAt(1).widget().currentIndex()

    # creates game view and adds it to _game_view layout
    def create_game_view(self):
        # layout
        game_layout = QVBoxLayout()

        self._card_layout[2] = QHBoxLayout()
        # create card iamges for teammate
        for _ in range(0, 10):
            svgWidget = QtSvg.QSvgWidget('img/BACK.svg')
            svgWidget.setFixedSize(CARD_WIDTH, CARD_HEIGHT)
            self._card_layout[2].addWidget(svgWidget)

        # add this card layout
        game_layout.addLayout(self._card_layout[2])

        # Hbox for center of the screen
        middle_layout = QHBoxLayout()

        # player on other team
        self._card_layout[1] = QVBoxLayout()
        self._card_layout[1].setAlignment(QtCore.Qt.AlignLeft)

        for _ in range(0, 10):
            svgWidget = QtSvg.QSvgWidget('img/BACK.svg')
            svgWidget.setFixedSize(CARD_WIDTH/2, CARD_HEIGHT/2)
            self._card_layout[1].addWidget(svgWidget)
        
        # add this card layout
        middle_layout.addLayout(self._card_layout[1])

        # add other team info label
        self._player_info[1] = QLabel(self)
        self._player_info[1].setAlignment(QtCore.Qt.AlignLeft | QtCore.Qt.AlignVCenter)
        middle_layout.addWidget(self._player_info[1])

        # layout in middle center (for teammate and your info)
        middle_center_layout = QVBoxLayout()

        # add teammate info label
        self._player_info[2] = QLabel(self)
        self._player_info[2].setAlignment(QtCore.Qt.AlignTop | QtCore.Qt.AlignHCenter)
        middle_center_layout.addWidget(self._player_info[2])

        # add where the 4 cards will go during play
        self._played_cards = QHBoxLayout()
        for _ in range(0, NUMBER_PLAYERS):
            svgWidget = QtSvg.QSvgWidget('img/BACK.svg')
            svgWidget.setFixedSize(CARD_WIDTH, CARD_HEIGHT)
            self._played_cards.addWidget(svgWidget)
            
        # add this card layout
        middle_center_layout.addLayout(self._played_cards)

        # add player info label
        self._player_info[0] = QLabel(self)
        self._player_info[0].setAlignment(QtCore.Qt.AlignBottom | QtCore.Qt.AlignCenter)
        middle_center_layout.addWidget(self._player_info[0])

        # add middle center layout to middle layout
        middle_layout.addLayout(middle_center_layout)

        # add other team info label
        self._player_info[3] = QLabel(self)
        self._player_info[3].setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)
        middle_layout.addWidget(self._player_info[3])

        # set player info as waiting
        for index in range(0, 4):
            self._player_info[index].setText("Waiting for player")

        self._card_layout[3] = QVBoxLayout()
        self._card_layout[3].setAlignment(QtCore.Qt.AlignRight)
        # create card iamges for other team
        for _ in range(0, 10):
            svgWidget = QtSvg.QSvgWidget('img/BACK.svg')
            svgWidget.setFixedSize(CARD_WIDTH/2, CARD_HEIGHT/2)
            self._card_layout[3].addWidget(svgWidget)

        # add this card layout
        middle_layout.addLayout(self._card_layout[3])

        # add the middle layout
        game_layout.addLayout(middle_layout)

        # create card iamges for player
        self._card_layout[0] = QHBoxLayout()
        self._card_layout[0].setAlignment(QtCore.Qt.AlignBottom)
        for _ in range(0, 10):
            svgWidget = QtSvg.QSvgWidget('img/BACK.svg')
            svgWidget.setFixedSize(CARD_WIDTH, CARD_HEIGHT)
            self._card_layout[0].addWidget(svgWidget)

        # add this card layout
        game_layout.addLayout(self._card_layout[0])

        # add to layout
        game_layout.addLayout(self.create_bet_controls())
        self._game_view.setLayout(game_layout)

    # returns layout containing all bet controls
    def create_bet_controls(self):
        # create layout for betting actions
        layout = QHBoxLayout()

        # number bet choice button
        number_bet = QComboBox()
        number_bet.addItems(["6", "7", "8", "9", "10"])
        layout.addWidget(number_bet)
        self._bet_controls.append(number_bet)

        # suit bet choice button
        suit_bet = QComboBox()
        suit_bet.addItems(["Spades", "Clubs", "Diamonds", "Hearts", "No Trumps"])
        layout.addWidget(suit_bet)
        self._bet_controls.append(suit_bet)

        # bet button, sends bet based on what we entered in the choice boxes above
        button = QPushButton('Bet', self)
        button.clicked.connect(lambda: self.send_to_client(number_bet.currentText() +
                suit_to_letter(suit_bet.currentText())))
        layout.addWidget(button)
        self._bet_controls.append(button)

        # pass button
        button = QPushButton('Pass', self)
        button.clicked.connect(lambda: self.send_to_client("PASS"))
        layout.addWidget(button)
        self._bet_controls.append(button)

        # misere button (TODO functionality)
        button = QPushButton('Bet Misere', self)
        layout.addWidget(button)
        self._bet_controls.append(button)

        # open misere button (TODO functionality)
        button = QPushButton('Bet Open Misere', self)
        layout.addWidget(button)
        self._bet_controls.append(button)

        # bet error message
        self._bet_label = QLabel(self)
        self._bet_label.setFixedWidth(200)
        # TODO set to red text
        layout.addWidget(self._bet_label)

        # main menu button
        button = QPushButton('Exit to menu', self)
        layout.addWidget(button)
        button.clicked.connect(lambda: self.exit_to_menu())
        self._bet_controls.append(button)

        # disable bet buttons by default
        self.activate_bet_controls(set=False)

        return layout

    # toggles button activation
    # leave bool as None to toggle bet
    def activate_bet_controls(self, set=None):
        for elem in self._bet_controls[:len(self._bet_controls) - 1]:
            elem.setEnabled(not elem.isEnabled() if set == None else set)

    # reset bet controls
    def reset_bet_controls(self):
        self.activate_bet_controls(set=False)
        self._bet_controls[0].setCurrentIndex(0)
        self._bet_controls[1].setCurrentIndex(0)

    # given deck, updates the players hand
    def update_player_hand(self, deck, setEvent = False):
        # update each card
        for i, card in enumerate(deck):
            widget = self._card_layout[self._player].itemAt(i).widget()
            widget.load('img/' + card + '.svg')

            # we only need this set after it is possible to choose a card
            if setEvent:
                # disable until we are required to send a card
                widget.setEnabled(False)

                # note the extra card=card argument to stop each 
                # lambda using local variable card
                widget.mouseReleaseEvent = lambda event, card=card: self.send_card(card)

    # resets all players hands
    def reset_players_hands(self):
        # show each players cards again
        for i in range(0, NUMBER_PLAYERS):
            for j in range(0, 10):
                widget = self._card_layout[i].itemAt(j).widget()
                widget.load('img/BACK.svg')
                widget.show()

        # reset card layout to what it was before
        self.rearrange_card_layout(-int(self._player)) # int to remove pylint errors

    # sends card to client and also disables all cards from being pressed
    def send_card(self, card):
        self.send_to_client(card)

    # toggles card controls
    # supply set to give them a value
    def activate_card_controls(self, set=None):

        for i in range(0, self._card_layout[self._player].count()):
            widget = self._card_layout[self._player].itemAt(i).widget()
            widget.setEnabled(not widget.isEnabled() if set == None else set)

    # removes given card from our hand
    # note closing the widget does not decrease the count of cards in the layout
    def remove_card_from_hand(self, player, card):

        # find location of card in our deck and close that widget
        if player == self._player:
            index = self._players[self._player]["deck"].index(card)
        else:
            index = self._round
        self._card_layout[player].itemAt(index).widget().hide()

    # resets the cards played interface
    def reset_cards_played(self):
        self._cards_played = 0
        for i in range (0, self._played_cards.count()):
            self._played_cards.itemAt(i).widget().load('img/BACK.svg')

    # adds the card to the center
    def add_card_played(self, card):
        self._played_cards.itemAt(self._cards_played).widget().load('img/' + card + '.svg')
        self._cards_played += 1

    # reset player info
    def reset_player_info(self):
        # set player info as waiting
        for index in range(0, 4):
            self._player_info[index].setText("Waiting for player")


    # change the card layout so that number is the player at the base of the 
    # interface. player 0 is there by default.
    def rearrange_card_layout(self, number):
        b = self._card_layout[-number:]
        b.extend(self._card_layout[:-number])
        self._card_layout = b

        # same for self._player_info
        c = self._player_info[-number:]
        c.extend(self._player_info[:-number])
        self._player_info = c

    # check for new input from our Client regularly
    # this means that our GUI is not being blocked for waiting
    # also note that multiprocess poll is not the same as subprocess poll,
    # hence the need for Client on seperate process
    # see MsgTypes enum for details on message contents
    def handle_client_input(self):

        # slow down interaction for when you are playing with bots
        after = POLL_DELAY

        # check if game is still going
        if self._parent_conn == None:
            return

        # repeatedly poll if we have input from Client
        while self._parent_conn.poll():
            data = self.recieve_from_client()
            print(data)

            # recieve game details
            if data["type"] is MsgType.PLAYER:
                self._player = data["player"]
                self._players = data["players"]

                # we want to arrange the self._card_layout indexes so that 
                # index 0 becomes the new index self._player.
                self.rearrange_card_layout(self._player)

                # update deck on screen
                # we only will add the click event either during choosing the kitty
                # or when the first round begins
                self.update_player_hand(data["players"][self._player]["deck"])

                # update all info on screen
                for index in range(0, NUMBER_PLAYERS):
                    self._player_info[index].setText(data["players"][index]["name"] + os.linesep)

            # enable bet controls on our bet
            elif data["type"] is MsgType.BETOURS:
                # activate bet controls
                self.activate_bet_controls(set=True)
            
            elif data["type"] is MsgType.BETINFO:
                # reset error text and disable bet controls if this was us
                if data["player"] == self._player:
                    self._bet_label.setText("")
                    self.activate_bet_controls(set=False)

                # add betting text
                self._player_info[data["player"]].setText(
                        self._player_info[data["player"]].text()
                        + os.linesep + string_to_bet(data["bet"]))

                if MIN_TIME_BETWEEN_MOVES:
                    after = MIN_TIME_BETWEEN_MOVES
                    break

            # display why the users bet failed
            elif data["type"] is MsgType.BETFAILED:
                self._bet_label.setText(data["message"])

            # disable controls after betting is done
            elif data["type"] is MsgType.BETWON:
                self.activate_bet_controls(set=False)

                if MIN_TIME_BETWEEN_MOVES:
                    after = MIN_TIME_BETWEEN_MOVES
                    break

            # update bet (cards are now sorted by suit)
            elif data["type"] is MsgType.GAMESTART:
                self._players[self._player]["deck"] = data["deck"]
                self.update_player_hand(data["deck"], setEvent=True)

            # new round
            elif data["type"] is MsgType.ROUNDNEW:
                # reset number of cards played
                self.reset_cards_played()
                self._round = data["round"]

            # we can play a card, activate card controls
            elif data["type"] is MsgType.ROUNDCHOOSE:
                self.activate_card_controls(set=True)

            # card has been played
            elif data["type"] is MsgType.ROUNDCARDPLAYED:
                # remove card from the hand
                self.activate_card_controls(set=False)
                self.remove_card_from_hand(data["player"], data["card"])
                    
                # play card to self._played_cards
                self.add_card_played(data["card"])

                # only have a break if we are not next
                if MIN_TIME_BETWEEN_MOVES and \
                        not (data["player"] + 1) % NUMBER_PLAYERS == self._player:
                    after = MIN_TIME_BETWEEN_MOVES
                    break

            # player played bad card, show them a message
            elif data["type"] is MsgType.ROUNDBAD:
                QMessageBox.information(self, '500', data["message"])

            # also case that we are in the last round of play
            elif data["type"] is MsgType.ROUNDWON:
                # remove card from the hand
                self.activate_card_controls(set=False)
                self.remove_card_from_hand(data["player"], data["card"])

                # play card to self._played_cards
                self.add_card_played(data["card"])
                if MIN_TIME_BETWEEN_MOVES:
                    after = MIN_TIME_BETWEEN_MOVES
                    break

        # repeat 
        QtCore.QTimer.singleShot(after, self.handle_client_input)

    # when user presses join
    def join(self, port, password, ip, username):
        # communication
        self._parent_conn, child_conn = Pipe()

        # this call is blocking if the game_loop method is in our controller class!
        self._client = Process(target=Client, args=([ip, port, password, username], child_conn,))
        self._client.start()

        # switch to game view
        self._stacked_layout.setCurrentIndex(2)

        # check for new data sent by Client in regular intervals
        QtCore.QTimer.singleShot(POLL_DELAY, self.handle_client_input)

    # sends data to the client, must be a string (non blocking)
    def send_to_client(self, data):
        if self._parent_conn != None:
            self._parent_conn.send(data)

    # recieves data from client (blocking)
    def recieve_from_client(self):
        if self._parent_conn != None:
            return self._parent_conn.recv()
        
        return None

    # when user presses host server
    def host(self, port, password, ptypes):
        self.create_server(port, password, ptypes)

    # when user presses the go button
    def create_game(self):

        # these details are always required details
        port = str(self.get_port())
        username = self.get_username()

        #TODO error checking e.g. empty password

        # rest of the details are different for each options type
        if self._options_type == 0:
            password = "pass"
            ptypes = "2022"
            ip = get_localhost_ip()
            self.host(port, password, ptypes)

        elif self._options_type == 1:
            password = self.get_password()
            ip = self.get_ip()

        elif self._options_type == 2:
            password = self.get_password()
            ip = get_localhost_ip()
            ptypes = ["0"]

            for index in range(0,3):
                ptypes.append(str(self.get_player_type(index)))

            self.host(port, password, ''.join(ptypes))

        self.join(port, password, ip, username)

    # ensure we exit only after cleaning up
    def closeEvent(self, event):
        self.close_subprocesses()
        return QWidget.closeEvent(self, event)

    # closes server and client subprocesses
    def close_subprocesses(self):
        if self._server != None:
            self._server.kill()
            self._server = None
            self._parent_conn = None
            print("Server Terminated")

        if self._client != None:
            self._client.terminate()
            self._client = None
            print("Client Terminated")

    # change layout and close server and client subprocesses
    def exit_to_menu(self):
        self._stacked_layout.setCurrentIndex(0)
        self.close_subprocesses()
        self.reset()

    # reset game variables
    def reset(self):
        # reset all parts of gui
        self.reset_cards_played()
        self.reset_bet_controls()
        self.reset_players_hands()
        self.reset_player_info()

        # reset all player details
        self._player = 0
        self._players = None
        self._round = None
        self._cards_played = 0

    # creates the server
    def create_server(self, port, password, playertypes):
            
        # create server args
        srvargs = ["./server", port, password, playertypes]
            
        # create server
        self._server = subprocess.Popen(srvargs, stdout=subprocess.DEVNULL)
        print("Server Created.")

if __name__ == '__main__':    
    app = QApplication([])
    ex = Controller()
    sys.exit(app.exec_())