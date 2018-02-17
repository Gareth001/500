CFLAGS = -Wall -pedantic -std=gnu99 -pthread

all: server client

server: server.o
	gcc $(CFLAGS) server.o -o server

server.o: server.c
	gcc $(CFLAGS) -c server.c

client: client.o
	gcc $(CFLAGS) client.o -o client

client.o: client.c
	gcc $(CFLAGS) -c client.c

clean: 
	rm server.o client.o