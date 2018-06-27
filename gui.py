import subprocess
import os
import tkinter
from tkinter import messagebox
import re
import sys

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

class Controller():
    def __init__(self, master):
        master.title("500") # title of window
        self._master = master

        # create view
        self._view = View(self._master, [self.join, self.host, self.game_exit])

        # kill children on close
        self._master.protocol("WM_DELETE_WINDOW", self.game_exit)

        # server
        self._server = None
        self._client = None

    # when user presses join
    def join(self):
        
        ip = "127.0.0.1"
        port = "2222"
        password ="pass"
        username = "test"

        self.create_client(ip, port, password, username)
        
        players = []
        player = 0

        # game loop
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
                for i in range(0, 4):
                    # read ith players name, removing newline
                    name = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]
                    name.decode(sys.stdout.encoding)

                    # remove text before first comma and newline
                    name = ', '.join(str(name)[:-1].split(', ')[1:])
                
                    # check if this player is us
                    if re.search(r' \(You\)$', name) != None:
                        name = re.sub(r' \(You\)$', '', name)
                        player = i

                    # remove teammate tag
                    name = re.sub(r' \(Teammate\)$', '', name)

                    players.append({"name" : name})

                # next line is always 'Game starting!'
                self._client.stdout.readline(BUFFER_LENGTH)

                # our deck is next
                name = self._client.stdout.readline(BUFFER_LENGTH)[:-len(NEWLINE)]

                # remove beginning text
                name = re.sub(r'^.*: ', '', str(name)[0:-1])

                # split into cards and save as our deck
                players[player]["deck"] = name.split(' ')

                # print all details                
                print("Playerid:{0}, Players:{1}".format(player, players))

            # betting round
            elif line == b'Betting round starting' + NEWLINE:
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
                        
                        print("player {0} won the bet with {1}".format(bet_winner, winning_bet))
                        break

                    # case a player bet
                    elif re.search(r'^Player ', bet):
                        # get our players bet
                        bet = re.sub(r'^Player ', '', str(bet))
                        bet = bet.split(' ')
                        bet[0] = int(bet[0])

                        if len(bet) == 3:
                            # player bet[0] bet something or misere or openmisere
                            print("player {0} bet {1}".format(bet[0], bet[2]))

                        elif len(bet) == 2: 
                            print("player {0} passed".format(bet[0]))

                        
                    # case our bet
                    elif bet == 'Your bet: ':
                        print("our bet")
                        self._client.stdin.write(b"PASS" + NEWLINE)
                        self._client.stdin.flush()

                    elif bet == 'Everyone passed. Game restarting.':
                        print('everyone passed')

                    # bet was a failure ( this should be in Your bet)
                    else:
                        print(str(bet))

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
        if self._server != None:
            self._client.terminate()
            print("Client Terminated")

    # creates the server
    def create_server(self, port, password, playertypes):
            
        # create server args
        srvargs = ["./server", port, password, playertypes]
            
        # create server
        self._server = subprocess.Popen(srvargs, stdout=subprocess.DEVNULL)
        print("Server Created.")

    # creates the server
    def create_client(self, ip, port, password, playertypes):
            
        # create server args
        # note optional client argument given
        clargs = ["./client", ip, port, password, playertypes, "1"] 
            
        # create server
        self._client = subprocess.Popen(clargs, stdin = subprocess.PIPE, stdout = subprocess.PIPE)
        print("Client Created.")

if __name__ == '__main__':
    root = tkinter.Tk()
    app = Controller(root)
    root.resizable(0,0) #Remove maximise button
    root.geometry('760x760+150+150')
    root.mainloop()
