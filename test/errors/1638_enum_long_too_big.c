/* expect: an enum constant has to fit in an int */
enum e { big = 8388608L };
int main(void) { return 0; }
