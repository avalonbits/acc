/* expect: 2:12: error: a wide string initialises an array of wchar_t, and this is not one */
char c[] = L"x";
int main(void) { return c[0]; }
