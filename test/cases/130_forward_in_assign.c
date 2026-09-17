/* A call to a function defined further down, from inside an assignment.
 *
 * The call creates the symbol for f, and it creates it in the middle of
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
    int s;
    s = f(41);

    return s;
}

int f(int a) { return a + 1; }
