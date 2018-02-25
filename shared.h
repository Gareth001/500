#ifndef SHARED_H 
#define SHARED_H 

// definitions
#define BUFFER_LENGTH 100
#define MAX_PASS_LENGTH 20
#define MAX_NAME_LENGTH 20

// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fd, int length);

// returns 0 if the number of digits of i is not the same as
// the length of s
int check_input_digits(int i, char* s);

#endif /* SHARED_H */ 
