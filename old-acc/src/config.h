/* acc build configuration.
 *
 * tinycc generates this file with ./configure. acc does not: the two builds
 * that matter here are selected on the compile line (-DTCC_TARGET_I386 for the
 * validation build, -DTCC_TARGET_EZ80 for the cross-compiler), and everything
 * else about the configuration is fixed. A generated file would be one more
 * thing that can disagree with the Makefile.
 */
#define TCC_VERSION "0.9.28rc"
#define ACC_VERSION "0.1.0-dev"

#ifndef CONFIG_TCCDIR
#define CONFIG_TCCDIR "/usr/local/lib/acc"
#endif

#define CONFIG_TCC_PREDEFS 1

/* No backtrace, no bound checking, no threads. None of the three can work on
 * a machine with no signals, no MMU and one thread, and all three cost space
 * that the 448 KB budget does not have. */
#define CONFIG_TCC_BACKTRACE 0
#define CONFIG_TCC_BCHECK 0
#define CONFIG_TCC_SEMLOCK 0

#ifdef TCC_TARGET_EZ80
/* No dynamic linking on the Agon: no dlopen, no shared objects, no ld.so.
 * This is what keeps <dlfcn.h> out of tcc.h and the -run machinery out of the
 * binary. */
#define CONFIG_TCC_STATIC 1
#endif
