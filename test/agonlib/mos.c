/* <agon/mos.h> against libagon, under MOS 3.0.2: every call both have,
 * with what each answers, and what it did to the card and to the buffers it
 * was given. Where a call answers something that differs between runs --
 * the clock, an address -- what is printed is only whether it is sensible.
 * The calls acc has and libagon does not are at the end, under #ifndef
 * AGONDEV, with libagon's build printing what they should say.
 *
 * Some of libagon's are wrong, and are held to what MOS says instead, the
 * same way: its mos_getError restarts the machine, its ffs_ftruncate stops
 * it with MOS's RST 38 panic, its mos_uputc answers 0 when the byte went
 * and 1 when the port is shut -- MOS sets the carry for a byte that went,
 * and libagon's own header says 0 is the error -- its sd_getunlockcode
 * answers something different each time, where MOS makes one code and
 * keeps it, and its sd_init is refused the code it is given. The
 * emulator has no card for raw blocks, so sd_init fails in both once the
 * code is right, and sd_readblocks is not asked: MOS 3.0.2's own check of
 * the code reads the wrong bytes, which lib/mos.c says more about. */
#include <stdio.h>
#include <string.h>

#include <agon/mos.h>

static char buf[512], buf2[512];

static void show(const char *what, int v)
{
    printf("%s %d\n", what, v);
}

static void text(const char *what, const char *s)
{
    int i;

    printf("%s [", what);
    for (i = 0; s && s[i] && i < 60; i++)
        putchar(s[i] >= ' ' && s[i] < 127 ? s[i] : '?');
    printf("]\n");
}

static void files(void)
{
    uint8_t fh;
    int i, n;
    FIL *fil;

    for (i = 0; i < 300; i++)
        buf[i] = (char) (i * 7 + 3);
    show("save", mos_save("t_a.bin", buf, 300));
    memset(buf2, 0, sizeof buf2);
    show("load", mos_load("t_a.bin", buf2, sizeof buf2));
    show("loaded same", memcmp(buf, buf2, 300) == 0);
    show("load missing", mos_load("t_nothere.bin", buf2, sizeof buf2));
    show("copy", mos_copy("t_a.bin", "t_b.bin"));
    show("ren", mos_ren("t_b.bin", "t_c.bin"));
    show("mkdir", mos_mkdir("t_d"));
    show("mkdir again", mos_mkdir("t_d"));
    show("cd", mos_cd("t_d"));
    show("getcwd", ffs_getcwd(buf2, sizeof buf2));
    text("cwd", buf2);
    show("cd up", mos_cd(".."));
    show("del", mos_del("t_c.bin"));
    show("del missing", mos_del("t_c.bin"));
#ifndef AGONDEV
    mos_getError(4, buf2, sizeof buf2);
    text("error 4", buf2);
    mos_getError(8, buf2, 10);
    text("error 8 in 10", buf2);
#else
    text("error 4", "Could not find file");
    text("error 8 in 10", "Access de find file");
#endif

    fh = mos_fopen("t_e.txt", FA_WRITE | FA_CREATE_ALWAYS);
    show("fopen", fh != 0);
    mos_fputc(fh, 'H');
    mos_fputc(fh, 'i');
    show("fwrite", (int) mos_fwrite(fh, "!\nsecond line\n", 14));
    fil = mos_getfil(fh);
    show("getfil fptr", fil ? (int) fil->fptr : -1);
    show("fclose", mos_fclose(fh));
    fh = mos_fopen("t_e.txt", FA_READ);
    for (n = 0; n < 4; n++) {
        char c = mos_fgetc(fh);

        printf("fgetc %d eof %d\n", c, mos_feof(fh));
    }
    show("flseek", mos_flseek(fh, 5));
    memset(buf2, 0, sizeof buf2);
    show("fread", (int) mos_fread(fh, buf2, 100));
    text("read", buf2);
    show("eof", mos_feof(fh));
    show("flseek back", mos_flseek(fh, 1));
    printf("then %c\n", mos_fgetc(fh));
    show("fclose", mos_fclose(fh));
    show("fopen missing", mos_fopen("t_nothere.txt", FA_READ));
    show("del e", mos_del("t_e.txt"));
    show("del a", mos_del("t_a.bin"));
}

