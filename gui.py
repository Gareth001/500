import os
import re
import sys
import enum
from enum import Enum
import subprocess
from multiprocessing import Process, Pipe
import tkinter
from tkinter import messagebox

WIDTH = 760
HEIGHT = 760
BUFFER_LENGTH = 100 # length of buffer for client process
NEWLINE = os.linesep.encode('utf-8') # newline for this operating system (bytes)
POLL_DELAY = 50 # milliseconds to wait until checking for new data

class View(object):
    def __init__(self, master, bindings):
        # events to controller
        self._bindings = bindings
        self._event = "<ButtonRelease-1>"
        
        # main menu frame
        self._main_menu = tkinter.Frame(master)
        self._main_menu.pack(expand=True)
        self._main_menu.focus_set()

        label = tkinter.Label(self._main_menu, text="500 - GUI edition")
        label.pack(expand=True)
        
        # all having the same name is fine
        button = tkinter.Button(self._main_menu, text="Play")
        button.bind(self._event, self.join)
        button.pack(expand=True)
        
        button = tkinter.Button(self._main_menu, text="Host & Play")
        button.bind(self._event, self.host)
        button.pack(expand=True)
        
        button = tkinter.Button(self._main_menu, text="Help")
        button.bind(self._event, self.help)
        button.pack(expand=True)
        
        # no change of gui needed
        button = tkinter.Button(self._main_menu, text="Exit")
        button.bind(self._event, lambda event: self._bindings[2]())
        button.pack(expand=True)
        
    # change gui and execute callback
    def join(self, event):
        self._bindings[0]()

    # change gui and execute callback
    def host(self, event):
        self._bindings[1]()
        
    # show help
    def help(self, event):
        #messagebox.showinfo("Help", "Welcome to 500")
        self._bindings[3]("PASS")

# returns string representation of a bet
# TODO: Misere
def string_to_bet(string):
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

# communication between Client and Controller, data sent depends on "type" property
# this can take the following values
class MsgType(Enum):
    PLAYER = enum.auto()
    BETINFO = enum.auto()
    BETOURS = enum.auto()
    BETFAILED = enum.auto()
    BETWON = enum.auto()
    KITTYDECK = enum.auto()
    KITTYCHOOSE = enum.auto()
    KITTYEND = enum.auto()

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
        for i in range(0, 4):
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

# not a traditional MVC application, the model is run on a different process and 
# interacts with the controller through a pipe
# this must be done since interacting with the client subprocess requires blocking calls
# and the UI must be kept responsive
class Controller():
    def __init__(self, master):
        master.title("500") # title of window
        self._master = master

        # create view
        self._view = View(self._master, [self.join, self.host, self.game_exit, self.send_to_client])

        # kill children on close
        self._master.protocol("WM_DELETE_WINDOW", self.game_exit)

        # child processes
        self._server = None
        self._client = None

        # connection to child
        self.parent_conn = None

    # check for new input from our Client regularly
    # this means that our GUI is not being blocked for waiting
    # also note that multiprocess poll is not the same as subprocess poll,
    # hence the need for Client on seperate process
    def handle_client_input(self):

        # poll if we have input from Client
        if self.parent_conn.poll():
            data = self.recieve_from_client()
            print(data)

            # now interact with GUI depending on message type

        # repeat 
        self._master.after(POLL_DELAY, self.handle_client_input)

    # when user presses join
    def join(self):

        # communication
        self.parent_conn, child_conn = Pipe()

        # temp details
        ip = "127.0.0.1"
        port = "2222"
        password ="pass"
        username = "test"

        # this call is blocking if the game_loop method is in our controller class!
        self._client = Process(target=Client, args=([ip, port, password, username], child_conn,))
        self._client.start()

        # check for new data sent by Client in regular intervals
        self._master.after(POLL_DELAY, self.handle_client_input)

    # sends data to the client, must be a string (non blocking)
    def send_to_client(self, data):
        if self.parent_conn != None:
            self.parent_conn.send(data)

    # recieves data from client (blocking)
    def recieve_from_client(self):
        if self.parent_conn != None:
            return self.parent_conn.recv()
        
        return None

    # when user presses host server
    def host(self):

        # temp details
        password = "pass"
        port = "2222"
        ptypes = "2022"

        self.create_server(port, password, ptypes)

    # ensure we exit only after cleaning up
    def game_exit(self):
        self._master.destroy()

        if self._server != None:
            self._server.terminate()
            print("Server Terminated")

        if self._client != None:
            self._client.terminate()
            print("Client Terminated")

    # creates the server
    def create_server(self, port, password, playertypes):
            
        # create server args
        srvargs = ["./server", port, password, playertypes]
            
        # create server
        self._server = subprocess.Popen(srvargs, stdout=subprocess.DEVNULL)
        print("Server Created.")

if __name__ == '__main__':
    root = tkinter.Tk()
    app = Controller(root)
    root.resizable(0,0) #Remove maximise button
    root.geometry('760x760+150+150')
    root.mainloop()
