CFLAGS = -Wall -pedantic -std=gnu99 -pthread

all: server client

train-job: server.o
	gcc $(CFLAGS) server.o -o server

train-job.o: server.c
	gcc $(CFLAGS) -c server.c

train-job: client.o
	gcc $(CFLAGS) client.o -o client

train-job.o: client.c
	gcc $(CFLAGS) -c client.c

	
clean: 
	rm server.o client.o