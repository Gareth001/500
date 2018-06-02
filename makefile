CFLAGS = -Wall -pedantic -std=gnu99 -Werror

ifeq ($(OS),Windows_NT)
    CFLAGS += -lws2_32
endif

DEPS = shared.o shared.h cards.o cards.h bot.c bot.h
DEPS2 = shared.o shared.h

all: server client

server: server.c $(DEPS)
	gcc -o server $(DEPS) server.c $(CFLAGS)
    
client: client.c $(DEPS2)
	gcc -o client $(DEPS2) client.c $(CFLAGS)

clean: 
	rm server.o client.o