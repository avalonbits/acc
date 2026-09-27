/* expect: 5:27: error: an initial value here would put bytes inside the array */
/* An element whose value leaves bytes in the image, in an array built where
 * the output is rather than in the initialiser's buffer: the string would go
 * between two of the array's elements. */
int v[2] = { 1, (int) "x" };

int main(void) {
    return v[0];
}
