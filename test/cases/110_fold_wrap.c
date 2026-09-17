/* The same overflow, but in constants acc folds at compile time rather than
   in locals it computes at run time. acc runs on a 32-bit host when it is
   cross-compiling, so folding in the host's int gets a different answer from
   the machine's. */
int main(void) { return (8388607 + 1) + 8388649; }
