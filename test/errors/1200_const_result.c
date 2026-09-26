/* expect: 5:17: error: this is const, so it cannot be changed */
int cell;
const int *pick(void) { return &cell; }
int main(void) {
    *pick() += 1;
    return cell;
}
