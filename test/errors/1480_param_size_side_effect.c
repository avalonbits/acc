/* expect: 5:13: error: a parameter's array size is evaluated when the function is entered, and acc could not keep this one to do that */
/* One whose text is too long to keep: the rest are read again on entry,
 * as 282_vla_params checks. */
int n;
int f(int a[n++ + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n + n]) { return a[0]; }
int main(void) { int b[2] = {0}; return f(b); }
