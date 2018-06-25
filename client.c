#include "shared.h"

// Exit Statuses
// 1: Wrong arguments
// 2: Wrong password
// 3: Invalid username
// 4: Connect failed
// 5: Unexpected exit

// function declaration
struct in_addr* name_to_IP_addr(char* hostname);
int connect_to(struct in_addr* ipAddress, int port);
void game_loop(int fileDes);
void send_input(int fileDes);

// round functions
void get_player_details(int fileDes);
void bet_round(int fileDes);
void kitty_round(int fileDes);
void joker_round(int fileDes);
void play_round(int fileDes);

//Test comment, James is in da house. Memes 2.00 This will work surely
// main
int main(int argc, char** argv) {

    // argument checking
    if (argc != 5) {
        fprintf(stderr, "Usage: client ipaddress port password username\n");
        return 1;

    }

    // port, validity is checked when connecting
    int port = atoi(argv[2]);

    // init socket
    socket_init();

    // convert hostname to IP address
    struct in_addr* ipAddress = name_to_IP_addr(argv[1]);

    // establish a connection to given port
    int fileDes = connect_to(ipAddress, port);

    // buffer, will be reused
    char* message = malloc(BUFFER_LENGTH * sizeof(char));

    // send the password
    {
        sprintf(message, "%s\n", argv[3]);
        write_new(fileDes, message, strlen(message));

    }
    
    // read yes from server, otherwise exit
    read_from_fd(fileDes, message, BUFFER_LENGTH, false);
    if (strcmp(message, "yes") != 0) {
        // exit
        fprintf(stderr, "Auth failed\n");
        close(fileDes);
        return 2;

    }

    // send player name
    {
        sprintf(message, "%s\n", argv[4]);
        write_new(fileDes, message, strlen(message));

    }

    // check if server sent yes, otherwise invalid username
    read_from_fd(fileDes, message, BUFFER_LENGTH, false);
    if (strcmp(message, "yes") != 0) {
        // exit
        fprintf(stderr, "Invalid username\n");

        // Close socket
        close(fileDes);
        return 3;

    }

    fprintf(stderr, "Connected successfully! Waiting for others\n");

    // receive start from server
    read_from_fd(fileDes, message, BUFFER_LENGTH, false);

    // tell them yes, we are still here
    write_new(fileDes, "yes\n", 4);

    // receive start from server which lets us know everyone is here
    // server will either send start\n or something else
    read_from_fd(fileDes, message, BUFFER_LENGTH, false);
    if (strcmp(message, "start") != 0) {
        // exit
        fprintf(stderr, "Unexpected exit\n");

        // Close socket
        close(fileDes);
        return 5;

    }

    // take us to the game!
    fprintf(stdout, "All players connected!\n");
    free(message);

    // get player details here, we only want to do this once per game
    get_player_details(fileDes);

    // begin game loop
    game_loop(fileDes);

}

// game loop.
void game_loop(int fileDes) {
    
    char* message = malloc(BUFFER_LENGTH * sizeof(char));
   
    // loop until a team gets to 500 points (or -500 points)
    while (1) {
        // print out all the cards from the server
        read_from_fd(fileDes, message, BUFFER_LENGTH, false);
        fprintf(stdout, "Game starting!\nYour hand: %s\n", message);

        fprintf(stdout, "Betting round starting\n");

        bet_round(fileDes);

        fprintf(stdout, "Waiting for Kitty\n");

        kitty_round(fileDes);

        // kitty is over now
        fprintf(stdout, "Kitty finished\n");

        // we need to choose the suit for the joker if we are doing no trumps
        // very similar to how kitty round is handled
        fprintf(stdout, "Choosing joker suit\n");

        joker_round(fileDes);

        // game begins
        fprintf(stdout, "Game Begins\n");

        play_round(fileDes);
        
        // get final result of the game
        read_from_fd(fileDes, message, BUFFER_LENGTH, false);
        fprintf(stdout, "%s\n", message);
        
        // get total points for both teams
        // points for my team
        read_from_fd(fileDes, message, BUFFER_LENGTH, false);
        fprintf(stdout, "Your teams points: %s\n", message);
        // points for other team
        read_from_fd(fileDes, message, BUFFER_LENGTH, false);
        fprintf(stdout, "Other teams points: %s\n", message);
                
        // see if game has ended or not (one team on |500| points)
        read_from_fd(fileDes, message, BUFFER_LENGTH, false);
        if (strcmp(message, "endgame") == 0) { 
            // game is over, break from loop
            break;
            
        }
        
    }
    
    // game finally over
    free(message);
    fprintf(stdout, "Game over!\n");
    close(fileDes);
    exit(0);

}

// gets the player details from the server and prints them
void get_player_details(int fileDes) {
    // here we first receive all the players names in the game from the server.
    fprintf(stdout, "Players Connected:\n");
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    for (int i = 0; i < 4; i++) {
        // print the players details from the server
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);
        fprintf(stdout, "%s\n", result);

    }

    free(result);

}

