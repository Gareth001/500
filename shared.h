// reads from the fd until the newline character is found,
// and then returns this string (without newline)
char* read_from_fd(int fd, int length);

// returns 0 if the number of digits of i is not the same as
// the length of s
int check_input_digits(int i, char* s);
