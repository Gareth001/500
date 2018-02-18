CFLAGS = -Wall -pedantic -std=gnu99 -pthread

all: server client

server: server.c shared.o shared.h
	gcc $(CFLAGS) -o server shared.o server.c
    
client: client.c shared.o shared.h
	gcc $(CFLAGS) -o client shared.o client.c

clean: 
	rm server.o client.o