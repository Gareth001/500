import subprocess
import os
import tkinter
from tkinter import messagebox

WIDTH = 760
HEIGHT = 760

class Model(object):
    def __init__(self):

        # server
        self._server = None

    # compiles and creates the server
    def create_server(self, port, password, playertypes, recompile=False):

        # create make args
        args = ["make", "server"]
        if recompile:
            args.append("-B")
            
        # try to make
        try:
            make = subprocess.Popen(args, stdout=subprocess.DEVNULL)
            print("compiling")
            make.wait()
            print("compiled")
            
        except:
            print("Make not found. Searching for precompiled binaries.")
            
        # create server args
        if os.name == "nt":
            srvargs = ["server.exe", port, password, playertypes]
                
        else: # this should be the same for many other os
            srvargs = ["./server", port, password, playertypes]
            
        # create server
        if os.path.exists(srvargs[0]):
            self._server = subprocess.Popen(srvargs, stdout=subprocess.DEVNULL)
            print("Server Created.")
            
        else:
            print("No precompiled binaries found.")
        
    def exit(self):
        if self._server != None:
            self._server.terminate()
            print("server terminated")

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

        # create model
        self._model = Model()

        # create view
        self._view = View(self._master, [self.join, self.host, self.game_exit])

        # kill children on close
        self._master.protocol("WM_DELETE_WINDOW", self.game_exit)

    # when user presses join
    def join(self):
        print("join")
        
    # when user presses host server
    def host(self):

        password = "test"
        port = "2222"
        ptypes = "0111"

        self._model.create_server(port, password, ptypes)

    # ensure we exit only after cleaning up
    def game_exit(self):
        self._model.exit()
        self._master.destroy()

if __name__ == '__main__':
    root = tkinter.Tk()
    app = Controller(root)
    root.resizable(0,0) #Remove maximise button
    root.geometry('760x760+150+150')
    root.mainloop()