static void ffs(void)
{
    static FIL f;
    static DIR d;
    static FILINFO fi;
    uint32_t n32, free_clusters, cluster_size, serial;
    uint8_t r;
    char label[24];

    show("ffs_fopen", ffs_fopen(&f, "t_f.txt", FA_WRITE | FA_READ | FA_CREATE_ALWAYS));
    show("ffs_fwrite", (int) ffs_fwrite(&f, "abcdef\n", 7));
    show("ffs_fputc", (int) ffs_fputc(&f, 'X'));
    show("ffs_fputs", (int) ffs_fputs(&f, "yz\nlast"));
    show("ffs_fprintf", ffs_fprintf(&f, "|%d-%s-%x|", 42, "str", 255));
    show("ffs_fsync", ffs_fsync(&f));
    r = ffs_ftell(&f, &n32);
    printf("ffs_ftell %d %ld\n", r, (long) n32);
    r = ffs_fsize(&f, &n32);
    printf("ffs_fsize %d %ld\n", r, (long) n32);
    show("ffs_feof", ffs_feof(&f));
    show("ffs_ferror", ffs_ferror(&f));
    show("ffs_flseek", ffs_flseek(&f, 0));
    memset(buf2, 0, sizeof buf2);
    text("ffs_fgets", (char *) ffs_fgets(&f, buf2, sizeof buf2));
    memset(buf2, 0, sizeof buf2);
    show("ffs_fread", (int) ffs_fread(&f, buf2, 5));
    text("read", buf2);
    n32 = 3;
    show("ffs_flseek_p", ffs_flseek_p(&f, &n32));
#ifndef AGONDEV
    show("ffs_ftruncate", ffs_ftruncate(&f));
    r = ffs_fsize(&f, &n32);
    printf("ffs_fsize after truncate %d %ld\n", r, (long) n32);
#else
    show("ffs_ftruncate", 0);
    printf("ffs_fsize after truncate 0 3\n");
    ffs_fclose(&f);             /* what the truncate leaves: three bytes */
    ffs_fopen(&f, "t_f.txt", FA_WRITE | FA_READ | FA_CREATE_ALWAYS);
    ffs_fwrite(&f, "abc", 3);
#endif
    show("ffs_fclose", ffs_fclose(&f));

    show("ffs_stat", ffs_stat(&fi, "t_f.txt"));
    printf("stat size %ld attr %d name [%s]\n", (long) fi.fsize, fi.fattrib & AM_DIR,
           fi.fname);
    show("ffs_stat missing", ffs_stat(&fi, "t_nothere"));
    show("ffs_mkdir", ffs_mkdir("t_g"));
    show("ffs_rename", ffs_rename("t_f.txt", "t_g/t_h.txt"));
    show("ffs_chdir", ffs_chdir("t_g"));
    show("ffs_getcwd", ffs_getcwd(buf2, sizeof buf2));
    text("cwd", buf2);
    show("ffs_chdir up", ffs_chdir("/"));
    show("ffs_dopen", ffs_dopen(&d, "t_g"));
    for (;;) {
        r = ffs_dread(&d, &fi);
        if (r || !fi.fname[0])
            break;
        printf("dread %d [%s] %ld\n", r, fi.fname, (long) fi.fsize);
    }
    show("ffs_dclose", ffs_dclose(&d));
    show("ffs_dfindfirst", ffs_dfindfirst(&d, &fi, "t_g", "*.txt"));
    text("found", fi.fname);
    show("ffs_dfindnext", ffs_dfindnext(&d, &fi));
    text("then", fi.fname);
    ffs_dclose(&d);
    show("ffs_unlink", ffs_unlink("t_g/t_h.txt"));
    show("ffs_unlink dir", ffs_unlink("t_g"));
    show("ffs_unlink d", ffs_unlink("t_d"));
    r = ffs_getfree(NULL, &free_clusters, &cluster_size);
    printf("ffs_getfree %d %d %d\n", r, free_clusters > 0, cluster_size > 0);
    memset(label, 0, sizeof label);
    r = ffs_getlabel(NULL, label, &serial);
    printf("ffs_getlabel %d\n", r);
}

