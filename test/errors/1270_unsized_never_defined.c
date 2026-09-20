/* expect: error: 'a' is declared and used but never given room, so another file has to define it -- which needs the pieces linked together */
/* Nothing here reserves room for it and nothing here gives it a size, so
 * there is no address for the uses of it to be filled in with. Compiled to
 * an object it would be a symbol for the linker; compiled straight to a
 * program there is no linker coming. */
extern int a[];

int main(void) {
    return a[0];
}
