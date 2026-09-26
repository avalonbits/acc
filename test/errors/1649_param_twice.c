/* expect: 3:7: error: 'a' is already declared */
int f(int a,
      char a) { return a; }
int main(void) { return f(1, 2); }
