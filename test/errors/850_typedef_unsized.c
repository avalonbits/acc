/* expect: 2: error: a typedef of an array needs the array's size */
typedef int list[];
int main(void) {
    return 0;
}
