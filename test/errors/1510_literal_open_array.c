/* expect: 2: error: a compound literal of an array with no size needs a value to say how long it is */
int *p = (int[]){ };
int main(void) { return p[0]; }
