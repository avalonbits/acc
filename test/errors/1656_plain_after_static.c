/* expect: 3: error: 'z' was declared static, and this declaration is not */
static int z;
int z;
int main(void) { return z; }
