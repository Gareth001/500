import subprocess
import os
import tkinter
from tkinter import messagebox

WIDTH = 760
HEIGHT = 760
BUFFER_LENGTH = 100

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
        
        while True:
            line = self._client.stdout.readline(BUFFER_LENGTH)
            print(line)

            if line == b'':
                break

            if line == b'Your bet: \r\n':
                self._client.stdin.write(b"PASS\n")
                self._client.stdin.flush()


    # when user presses host server
    def host(self):

        password = "pass"
        port = "2222"
        ptypes = "0222"

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