static void strings(void)
{
    char *a, *e, *next, *result;
    uint24_t n, len;
    char s[64];
    uint8_t r;

    show("pmatch", mos_pmatch("a*c", "abbbc", 0));
    show("pmatch no", mos_pmatch("a*c", "abbbd", 0));
    show("pmatch case", mos_pmatch("A?C", "abc", 0));
    strcpy(s, "  first second  third");
    mos_getargument(&a, &e, s, 1);
    printf("getargument 1: %d %d\n", a ? (int) (a - s) : -1, e ? (int) (e - s) : -1);
    mos_getargument(&a, &e, s, 3);
    printf("getargument 3: %d %d\n", a ? (int) (a - s) : -1, e ? (int) (e - s) : -1);
    mos_getargument(&a, &e, s, 9);
    printf("getargument 9: %d\n", a ? (int) (a - s) : -1);
    strcpy(s, "alpha,beta gamma");
    r = mos_extractstring(&result, &next, s, ",", 0);
    printf("extractstring %d %d %d\n", r, (int) (result - s), (int) (next - s));
    r = mos_extractstring(&result, &next, next, NULL, 0);
    printf("extractstring %d %d %d\n", r, (int) (result - s), (int) (next - s));
    strcpy(s, "123 &ff -7 x");
    n = 0;
    r = mos_extractnumber(&n, &next, s, " ", 0);
    printf("extractnumber %d %d %d\n", r, r ? -1 : (int) n, (int) (next - s));
    r = mos_extractnumber(&n, &next, next, " ", 0);
    printf("extractnumber %d %d %d\n", r, r ? -1 : (int) n, (int) (next - s));
    r = mos_extractnumber(&n, &next, next, " ", 0);
    printf("extractnumber %d %d\n", r, r ? -1 : (int) n);
    r = mos_extractnumber(&n, &next, "x", " ", 0);
    printf("extractnumber bad %d\n", r);
    r = mos_escapestring(&len, "a\tb\x01", buf2, sizeof buf2);
    printf("escapestring %d %d ", r, (int) len);
    text("", buf2);
}

static void variables(void)
{
    char *name = NULL, *trans = NULL;
    uint8_t type = 0;
    int r, length, read;
    char c;

    type = 0;                   /* a string */
    r = mos_setvarval("TestVar", "hello world", &name, &type);
    printf("setvarval %d type %d\n", r, type);
    type = 1;                   /* a number */
    name = NULL;
    r = mos_setvarval("TestNum", (void *) 1234, &name, &type);
    printf("setvarval num %d type %d\n", r, type);
    name = NULL;
    length = sizeof buf2;
    type = 0;
    memset(buf2, 0, sizeof buf2);
    r = mos_readvarval("TestVar", buf2, &name, &length, &type);
    printf("readvarval %d length %d type %d ", r, length, type);
    text("", buf2);
    name = NULL;
    length = sizeof buf2;
    type = 3;
    memset(buf2, 0, sizeof buf2);
    r = mos_readvarval("TestNum", buf2, &name, &length, &type);
    printf("readvarval num %d length %d type %d ", r, length, type);
    text("", buf2);
    name = NULL;
    length = 0;
    type = 0;
    r = mos_readvarval("NoSuchVar", NULL, &name, &length, &type);
    printf("readvarval missing %d\n", r);

    r = mos_gsinit("x<TestNum>|Ay", &trans, 0);
    printf("gsinit %d:", r);
    while (mos_gsread(&c, &trans) == 0 && c)
        printf(" %d", c);
    printf("\n");
    memset(buf2, 0, sizeof buf2);
    r = mos_gstrans("<TestVar>!", buf2, sizeof buf2, &read, 0);
    printf("gstrans %d read %d ", r, read);
    text("", buf2);
    memset(buf2, 0, sizeof buf2);
    r = mos_substituteargs("[%1|%0|%*1]", "one two three", buf2, sizeof buf2, 0);
    printf("substituteargs %d ", r);
    text("", buf2);
    type = 255;
    name = NULL;
    printf("delete %d\n", mos_setvarval("TestVar", NULL, &name, &type));
}

