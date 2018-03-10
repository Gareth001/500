#include "shared.h"

// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fd, int length) {
    
    char* buffer = malloc(length * sizeof(char));
    char* charBuffer = malloc(sizeof(char));
    
    buffer[0] = '\0';
    int i;
    
    for (i = 0; i < length - 1; i++) {

        if (read(fd, charBuffer, 1) != 1) {
            // exit on bad read
            fprintf(stderr, "Unexpected exit\n");
            exit(5);
            
        } else if (charBuffer[0] != '\n') {
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
