import subprocess
import os
import tkinter

WIDTH = 760
HEIGHT = 760

class Model(object):
    def __init__(self):

        # server
        self._server = None

    # compiles and creates the server
    def create_server(self, port, password, playertypes="0000", recompile=False):
        if os.name == "posix":

            # make and wait
            if recompile:
                args = ["make", "server", "-B"]
            else:
                args = ["make", "server"]

            make = subprocess.Popen(args, stdout=subprocess.DEVNULL)
            print("compiling")
            make.wait()

            # create server
            print("server created")
            self._server = subprocess.Popen(["./server", port, password, playertypes],
                    stdout=subprocess.DEVNULL)

    def exit(self):
        if self._server != None:
            self._server.terminate()
            print("server terminated")

class View(object):
    def __init__(self, master, bindings):

        # canvas setup
        self._canvas = tkinter.Canvas(master, bg='grey60',
                                 width=WIDTH, height=HEIGHT) #Create canvas
        self._canvas.pack(expand=1, padx=20, pady = 20) #Pack canvas
        self._canvas.focus_set()
        self._bindings = bindings

        host_button = tkinter.Button(self._canvas, text="Host & Play")
        host_button.bind("<ButtonPress>", self.host)
        host_button.pack()

    # change gui and execute callback
    def host(self, event):
        self._bindings[0]()


class Controller():
    def __init__(self, master):
        master.title("Sample Text") # title of window
        self._master = master

        # create model
        self._model = Model()

        # create view
        self._view = View(self._master, [self.host])

        # kill children on close
        self._master.protocol("WM_DELETE_WINDOW", self.game_exit)

    # when user presses host server
    def host(self):

        password = "test"
        port = "2222"
        ptypes = "0111"

        self._model.create_server(port, password, playertypes=ptypes)

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
