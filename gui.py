import subprocess
import os
import tkinter
from tkinter import messagebox
import re
import sys
from multiprocessing import Process, Pipe
import time

WIDTH = 760
HEIGHT = 760
BUFFER_LENGTH = 100
NEWLINE = os.linesep.encode('utf-8')

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
        messagebox.showinfo("Help", "Welcome to 500")
        self._bindings[3]("PASS")

# returns list representation of a string bet
# list has one element for misere
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

# connection pipe to parent process (Controller)
class Client():
    def __init__(self, args, conn):

        # connection
        self._conn = conn

        # game details
        self._players = []
        self._player = 0

        # create client args
        # note optional client argument given (the "1")
        clargs = ["./client"]
        clargs.extend(args)
        clargs.append("1")

        # create server
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

    # gets game data from client and sends it to parent
    def get_game_data(self):

        # these four lines are the players names
        for i in range(0, 4):
            # read ith players name, removing newline
            name = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]
            name.decode(sys.stdout.encoding)

            # remove text before first comma and newline
            name = ', '.join(str(name)[:-1].split(', ')[1:])
        
            # check if this player is us
            if re.search(r' \(You\)$', name) != None:
                name = re.sub(r' \(You\)$', '', name)
                self._player = i

            # remove teammate tag
            name = re.sub(r' \(Teammate\)$', '', name)

            self._players.append({"name" : name})

        # next line is always 'Game starting!'
        self._client.stdout.readline(BUFFER_LENGTH)

        # our deck is next
        name = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]

        # remove beginning text
        name = re.sub(r'^.*: ', '', str(name)[0:-1])

        # split into cards and save as our deck
        self._players[self._player]["deck"] = name.split(' ')

        # send details to parent
        self.send_to_parent({"players" : self._players, "player" : self._player})

    # completes bet round for the client
    def bet_round(self):
        # loop
        while True:
            bet = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]
            bet = bet.decode(sys.stdout.encoding)
            
            # case bet ending
            if re.search(r'won', bet):

                # getting winning player and their bet
                bet = re.sub(r'^Player ', '', str(bet))
                bet = bet.split(' ')

                bet_winner = int(bet[0])
                winning_bet = bet[5][:-1]
                
                print("player {0} won the bet with {1}".format(bet_winner, string_to_bet(winning_bet)))
                break

            # case a player bet
            elif re.search(r'^Player ', bet):
                # get our players bet
                bet = re.sub(r'^Player ', '', str(bet))
                bet = bet.split(' ')
                bet[0] = int(bet[0])

                if len(bet) == 3:
                    # player bet[0] bet something or misere or openmisere
                    print("player {0} bet {1}".format(bet[0], string_to_bet(bet[2])))

                elif len(bet) == 2: 
                    print("player {0} passed".format(bet[0]))

            # case our bet
            elif bet == 'Your bet: ':
                print("our bet")

                bet = self.get_from_parent().encode('utf-8')

                print(str(bet))
                
                self._client.stdin.write(bet + NEWLINE)
                self._client.stdin.flush()

            elif bet == 'Everyone passed. Game restarting.':
                print('everyone passed')

            # bet was a failure ( this should be in Your bet)
            else:
                print(str(bet))

    # loop 
    def game_loop(self):
        while True:
            line = self._client.stdout.readline(BUFFER_LENGTH)
            print(line)

            # early exit
            if line == b'':
                break

            # waiting for others
            elif line == b'Connected successfully! Waiting for others' + NEWLINE:
                print("waiting for others")

            # giving all player details now
            elif line == b'Players Connected:' + NEWLINE:
                self.get_game_data()

            # betting round
            elif line == b'Betting round starting' + NEWLINE:
                self.bet_round()
                
# not a traditional MVC application, the model is run on a different process and 
# interacts with the controller through a pipe
# this must be done since interacting with subprocess requires blocking calls
# and the UI must be kept responsive.
class Controller():
    def __init__(self, master):
        master.title("500") # title of window
        self._master = master

        # create view
        self._view = View(self._master, [self.join, self.host, self.game_exit, self.send_to_client])

        # kill children on close
        self._master.protocol("WM_DELETE_WINDOW", self.game_exit)

        # server
        self._server = None

        # connection to child
        self.parent_conn = None

    # when user presses join
    def join(self):

        # communication
        self.parent_conn, child_conn = Pipe()

        # temp details
        ip = "127.0.0.1"
        port = "2222"
        password ="pass"
        username = "test"

        # this call is blocking if the game_loop method is in our controller
        p = Process(target=Client, args=([ip, port, password, username], child_conn,))
        p.start()

        # get all player details
        print(self.recieve_from_client())

    # sends data to the client
    # must be a string
    def send_to_client(self, data):
        if self.parent_conn != None:
            self.parent_conn.send(data)

    # recieves data from client
    def recieve_from_client(self):
        if self.parent_conn != None:
            return self.parent_conn.recv()
        
        return None

    # when user presses host server
    def host(self):

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