static void paths(void)
{
    uint8_t index = 0;
    int r, length;

    length = sizeof buf2;
    memset(buf2, 0, sizeof buf2);
    r = mos_resolvepath("mos/*.bin", buf2, &length, &index, NULL, 0);
    printf("resolvepath %d index %d length %d ", r, index, length);
    text("", buf2);
    length = sizeof buf2;
    memset(buf2, 0, sizeof buf2);
    r = mos_getdirforpath("mos/x.bin", buf2, &length, 0);
    printf("getdirforpath %d %d ", r, length);
    text("", buf2);
    text("getleafname", mos_getleafname("a/b/leaf.txt"));
    show("isdirectory mos", mos_isdirectory("mos"));
    show("isdirectory MOS.bin", mos_isdirectory("MOS.bin"));
    length = sizeof buf2;
    memset(buf2, 0, sizeof buf2);
    r = mos_getabsolutepath("mos/../bin", buf2, &length);
    printf("getabsolutepath %d ", r);
    text("", buf2);
}

static void machine(void)
{
    static vdp_time_t t;
    static UART u;
    uint8_t *kb;
    void *old;
    int r;

    r = mos_getrtc(buf2);
    printf("getrtc %d ", r);
    text("", buf2);
    memset(&t, 0xff, sizeof t);
    mos_unpackrtc(&t, 0);
    printf("unpackrtc %d %d %d %d %d\n", t.year, t.month, t.day, t.hour, t.dayOfWeek);
    show("clearvdpflags", mos_clearvdpflags(vdp_pflag_mode) & vdp_pflag_mode);
    kb = mos_getkbmap();
    show("getkbmap", kb != NULL);
    old = mos_setintvector(0x18, (void (*)(void)) 0x123456);
    show("setintvector", old != NULL);
    show("setintvector back", mos_setintvector(0x18, (void (*)(void)) old) == (void *) 0x123456);
    u.baudRate = 9600;
    u.dataBits = 8;
    u.stopBits = 1;
    u.parity = 0;
    u.flowcontrol = 0;
    u.eir = 0;
    show("uopen", mos_uopen(&u));
#ifndef AGONDEV
    show("uputc", mos_uputc('Z'));
#else
    mos_uputc('Z');
    show("uputc", 1);
#endif
    show("ugetc_nb", mos_ugetc_nb());
    mos_uclose();
#ifndef AGONDEV
    show("uputc closed", mos_uputc('Z'));
#else
    show("uputc closed", 0);
#endif
    mos_i2c_open(I2C_SPEED_57600);
    buf2[0] = 1;
    show("i2c_write", mos_i2c_write(0x50, 1, (unsigned char *) buf2));
    show("i2c_read", mos_i2c_read(0x50, 1, (unsigned char *) buf2));
    mos_i2c_close();
    show("sd_getunlockcode", sd_getunlockcode() != 0);
#ifndef AGONDEV
    show("sd_getunlockcode same", sd_getunlockcode() == sd_getunlockcode());
    show("sd_init", sd_init(sd_getunlockcode()));
    show("sd_init wrong code", sd_init(sd_getunlockcode() ^ 1));
#else
    show("sd_getunlockcode same", 1);
    show("sd_init", 1);
    show("sd_init wrong code", 2);
#endif
    show("oscli", mos_oscli("cd /", NULL, 0));
    show("oscli bad", mos_oscli("nosuchcommand", NULL, 0));
}

