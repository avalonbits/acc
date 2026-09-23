/* C99 6.10.9's _Pragma operator.
 *
 * It takes a string, which may come from a macro's # -- the one way a
 * pragma can come out of a macro at all -- and leaves nothing behind, at
 * file scope and in the middle of a statement list alike. An unknown one is
 * ignored, as C says an unknown pragma is. (The values of __STDC_VERSION__
 * and __STDC_HOSTED__ are checked in test/lib.sh: the reference build runs
 * agondev at its own default standard, which is not C99.) */
#define PRAGMA(x) _Pragma(#x)

_Pragma("STDC FP_CONTRACT OFF")
PRAGMA(STDC CX_LIMITED_RANGE ON)

int main(void) {
    int r = 0;

    _Pragma("anything at all") r++;
    PRAGMA(unknown to everyone) r++;
    r++; _Pragma("once more, with nothing after it")

    return r + 39;              /* 3 checks */
}
