#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fd, int length) {
    
    char* buffer = malloc(length * sizeof(char));
    char* charBuffer = malloc(sizeof(char));
    
    buffer[0] = '\0';
    int i;
    
    for (i = 0; i < length - 1; i++) {
        
        read(fd, charBuffer, 1);
        
        if (charBuffer[0] != '\n') {
            buffer[i] = charBuffer[0];
            
        } else {
            break;
            
        }
        
    }
    
    free(charBuffer);
    buffer[i] = '\0';
    return buffer;
    
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
    return (length == strlen(s));
    
}
