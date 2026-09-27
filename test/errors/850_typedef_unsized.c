/* expect: 4:19: error: an array type here needs its size */
typedef int list[];
int main(void) {
    return sizeof(list);
}
