/*
 * acc's linker script: agondev's, with one line changed.
 *
 * agondev's config/linker.conf puts the heap's top and the stack's bottom at
 * the same address, the top of the 448 KB MOS gives a program:
 *
 *     __stack    = RAM_START + RAM_SIZE;
 *     ___heaptop = RAM_START + RAM_SIZE;
 *
 * So the two grow towards each other out of one region with nothing between
 * them. malloc hands out memory the stack is already standing in, and
 * nothing says so: the compiler carries on with a symbol table that a return
 * address has been written through, and what it does after that is anyone's
 * guess -- most often it does not come back at all.
 *
 * Here the heap stops ACC_STACK_RESERVE below the stack. The stack keeps
 * that much to itself, and a compile that needs more memory than is left
 * gets a null from malloc, which acc reports as running out of memory and
 * stops -- rather than carrying on over its own frames. libagon's sbrk is
 * what reads ___heaptop, and malloc is what reads sbrk.
 *
 * The size is measured rather than guessed: `make -f Makefile.agon STACK=1`
 * builds an acc that paints the reserve before a compile and says afterwards
 * how far down the paint was overwritten. On the Agon, of 16 KB reserved:
 *
 *   test/bench/big.c        391 bytes     ordinary code
 *   test/bench/records.c    622 bytes
 *   test/bench/matrix.c     961 bytes
 *   test/bench/features.c   996 bytes
 *   60 nested parens      3,066 bytes     and 40 nested ifs with it
 *   127 nested parens     9,059 bytes     C99's own nesting limits, with
 *                                         127 nested blocks as well
 *
 * So 16 KB is not far off twice what the deepest source C99 asks a compiler
 * to accept costs. This is still a guard for the heap's side only: a source
 * nested past those limits walks the stack into the heap, and acc does not
 * count how deep it has gone.
 *
 * Everything else here is agondev's, copied so that the one line can be
 * changed; test/heap.sh checks that nothing else has drifted from it.
 */

/*
    See a thorouhly commented linker file here:
    https://github.com/wntrblm/Castor_and_Pollux/blob/main/firmware/scripts/samd21g18a.ld
*/

ENTRY(__start)

/*
    RAM_START and RAM_SIZE need to be set previously
*/

MEMORY {
    USERRAM (rw) : ORIGIN = RAM_START, LENGTH = RAM_SIZE  /* Define RAM region */
}

SECTIONS
{
    . = RAM_START;

    .init : { *(.init .init.args .init.bss .fini) } > USERRAM

    .text : { *(.text) } > USERRAM

    .init_array : { *(.init_array) } > USERRAM

    .ctors : { *(.ctors) } > USERRAM

    .dtors : { *(.dtors) } > USERRAM

    .fini_array : { *(.fini_array) } > USERRAM

    .rodata : { *(.rodata) } > USERRAM

    .data : { *(.data) } > USERRAM

    .bss :
    {
        bss_start = .;
        *(.bss .bss*)
        bss_end = .;
    }
}

__stack              = RAM_START + RAM_SIZE;
___low_bss           = bss_start;
___len_bss           = SIZEOF(.bss);
___heapbot           = bss_end;
ACC_STACK_RESERVE    = 0x4000;          /* 16 KB; see the note above */
___heaptop           = RAM_START + RAM_SIZE - ACC_STACK_RESERVE;
___run_clearbss      = ___len_bss > 0;

/* symbols for initializing the init_array and ctors sections */
___init_array_count     = SIZEOF(.init_array) / 3;
___ctors_count          = SIZEOF(.ctors) / 3;
___dtors_count          = SIZEOF(.dtors) / 3;
___fini_array_count     = SIZEOF(.fini_array) / 3;
___init_count           = ___init_array_count + ___ctors_count;
___fini_count           = ___dtors_count + ___fini_array_count;
___run_init             = ___init_count > 0;
___run_fini             = ___fini_count > 0;
___init_count_a         = (((___init_count -1) >> 16) & 0xFF) + 1;
___init_count_b         = (((___init_count -1)      ) & 0xFF) + 1;
___init_count_c         = (((___init_count -1) >>  8) & 0xFF) + 1;
___fini_count_a         = (((___fini_count -1) >> 16) & 0xFF) + 1;
___fini_count_b         = (((___fini_count -1)      ) & 0xFF) + 1;
___fini_count_c         = (((___fini_count -1) >>  8) & 0xFF) + 1;
___init_array_functions = SIZEOF(.init_array) + ADDR(.init_array);
___init_ctors_functions = SIZEOF(.ctors) + ADDR(.ctors);
___fini_dtors_functions = SIZEOF(.dtors) + ADDR(.dtors);
___fini_array_functions = SIZEOF(.fini_array) + ADDR(.fini_array);
