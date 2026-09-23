/* expect: 2: error: a string initialises an array of char, and this is not one; a wide string, L"...", is for wchar_t */
short w[] = "x";
int main(void) { return w[0]; }
