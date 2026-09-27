/* expect: 4:19: error: 'a' has no size yet */
extern int a[];
int main(void) {
    return sizeof a;
}
int a[3];
