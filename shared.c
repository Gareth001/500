#include "shared.h"

// reads from the fileDes until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fileDes, int length) {

    char* buffer = malloc(length * sizeof(char));
    char* charBuffer = malloc(sizeof(char));

    buffer[0] = '\0';
    int count;

    for (count = 0; count < length - 1; count++) {
        
        if (read_new(fileDes, charBuffer, 1) <= 0) {
            // exit on bad read
            fprintf(stderr, "Unexpected exit\n");
            exit(5);

        } else if (charBuffer[0] == '\n') {
            break;

        } else {
            buffer[count] = charBuffer[0];

        }

    }

    free(charBuffer);
    buffer[count] = '\0';
    return buffer;

}

// read multiplatform, return negative if error
int read_new(int fileDes, char* buffer, int length) {
    
    int val = -1;
    #ifdef _WIN32
        val = recv(fileDes, buffer, length, 0);
    
    #else
        val = read(fileDes, buffer, length);
        
    #endif
    
    return val;

}

// write multiplatform, return negative if error
int write_new(int fileDes, char* buffer, int length) {
        
    int val = -1;
    #ifdef _WIN32
        val = send(fileDes, buffer, length, 0);
    
    #else
        val = write(fileDes, buffer, length);
        
    #endif
    
    return val;

}

// initialise socket for cross platform
int socket_init(void) {
    #ifdef _WIN32
        WSADATA wsa_data;
        return WSAStartup(MAKEWORD(1,1), &wsa_data);
        
    #else
        return 0;
    
    #endif
    
}

// returns 0 if the number of digits of i is not the same as
// the length of s
int check_input_digits(int i, char* s) {
    // get digits of i
    int dupI = i;
    if (!dupI) {
        dupI++;
    }
    int length = 0;
    while(dupI) {
        dupI /= 10;
        length++;
    }
    // compare to length of string
    return (unsigned int)length == strlen(s);

}
