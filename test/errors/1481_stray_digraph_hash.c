/* expect: 2:12: error: stray '%:' in the source: it is '#', which is only a directive first on its line */
int x = 1; %:define Y 2
int main(void) { return x; }
