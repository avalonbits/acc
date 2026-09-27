/* expect: 2:9: error: a wide character constant past 0xffff does not fit in a wchar_t */
int c = L'\U0001F600';
int main(void) { return c; }
