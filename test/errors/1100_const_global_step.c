/* expect: 4: error: 'limit' is const, so it cannot be changed */
const int limit = 3;
int main(void) {
    limit++;
    return limit;
}
