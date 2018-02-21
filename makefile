CFLAGS = -Wall -pedantic -std=gnu99 -pthread
DEPS = shared.o shared.h cards.o cards.h

all: server client

server: server.c $(DEPS)
	gcc $(CFLAGS) -o server $(DEPS) server.c
    
client: client.c $(DEPS)
	gcc $(CFLAGS) -o client $(DEPS) client.c

clean: 
	rm server.o client.o