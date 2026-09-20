/* expect: 5: error: an address cannot become a floating-point value */
int g;

int main(void) {
    float f = (float) (int) &g;

    return (int) f;
}
