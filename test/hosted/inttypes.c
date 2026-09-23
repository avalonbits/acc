/* <inttypes.h>: each printf macro with a value of its type, and the
 * intmax_t functions, narrow and wide. */
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>

int main(void)
{
    int8_t s8 = -100;
    uint8_t u8 = 200;
    int16_t s16 = -30000;
    uint16_t u16 = 60000;
    int32_t s32 = -2000000000L;
    uint32_t u32 = 4000000000UL;
    int64_t s64 = -9000000000000000000LL;
    uint64_t u64 = 18000000000000000000ULL;
    intmax_t sm = INTMAX_MIN;
    uintmax_t um = UINTMAX_MAX;
    int_least8_t l8 = -5;
    int_fast16_t f16 = 1234;
    uint_least32_t lu32 = 0xdeadbeefUL;
    uint_fast64_t fu64 = 0x123456789abcdefULL;
    intptr_t ip = -77;
    uintptr_t up = 0xabcd;
    imaxdiv_t d;
    char *end;
    wchar_t *wend;
    static const wchar_t wide[] = L"  -123456789012345678z";

    printf("%" PRId8 " %" PRIi8 " %" PRIu8 " %" PRIo8 " %" PRIx8 " %" PRIX8 "\n",
           s8, s8, u8, u8, u8, u8);
    printf("%" PRId16 " %" PRIu16 " %" PRIx16 " %" PRIX16 "\n", s16, u16, u16, u16);
    printf("%" PRId32 " %" PRIu32 " %" PRIo32 " %" PRIx32 "\n", s32, u32, u32, u32);
    printf("%" PRId64 " %" PRIu64 " %" PRIx64 " %" PRIX64 "\n", s64, u64, u64, u64);
    printf("%" PRIdMAX " %" PRIuMAX " %" PRIxMAX "\n", sm, um, um);
    printf("%" PRIdLEAST8 " %" PRIdFAST16 " %" PRIxLEAST32 " %" PRIxFAST64 "\n",
           l8, f16, lu32, fu64);
    printf("%" PRIdPTR " %" PRIxPTR " %" PRIXPTR "\n", ip, up, up);

    printf("imaxabs %" PRIdMAX " %" PRIdMAX "\n", imaxabs(-42), imaxabs(INTMAX_MAX));
    d = imaxdiv(-9000000000000000007LL, 1000);
    printf("imaxdiv %" PRIdMAX " %" PRIdMAX "\n", d.quot, d.rem);

    errno = 0;
    printf("strtoimax %" PRIdMAX, strtoimax(" -0x7fffffffffffffffz", &end, 0));
    printf(" [%s] %d\n", end, errno);
    printf("strtoimax over %d %d\n",
           strtoimax("99999999999999999999", &end, 10) == INTMAX_MAX, errno == ERANGE);
    errno = 0;
    printf("strtoumax %" PRIuMAX " %d\n", strtoumax("18446744073709551615", &end, 10),
           errno);
    printf("wcstoimax %" PRIdMAX, wcstoimax(wide, &wend, 10));
    printf(" %d\n", (int) (wend - wide));
    printf("wcstoumax %" PRIuMAX, wcstoumax(L"0777 ", &wend, 0));
    printf(" %d\n", *wend == L' ');
    printf("wcstoumax none %" PRIuMAX, wcstoumax(L"xyz", &wend, 0));
    printf(" %d\n", *wend == L'x');

    return 0;
}
