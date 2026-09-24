/* expect: 4: error: an array type here needs its size */
typedef int list[];
int main(void) {
    return sizeof(list);
}
