/* expect: 3: error: a bit-field of 'long long' is not supported: C99 leaves the types past 'int' to the implementation */
struct s {
    long long a : 3;
};
int main(void) { return sizeof(struct s); }
