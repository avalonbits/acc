/* expect: 4: error: 'later' is called and not declared */
/* Called before its definition, it is taken to return int, as C says. */
int main(void) {
    later();
    return 0;
}

void later(void) {
}
