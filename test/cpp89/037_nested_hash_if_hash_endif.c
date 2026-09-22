int check() {
    int r = 0;
#if 1
  #if 1
    r += 1;
  #endif
  #if 0
    r += 100;
  #endif
#endif
#if 0
  #if 1
    r += 200;
  #endif
#endif
    return (r == 1) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Nested #if/#endif
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
