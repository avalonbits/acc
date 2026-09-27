/* expect: 4:12: error: a character constant holds one character; for more, use a string */
/* C gives 'ab' a value only an implementation can define; acc does not. */
int main(void) {
    return 'ab';
}