// handles the betting round for this player
void bet_round(int fileDes) {
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    while (1) {
        // server will either send bet or info they will print out or betover
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);

        if (strcmp(result, "bet") == 0) {
            fprintf(stdout, "Your bet: ");
            fflush(stdout);
            // send a bet from the player input
            send_input(fileDes);

        } else if (strcmp(result, "betover") == 0) {
            // betting round over
            break;

        } else if (strcmp(result, "badinput") == 0) {
            // bet ended unexpectedly, exit game
            fprintf(stderr, "Unexpected exit\n");
            close(fileDes);
            exit(5);

        } else if (strcmp("redeal", result) == 0) {
            fprintf(stdout, "Everyone passed. Game restarting.\n");
            game_loop(fileDes);

        } else {
            // server sent info on betting, print.
            fprintf(stdout, "%s\n", result);

        }

    }

    // result from betting round here
    read_from_fd(fileDes, result, BUFFER_LENGTH, false);
    fprintf(stdout, "%s\n", result);
    free(result);

}

// handles the kitty round
void kitty_round(int fileDes) {
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    while (1) {
        read_from_fd(fileDes, result, BUFFER_LENGTH, false); // kitty stuff

        if (strcmp(result, "send") == 0) {
            // get string of how many cards remain
            read_from_fd(fileDes, result, BUFFER_LENGTH, false);
            fprintf(stdout, "%s: ", result);
            fflush(stdout);

            // send input from user
            send_input(fileDes);

        } else if (strcmp(result, "kittyend") == 0) {
            // exit from the loop
            break;

        } else if (strcmp("badinput", result) == 0) {
            // game ended unexpectedly
            fprintf(stderr, "Unexpected exit\n");
            close(fileDes);
            exit(5);

        } else {
            // sends our deck with the kitty
            fprintf(stdout, "%s\n", result);

        }

    }

    free(result);

}

// handles the joker round
void joker_round(int fileDes) {
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    while (1) {
        read_from_fd(fileDes, result, BUFFER_LENGTH, false); // joker stuff

        if (strcmp(result, "jokerwant") == 0) {
            // print to user that we want the joker
            fprintf(stdout, "Choose joker suit: ");
            fflush(stdout);
            send_input(fileDes);

        } else if (strcmp(result, "jokerdone") == 0) {
            // exit from the loop
            break;

        } else if (strcmp("badinput", result) == 0) {
            // game ended unexpectedly
            fprintf(stderr, "Unexpected exit\n");
            close(fileDes);
            exit(5);

        } else {
            // server sent the joker suit here, we will get jokerdone next
            fprintf(stdout, "%s\n", result);

        }

    }

    free(result);
    
}

// handles the play round
void play_round(int fileDes) {
    char* result = malloc(BUFFER_LENGTH * sizeof(char));

    while (1) {

        // get message from server
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);
        if (strcmp(result, "gameover") == 0) {
            break;
            
        }
    
        // get hand from server
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);
        fprintf(stdout, "Your hand: %s\n", result);

        // get round number from server
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);
        fprintf(stdout, "%s\n", result);

        while (1) {
            read_from_fd(fileDes, result, BUFFER_LENGTH, false);

            if (strcmp("send", result) == 0) {
                // send card to server
                fprintf(stdout, "Choose card to play: ");
                fflush(stdout);
                send_input(fileDes);

            } else if (strcmp("roundover", result) == 0) {
                // round over
                break;

            } else if (strcmp("badinput", result) == 0) {
                // game ended unexpectedly
                fprintf(stderr, "Unexpected exit\n");
                close(fileDes);
                exit(5);

            } else {
                fprintf(stdout, "%s\n", result);

            }

        }

        // get number of tricks betting team has won, extra \n for readability
        read_from_fd(fileDes, result, BUFFER_LENGTH, false);
        fprintf(stdout, "%s\n\n", result);

    }

    free(result);

}

// sends input from stdin to the fileDes
void send_input(int fileDes) {
    char* buff = malloc(BUFFER_LENGTH * sizeof(char));
    fgets(buff, BUFFER_LENGTH, stdin);
    write_new(fileDes, buff, strlen(buff));
    free(buff);

}

// converts name to IP address
struct in_addr* name_to_IP_addr(char* hostname) {
    int error;
    struct addrinfo* addressInfo;
    error = getaddrinfo(hostname, NULL, NULL, &addressInfo);

    if (error) {
        return NULL;

    }

    // extract the IP address and return it
    return &(((struct sockaddr_in*)(addressInfo->ai_addr))->sin_addr);

}

// connects to the given ip address at the given port
int connect_to(struct in_addr* ipAddress, int port) {
    struct sockaddr_in socketAddr;
    int fileDes;

    // create socket - TCP socket IPv4
    fileDes = socket(AF_INET, SOCK_STREAM, 0);
    if (fileDes < 0) {
        fprintf(stderr, "Connect failed\n");
        exit(4);
    }

    // populate server address structure with IP address and port number
    socketAddr.sin_family = AF_INET;    // IPv4
    socketAddr.sin_port = htons(port);    // convert port num to network byte
    socketAddr.sin_addr.s_addr = ipAddress->s_addr; // IP address

    // attempt to connect to the server
    if (connect(fileDes, (struct sockaddr*)&socketAddr,
            sizeof(socketAddr)) < 0) {
                
        #ifdef _WIN32
            printf("%d", WSAGetLastError());
        #endif
             
        fprintf(stderr, "Connect failed\n");
        exit(4);

    }

    // now have connected socket
    return fileDes;
}
