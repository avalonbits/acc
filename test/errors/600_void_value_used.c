/* expect: 7: error: a void function returns nothing, so its result cannot be used */
/* A void function has no result to use. */
void nothing(void) {
}

int main(void) {
    int n = nothing();
    return n;
}
