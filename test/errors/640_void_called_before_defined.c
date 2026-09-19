/* expect: 4: error: 'later' returns void and is called before it is defined, which declares it as returning int; move its definition above the call */
/* Called before its definition, it is taken to return int, as C says. */
int main(void) {
    later();
    return 0;
}

void later(void) {
}
