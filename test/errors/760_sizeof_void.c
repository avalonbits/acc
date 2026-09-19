/* expect: 4: error: 'void' has no size */
/* void is the type with no values, so it has no size either. */
int main(void) {
    return sizeof(void);
}
