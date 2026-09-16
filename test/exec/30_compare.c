/* expect:
lt 1 0 0
le 1 1 0
gt 0 0 1
ge 0 1 1
eq 0 1 0
ne 1 0 1
u: 1 0 1 0
neg: 1 0 1 0
*/
int printf(const char *, ...);
int lt(int a,int b){return a<b;} int le(int a,int b){return a<=b;}
int gt(int a,int b){return a>b;} int ge(int a,int b){return a>=b;}
int eq(int a,int b){return a==b;} int ne(int a,int b){return a!=b;}
int ult(unsigned a,unsigned b){return a<b;} int ugt(unsigned a,unsigned b){return a>b;}
int main(void) {
    printf("lt %d %d %d\r\n", lt(1,2), lt(2,2), lt(3,2));
    printf("le %d %d %d\r\n", le(1,2), le(2,2), le(3,2));
    printf("gt %d %d %d\r\n", gt(1,2), gt(2,2), gt(3,2));
    printf("ge %d %d %d\r\n", ge(1,2), ge(2,2), ge(3,2));
    printf("eq %d %d %d\r\n", eq(1,2), eq(2,2), eq(3,2));
    printf("ne %d %d %d\r\n", ne(1,2), ne(2,2), ne(3,2));
    printf("u: %d %d %d %d\r\n", ult(1,2), ult(2,1), ugt(2,1), ugt(1,2));
    /* the signed cases that need __setflag: a negative against a positive */
    printf("neg: %d %d %d %d\r\n", lt(-1,1), lt(1,-1), gt(1,-1), gt(-1,1));
    return 0;
}
