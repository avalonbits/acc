# `ctype.h`

What kind of character a byte is, and its letter in the other case:
C99's twelve classes and `tolower` and `toupper`, in the C locale, which
is the only one acc has. The classes are ASCII's: the letters are the
twenty-six either way, and nothing above 127 is in any class.

| Function   | Characters it answers 1 for                          |
|------------|------------------------------------------------------|
| `isdigit`  | `0` to `9`                                           |
| `isxdigit` | `0` to `9`, `a` to `f`, `A` to `F`                   |
| `islower`  | `a` to `z`                                           |
| `isupper`  | `A` to `Z`                                           |
| `isalpha`  | `a` to `z`, `A` to `Z`                               |
| `isalnum`  | `0` to `9`, `a` to `z`, `A` to `Z`                   |
| `isblank`  | space (32), tab (9)                                  |
| `isspace`  | space, tab, newline, vt, ff, cr (32, 9 to 13)        |
| `iscntrl`  | 0 to 31, and 127                                     |
| `isprint`  | 32 to 126: space and everything `isgraph` takes      |
| `isgraph`  | 33 to 126: `!` to `~`                                |
| `ispunct`  | what `isgraph` takes, less what `isalnum` takes      |

Each takes an `int`, which C99 says is a value that fits in an
`unsigned char`, or `EOF` (-1). Each answers 1 or 0, and 0 for `EOF`,
for every value from 128 to 255, and for any other number outside the
256 bytes, negative or past 255: they are comparisons rather than a
table, so no argument reads past anything. `tolower` and `toupper` give
back any of those unchanged.

A `char` is signed in acc, so a plain `char` past 127 arrives as a
negative number. That answers 0 here, and is still undefined in C; the
fix is the cast at the call:

    if (isalpha((unsigned char) *s))

They are functions rather than macros, so a program can take their
address. The Agon's screen font has characters above 127, but no class
here has any of them; a program that wants to sort those needs a table
of its own.

`wctype.h`'s `isw` functions, `towlower` and `towupper` are these under
wide names.

## `isalpha`, `isupper`, `islower`, `isdigit`, `isxdigit`, `isalnum`

    int isalpha(int c);
    int isupper(int c);
    int islower(int c);
    int isdigit(int c);
    int isxdigit(int c);
    int isalnum(int c);

Letters and digits. `isupper` answers 1 for `A` to `Z`, `islower` for
`a` to `z`, and `isalpha` for either. `isdigit` answers 1 for the ten
decimal digits `0` to `9`, `isxdigit` for those and the six hex letters
in either case, and `isalnum` for a letter or a decimal digit.

## `isspace`, `isblank`

    int isspace(int c);
    int isblank(int c);

White space. `isspace` answers 1 for a space (32) and for the five
characters from 9 to 13: tab, newline, vertical tab, form feed and
carriage return. `isblank` answers 1 for only the two that separate
words on a line, space and tab.

## `iscntrl`, `isprint`, `isgraph`, `ispunct`

    int iscntrl(int c);
    int isprint(int c);
    int isgraph(int c);
    int ispunct(int c);

`iscntrl` answers 1 for the control characters, 0 to 31 and 127 (DEL).
`isprint` answers 1 for the rest of ASCII, 32 to 126, and `isgraph` for
those but the space, 33 (`!`) to 126 (`~`). Every byte from 0 to 127 is
one of a control or a printing character, and no byte is both.

`ispunct` answers 1 for a character `isgraph` takes and `isalnum` does
not: the 32 of `!"#$%&'()*+,-./`, `:;<=>?@`, `[\]^_` and `` ` ``, and
`{|}~`.

## `tolower`, `toupper`

    int tolower(int c);
    int toupper(int c);

`tolower` answers the lower-case letter for `A` to `Z`, and `toupper`
the upper-case one for `a` to `z`. Anything else comes back as it was,
`EOF` and the values from 128 to 255 included.

    for (char *p = name; *p; p++)
        *p = toupper((unsigned char) *p);
