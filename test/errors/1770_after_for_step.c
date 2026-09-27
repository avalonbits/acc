/* expect: 6:6: error: 'else' without an 'if' */
/* The token straight after a loop whose step is read again after the body:
 * its column is still its own. */
int main(void) {
    for (int i = 0; i < 3; i++) ;
	    else;
    return 0;
}
