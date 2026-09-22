/* The macros a compiler has defined before it reads a line.
 *
 * None of them is in C, and a program cannot do without them: <stdint.h>
 * is written in terms of them, and so is any program that wants an integer
 * of a named width with no header to get it from. What they say is how wide
 * everything on this machine is, so the two compilers have to agree to the
 * letter -- which is what this case is for, being compiled by both.
 *
 * Of the gcc torture tests acc would not take, more were turned down for
 * the want of these than for anything else: ninety-eight of them.
 */
__SIZE_TYPE__    a_size;
__PTRDIFF_TYPE__ a_diff;
__WCHAR_TYPE__   a_wide;
__INTPTR_TYPE__  a_ptr;
__UINTPTR_TYPE__ an_uptr;
__INT8_TYPE__    i8;
__UINT8_TYPE__   u8;
__INT16_TYPE__   i16;
__UINT16_TYPE__  u16;
__INT32_TYPE__   i32;
__UINT32_TYPE__  u32;
__INT64_TYPE__   i64;
__UINT64_TYPE__  u64;
__INTMAX_TYPE__  imax;
__UINTMAX_TYPE__ umax;

int main(void) {
    int r = 0;

    /* A type macro is a type, so it can be declared with and measured. */
    if (sizeof a_size == 3 && sizeof a_diff == 3 && sizeof a_wide == 2) r++;
    if (sizeof a_ptr == 3 && sizeof an_uptr == 3) r++;
    if (sizeof i8 == 1 && sizeof u8 == 1 && sizeof i16 == 2 && sizeof u16 == 2) r++;
    if (sizeof i32 == 4 && sizeof u32 == 4 && sizeof i64 == 8 && sizeof u64 == 8) r++;
    if (sizeof imax == 8 && sizeof umax == 8) r++;

    /* And it is the signedness it says, not merely the width. */
    i8 = -1; u8 = (__UINT8_TYPE__) -1;
    if (i8 < 0 && u8 > 0) r++;
    i32 = -1; u32 = (__UINT32_TYPE__) -1;
    if (i32 < 0 && u32 > 0) r++;

    /* What each one holds. */
    if (__CHAR_BIT__ == 8 && __SCHAR_MAX__ == 127 && __SHRT_MAX__ == 32767) r++;
    if (__INT_MAX__ == 8388607 && __LONG_MAX__ == 2147483647L) r++;
    if (__LONG_LONG_MAX__ == 9223372036854775807LL) r++;

    /* How wide each is, which has to agree with sizeof for the same type. */
    if (__SIZEOF_SHORT__ == sizeof(short) && __SIZEOF_INT__ == sizeof(int)) r++;
    if (__SIZEOF_LONG__ == sizeof(long)
        && __SIZEOF_LONG_LONG__ == sizeof(long long)) r++;
    if (__SIZEOF_POINTER__ == sizeof(void *)
        && __SIZEOF_SIZE_T__ == sizeof a_size) r++;
    if (__SIZEOF_FLOAT__ == sizeof(float)
        && __SIZEOF_DOUBLE__ == sizeof(double)) r++;

    /* Which end the low byte is at, named rather than numbered. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    r++;
#endif
    {
        unsigned long word = 1;

        if (*(unsigned char *) &word == 1) r++;     /* and it really is */
    }

    /* They are macros, so a program may say it would rather not have one.
     * gcc allows that, and a program that defines its own must be able to. */
#undef __INT_MAX__
#define __INT_MAX__ 5
    if (__INT_MAX__ == 5) r++;

    return r + 25;              /* 17 checks */
}