static void sysvars(void)
{
    uint8_t *v = mos_sysvars();

    show("sysvars", v != NULL);
    show("sys_vars", (uint8_t *) sys_vars == v);
    show("time", getsysvar_time() > 0);
    show("cols", getsysvar_scrCols());
    show("rows", getsysvar_scrRows());
    show("width", getsysvar_scrwidth());
    show("height", getsysvar_scrheight());
    show("colours", getsysvar_scrColours());
    show("mode", getsysvar_scrMode());
    show("keyascii", getsysvar_keyascii());
    show("keymods", getsysvar_keymods());
    show("vkeycode", getsysvar_vkeycode());
    show("vkeydown", getsysvar_vkeydown());
    show("keydelay", getsysvar_keydelay());
    show("keyrate", getsysvar_keyrate());
    show("keyled", getsysvar_keyled());
    show("mouseX", getsysvar_mouseX());
    show("mouseY", getsysvar_mouseY());
    show("mouseButtons", getsysvar_mouseButtons());
    show("mouseWheel", getsysvar_mouseWheel());
    show("mouseXDelta", getsysvar_mouseXDelta());
    show("mouseYDelta", getsysvar_mouseYDelta());
    show("scrchar", getsysvar_scrchar());
    show("scrpixel", (int) getsysvar_scrpixel());
    show("scrpixelIndex", getsysvar_scrpixelIndex());
    show("audioChannel", getsysvar_audioChannel());
    show("audioSuccess", getsysvar_audioSuccess());
    show("vdp_pflags", getsysvar_vdp_pflags() & 0x7f);
    show("rtc", getsysvar_rtc() == (volatile SYSVAR_RTCDATA *) (v + sysvar_rtc));
    show("struct cols", sys_vars->scrCols == getsysvar_scrCols());
    show("struct width", sys_vars->scrWidth == getsysvar_scrwidth());
    show("struct mode", sys_vars->scrMode == getsysvar_scrMode());
    show("struct pixel", sys_vars->scrpixel == getsysvar_scrpixel());
    show("struct pixelG", sys_vars->scrpixelG == v[sysvar_scrpixel + 2]);
    show("struct mouseYDelta", sys_vars->mouseYDelta == (uint16_t) getsysvar_mouseYDelta());
}

/* What acc has that libagon does not. libagon's build prints what each
 * should answer. */
static void beyond(void)
{
    static FIL f;
#ifndef AGONDEV
    void *sd_init_fn = mos_getfunction(MOS_FUNC_SD_INIT, 0);
    void *past = mos_getfunction(0x40, 0);
    char *(*leaf)(char *) = 0;

    uint8_t *(*getsysvars)(void) =
        (uint8_t *(*)(void)) mos_getfunction(MOS_FUNC_GETSYSVARS, 0);

    printf("getfunction %d %d %d %d\n", sd_init_fn != NULL, past == NULL,
           mos_getfunction(MOS_FUNC_GETSYSVARS, 1) == NULL,
           getsysvars && getsysvars() == mos_sysvars());
    ffs_fopen(&f, "t_i.txt", FA_WRITE | FA_CREATE_ALWAYS);
    printf("not implemented %d %d %d %d %d %d %d %d\n", ffs_fforward(&f, 0, 0, 0),
           ffs_expand(&f, 10, 0), ffs_chmod("t_i.txt", 0, 0), ffs_utime("t_i.txt", 0),
           ffs_chdrive("0:"), ffs_mkfs("", 0, 0, 0), ffs_fdisk(0, 0, 0), ffs_setcp(437));
    ffs_fclose(&f);
    ffs_unlink("t_i.txt");
    show("gp", getsysvar_gp() == ((uint8_t *) mos_sysvars())[0x37]);
    show("rtcEnable", getsysvar_rtcEnable() == sys_vars->rtcEnable);
    mos_putstring("putstring\r\n");
    (void) leaf;
#else
    (void) f;
    printf("getfunction 1 1 1 1\n");
    printf("not implemented 23 23 23 23 23 23 23 23\n");
    show("gp", 1);
    show("rtcEnable", 1);
    printf("putstring\r\n");
#endif
}

int main(void)
{
    files();
    ffs();
    strings();
    variables();
    paths();
    machine();
    sysvars();
    beyond();

    return 0;
}
