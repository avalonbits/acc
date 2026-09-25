/* A call to a function defined further down, from inside an assignment.
 *
 * The declaration in main's body creates the symbol for f -- it was the
 * call that did, when a call could declare a function -- in the middle of
 * main's body. Pushed as if it were one of main's locals it is dropped at
 * main's closing brace, and the definition below then makes a second, unrelated
 * symbol -- leaving the fixup for the call pointing at whatever later occupied
 * that slot. Here that was a parameter, whose value read as an address of 6.
 *
 * `return f(41);` does not show it: with nothing else on the stack the slot
 * happens to be reused by f itself and the wrong index is right anyway. It
 * takes a local in between.
 */
int main(void) {
    int f(int);                 /* declared in the block, as C99 asks */
    int s;
    s = f(41);

    return s;
}

int f(int a) { return a + 1; }
