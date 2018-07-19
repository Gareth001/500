CFLAGS = -Wall -pedantic -std=gnu99 -Werror

ifeq ($(OS),Windows_NT)
    CFLAGS += -lws2_32
endif

SERVER = server.c shared.o cards.o bot.o
CLIENT = client.c shared.o

all: server client

server: $(SERVER)
	$(CC) -o server $(SERVER) $(CFLAGS)
    
client: $(CLIENT)
	$(CC) -o client $(CLIENT) $(CFLAGS)