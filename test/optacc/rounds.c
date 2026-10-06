/* zap's assemble_line with the bodies it reads in place, cut down by
 * line from its source to what still shows this: merged, it has a byte
 * that must be in A held by a value spilled already, a dozen times over,
 * and the allocator finds one more each round only if it scans on past
 * the first -- stopping there, it ran out of rounds (test/optacc.sh). */
typedef unsigned char uint8_t;
typedef unsigned int uint24_t;
typedef unsigned long uint32_t;
typedef struct __acc_file {
char *buf;
}
FILE;
typedef struct _buf_reader {
char* buf_;
uint24_t bsz_;
}
buf_reader;
buf_reader* br_open(buf_reader* br, const char* fname, int bsz);
_Bool br_fill_lines(buf_reader* br, _Bool* too_long);
typedef enum _isa_transform {
TR_NONE = 0,
TR_REL
}
isa_transform;
typedef struct _isa_row {
uint8_t condA;
uint8_t transformA;
uint8_t cpu;
}
isa_row;
typedef unsigned long clock_t;
struct tm {
int tm_min;
}
;
static inline unsigned elapsed_cs(clock_t begin, clock_t end) {
}
typedef struct {
uint8_t month;
}
FATFS;
typedef struct sym sym;
typedef struct _dop {
uint8_t r0, r1, r2;
uint8_t noreg;
uint8_t mode;
const sym* fwd;
const sym* fwd2;
_Bool fwd2_neg;
int imm;
}
dop;
typedef struct _macro macro;
typedef struct {
int lstat;
int row0;
int outoff;
int nbytes;
}
lstfix;
struct _macro {
uint8_t nparam;
}
;
typedef enum {
}
zap_err;
typedef struct symblock symblock;
struct _namblock {
char buf[4096];
}
;
struct sym {
uint8_t len;
}
;
typedef struct {
int off;
}
fillpatch;
typedef struct {
int off;
uint8_t kind;
}
latepatch;
typedef struct {
int cur;
}
laterun;
typedef struct {
sym* head;
}
locslot;
typedef struct _zap_state {
buf_reader rd;
uint8_t* o;
_Bool adl;
int line;
lstfix* lstfix;
int lstfix_used;
_Bool errhave;
const char* errmacro;
locslot locs[64];
int defer_used;
latepatch* late;
int late_runs;
}
zap_state;
_Static_assert(__builtin_offsetof(zap_state, locs) > __builtin_offsetof(zap_state, line),
"the local table must come after the output cursor");
struct symblock {
symblock* next;
}
;
typedef struct rowinfo rowinfo;
struct rowinfo {
uint8_t a0, a1, a2;
uint8_t b0, b1, b2;
uint8_t aempty, bempty;
const isa_row* row;
}
;
typedef struct {
uint8_t modes;
uint8_t count;
const rowinfo* rows;
}
grpinfo;
typedef struct insninfo insninfo;
struct insninfo {
const insninfo* next;
const char* name;
const grpinfo* groups;
const rowinfo* rows;
uint8_t count;
}
;
typedef struct {
uint8_t prefix2;
}
emitted;
_Bool out_peek(int off, uint8_t* dst, int n);
extern uint8_t cpu_mask;
extern uint8_t list_fh;
extern const uint8_t shl4[16];
extern _Bool want_list;
extern uint8_t cclass[256];
_Bool fix_add(const sym* target, const sym* sub, int addend, uint8_t width, int off);
extern zap_state state;
static inline int out_at(const uint8_t* p) {
}
static inline _Bool reg_of_text(const char* s, int n, dop* op, _Bool* is_cc,
uint8_t* cc_index) {
if (n == 1) {
const char b = (char) (s[1] | 0x20);
if (b == 'x') {
}
}
}
static inline _Bool is_space_ch(char c) {
}
static inline _Bool name_ch(char c) {
}
static inline _Bool num_ch(char c) {
}
static inline _Bool digit_ch(char c) {
}
static inline _Bool hex_digits(const char* d, int n, int* out) {
union {
uint8_t b[sizeof(int)];
}
u;
}
__attribute__((always_inline)) static inline _Bool parse_operand(dop* op, const char** pp, const char* e) {
const char* p = *pp;
uint8_t cl = cclass[(uint8_t) *p];
if ((cl & 0x20) != 0) {
const char* s = p;
while (p < e && name_ch(*p)) {
}
const int n = (int) (p - s);
_Bool is_cc = 0;
uint8_t cc_index = 0;
if (reg_of_text(s, n, op, &is_cc, &cc_index)) {
if (is_cc) {
while (p < e && num_ch(*p)) {
}
}
}
}
{
const char* s = p;
const int n = (int) (p - s);
int nn = n;
if (n > 0 && (*s == '-' || *s == '+')) {
}
if (nn == 0) {
unsigned k = 1;
for (;
k < (unsigned) nn;
k++) {
}
}
while (p < e && is_space_ch(*p)) {
}
}
}
static inline const insninfo* bucket_at(char first, unsigned n) {
}
static inline _Bool same_ci(const char* name, const char* s, int n) {
for (unsigned i = 1;
i < (unsigned) n;
i++) {
if (name[i] != (s[i] | 0x20)) {
}
}
}
static inline const insninfo* mnemonic_of(const char* s, int n) {
for (const insninfo* ins = bucket_at(s[0], (unsigned) n);
ins != ((void *) 0);
ins = ins->next) {
if (same_ci(ins->name, s, n)) {
}
}
}
__attribute__((always_inline)) static inline const isa_row* match_row_cc(
const insninfo* insn, const dop* a, const dop* b, uint8_t want) {
const uint8_t a0 = a->r0, a1 = a->r1, a2 = a->r2;
const uint8_t bnone = b->noreg;
const rowinfo* ri = insn->rows;
for (uint8_t n = insn->count;
n != 0;
n--, ri++) {
if ((uint8_t) ((ri->a0 & a0) | (ri->a1 & a1) | (ri->a2 & a2)
| (ri->bempty & bnone)) != 0) {
}
}
}
__attribute__((always_inline)) static inline const isa_row* match_row(const insninfo* insn,
const dop* a,
const dop* b) {
const uint8_t want = (uint8_t) (shl4[a->mode & 15] | (b->mode & 15));
const grpinfo* g = insn->groups;
while (g->modes != want) {
}
const uint8_t a0 = a->r0, a1 = a->r1, a2 = a->r2;
const uint8_t b0 = b->r0, b1 = b->r1, b2 = b->r2;
const uint8_t anone = a->noreg;
const uint8_t bnone = b->noreg;
const rowinfo* ri = g->rows;
for (uint8_t k = g->count;
k != 0;
k--, ri++) {
if ((uint8_t) ((ri->a0 & a0) | (ri->a1 & a1) | (ri->a2 & a2)
| (ri->aempty & anone)) != 0
&& (uint8_t) ((ri->b0 & b0) | (ri->b1 & b1) | (ri->b2 & b2)
| (ri->bempty & bnone)) != 0) {
const isa_row* row = ri->row;
if ((row->cpu & cpu_mask) == 0) {
}
}
}
}
__attribute__((always_inline)) static inline uint8_t transform(emitted* out, dop* op, uint8_t type) {
switch (type) {
if (((op->r1 & 0x80) | (op->r2 & 0x02)) != 0) {
}
}
}
__attribute__((always_inline)) static inline _Bool emit_row(const isa_row* row, dop* a, dop* b, uint8_t suffix) {
uint8_t* o = state.o;
if (suffix != 0) {
}
if (row->transformA != TR_NONE) {
const dop* rel = (row->transformA == TR_REL) ? a : b;
if (rel->fwd != ((void *) 0)) {
if (!fix_add(rel->fwd, rel->fwd2, rel->imm,
rel->fwd2_neg ? 0x80 : 0,
out_at(o))) {
}
if (a->fwd != ((void *) 0)
&& !fix_add(a->fwd, a->fwd2, a->imm,
(uint8_t) (((row->condA & 0x10) ? 1
: ((suffix != 0 ? (suffix & (0x10 | 0x20)) == 0 : state.adl) ? 3 : 2))
| (a->fwd2_neg ? 0x80 : 0)),
out_at(o))) {
}
}
}
}
__attribute__((noinline)) _Bool assemble_line(const char* p, const char* e, const char** stop) {
const char* s = p;
int n = (int) (p - s);
const insninfo* insn = mnemonic_of(s, n);
if (insn == ((void *) 0)) {
}
dop a;
dop b;
if (!parse_operand(&a, &p, e)) {
}
const isa_row* row = match_row(insn, &a, &b);
return emit_row(row, &a, &b, 0);
}
static _Bool resolve_deferred(void) {
for (int i = 0;
i < state.defer_used;
i++) {
}
}
static void apply_late(uint8_t* buf, int lo, int hi, laterun* run, int to) {
int* cur = &run->cur;
for (int i = *cur;
i < to;
i++) {
const latepatch* lp = &state.late[i];
const int n = lp->kind == 5 ? 1 : lp->kind;
for (int k = 0;
k < n;
k++) {
const int at = lp->off + k;
if (at >= lo && at < hi) {
}
}
}
}
static void apply_range(uint8_t* buf, int lo, int hi, int* cur) {
for (int r = 0;
r < state.late_runs;
r++) {
}
}
__attribute__((noinline)) static _Bool run(const char* path) {
if (br_open(&state.rd, path, 16) == ((void *) 0)) {
}
}
static _Bool opt_hex(const char* attached, const char* next, int* used,
int* out) {
const char* p = attached;
if (*p == 0) {
}
int n = 0;
for (;
*p != 0;
p++, n++) {
if (*p >= '0' && *p <= '9') {
}
}
}
static void err_reopen(const char* path, int line) {
buf_reader r;
if (br_open(&r, path, 1) == ((void *) 0)) {
}
_Bool too_long = 0;
while (br_fill_lines(&r, &too_long)) {
const char* p = r.buf_;
const char* const e = p + r.bsz_;
while (p < e) {
}
}
}
static void list_out(const char* buf, int n) {
if (list_fh != 0) {
}
}
static void list_hex(char* buf, int* w, uint32_t v, int digits) {
for (int shift = (digits - 1) * 4;
shift >= 0;
shift -= 4) {
}
}
void list_line(int pc, int from, int to, int line,
int depth, const char* text, const char* tend) {
int n = to - from;
int row = 0;
do {
if (row == 0) {
for (int i = 0;
i < 7;
i++) {
}
}
}
while (row * 4 < n);
for (int i = 0;
i < state.lstfix_used;
i++) {
const lstfix* r = &state.lstfix[i];
for (int row = 0;
row * 4 < r->nbytes;
row++) {
const int at = (row == 0)
? r->lstat
: r->lstat + r->row0 + (row - 1) * 20;
int have = r->nbytes - row * 4;
uint8_t bytes[4];
if (have > 0 && !out_peek(r->outoff + row * 4, bytes, have)) {
}
}
}
}
void list_invocation(const macro* m, int base, int line, int depth,
const char* e) {
}
void list_args(const macro* m, int base, int depth) {
char buf[128 + 48];
const int lim = (int) sizeof(buf) - 3;
int w = 0;
for (const char* q = " Args: ";
*q != 0;
q++) {
}
if (m->nparam == 0) {
for (const char* q = "none";
*q != 0 && w < lim;
q++) {
}
}
}
static _Bool sidecar_name(const char* src, const char* ext, char* out, int cap) {
int n = 0;
while (src[n] != 0) {
}
}
static int sym_cmp(const void* a, const void* b) {
const sym* const x = *(const sym* const*) a;
const sym* const y = *(const sym* const*) b;
const int n = x->len < y->len ? x->len : y->len;
for (int i = 0;
i < n;
i++) {
}
}
static void report(const char* in) {
if (!state.errhave && state.errmacro == ((void *) 0)) {
}
}
int main(int argc, char* argv[]) {
if (want_list) {
}
}