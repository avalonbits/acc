/* expect: 2: error: '__func__' is not declared */
const char *name = __func__;
int main(void) { return name[0]; }
