CFLAGS = -Wall -pedantic -std=gnu99
DEPS = shared.o shared.h cards.o cards.h bot.c bot.h
DEPS2 = shared.o shared.h

all: server client

server: server.c $(DEPS)
	gcc $(CFLAGS) -o server $(DEPS) server.c
    
client: client.c $(DEPS2)
	gcc $(CFLAGS) -o client $(DEPS2) client.c

clean: 
	rm server.o client.o