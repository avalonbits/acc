/* expect: 6:38: error: a void function returns nothing, so its result cannot be used */
/* Nor as an operand. */
void nothing(void) {
}

int main(void) { return nothing() + 1; }
