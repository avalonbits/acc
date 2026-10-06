/* acc's declaration with the bodies it reads in place, cut down by line
 * from its source to what still shows this: merged, most of its locals are
 * values and take no room, so the room of the bodies it takes is in reach
 * of (ix+d) and the machine-level backend makes it -- counting all the
 * locals the first pass declared, it was not, and declaration took none
 * of them (test/optacc.sh). */
typedef struct __acc_file {
char *buf;
}
FILE;
typedef unsigned char Type;
typedef unsigned int NameRef;
typedef struct {
unsigned char pos, width, bytes, is_signed;
}
BitField;
__attribute__((noreturn)) void acc_error_at(int line, const char *fmt, ...);
extern char *name_arena;
enum {
TK_EOF = 0,
TK_IDENT,
TK_KW_VOID,
TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
TK_SEMI, TK_COMMA, TK_ASSIGN,
TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
TK_INC, TK_DEC, TK_LBRACKET, TK_DOT, TK_ARROW,
TK_KW_ENUM, TK_KW_STRUCT, TK_KW_UNION, TK_KW_TYPEDEF,
TK_KW_CONST, TK_KW_VOLATILE, TK_KW_RESTRICT,
TK_COUNT
}
;
extern int tok;
extern NameRef tok_name;
extern int tok_line;
int lex_rparen_follows(void);
int accept_next(void);
const char *tok_spelling(int token);
enum {
}
;
typedef struct {
NameRef name;
Type type;
unsigned char quals;
}
Sym;
enum {
}
;
enum {
}
;
int sym_push(NameRef name, int kind, int val);
int sym_push_local(NameRef name, int kind, int val);
extern Sym *sym_table;
int gen_local(int size);
int gen_local_far(int size);
int vconst_top(int *val, Type *type);
Type vtype_at(int depth);
int gen_jump(void);
void gen_cond_end(int to_stub, int slot, int lock, Type middle,
int middle_ext, int middle_null);
int get24(const unsigned char *at)
{
}
static inline __attribute__((always_inline))
int name_global(NameRef ref)
{
return get24((const unsigned char *) name_arena + ref - 3) - 1;
}
void name_set_global(NameRef ref, int sym)
{
}
typedef struct {
int ndeps, nitems, strings_len;
}
Object;
enum {
}
;
static const unsigned char compound_op[TK_COUNT] = {
}
;
static void global_address(int sym)
{
acc_error_at(tok_line, "'[' needs an array or a pointer, and this is %s",
((Type) (vtype_at(1)) == ((Type) (4 | 0x10))) ? "a floating-point value"
: "an integer");
}
enum {
}
;
static int member_lookup(int x, NameRef name, int *offset, int *top)
{
acc_error_at(tok_line, "expected a member's name, found %s",
tok_spelling(tok));
}
enum {
POST_VALUE, POST_OBJECT, POST_CALL }
;
static int postfix_chain(int object)
{
}
static void literal_bytes(Type type, int x, int count, Type elem, int elem_x,
int line, int address, int *countp);
static inline __attribute__((always_inline)) int starts_decl(void);
static void primary(void)
{
}
enum {
}
;
static int transparent_op(int token)
{
if (tok == TK_LPAREN) {
}
int cond;
Type cond_type;
if (vconst_top(&cond, &cond_type) && !((Type) (cond_type) == ((Type) (4 | 0x10)))) {
}
}
typedef char qualifiers_are_adjacent[(TK_KW_VOLATILE == TK_KW_CONST + 1
&& TK_KW_RESTRICT == TK_KW_CONST + 2)
? 1 : -1];
static int decl_storage;
static void qualifiers(void)
{
while (((unsigned char) ((*(const unsigned char *) &tok) - TK_KW_TYPEDEF) < 9u)) {
if (tok == TK_KW_CONST) {
if (((unsigned char) ((*(const unsigned char *) &tok) - TK_KW_TYPEDEF) < 5u)) {
}
}
}
}
static void not_redeclared(NameRef name, int line)
{
}
static void record_members(int x, int is_union, int line)
{
while (tok != TK_RBRACE) {
if ((tok == (TK_SEMI) ? accept_next() : 0)) {
}
}
}
static unsigned char decl_start[TK_COUNT] = {
}
;
static int is_typedef_name(NameRef name)
{
while (((unsigned char) ((*(const unsigned char *) &tok) - TK_KW_CONST) < 3u)) {
}
}
Type declarator_stars(Type base)
{
while ((tok == (TK_STAR) ? accept_next() : 0)) {
}
}
void not_void(Type type, const char *what, int line)
{
acc_error_at(line, "this goto jumps past the declaration of an array "
"scope");
}
static int array_size(const char *what, int line, int *variable)
{
for (;
;
) {
}
}
int vla_size_slot(Type type, int x)
{
if (!starts_decl()) {
}
}
enum {
SIZEOF_OBJECT, SIZEOF_VALUE }
;
static int sizeof_postfix(int what)
{
}
static int sizeof_paren(void)
{
int what;
if (tok != TK_RPAREN) {
if (what == SIZEOF_OBJECT && (tok == TK_ASSIGN || compound_op[tok])) {
}
}
if (tok == TK_IDENT) {
}
}
enum {
DECL_PTR, DECL_ARRAY, DECL_FUNC }
;
typedef struct {
unsigned char op;
}
DeclOp;
static DeclOp decl_ops[32];
static int ndecl_ops;
static void decl_push(int op, int a, int b, int c)
{
if (tok == TK_LPAREN) {
if (tok == TK_STAR || tok == TK_LPAREN || tok == TK_LBRACKET
|| (tok == TK_IDENT && !is_typedef_name(tok_name))) {
}
}
for (;
;
) {
}
}
static NameRef decl_full(void)
{
if (tok == TK_LPAREN) {
int first = ndecl_ops, i;
for (i = first;
i < ndecl_ops;
i++) {
DeclOp *op = &decl_ops[i];
if (op->op == DECL_ARRAY && i == ndecl_ops - 1) {
acc_error_at(tok_line, "an array of functions is not a "
"to them is");
}
}
}
}
NameRef direct_declarator(Type t, int tx, Type *type, int *ext, int *count)
{
}
typedef void (*InitPut)(Type scalar, int offset, int value);
static int init_index(Type elem, int elem_x, int count, int offset,
InitPut put);
static void string_for(Type elem, int line)
{
acc_error_at(line, "a string initialises an array of char, and this "
"wchar_t");
}
static int init_list(Type elem, int elem_x, int count, int offset, InitPut put,
int line, int braced);
static int local_array_object(Type elem, int elem_x, int *countp, int line,
int braced)
{
int sym;
}
static void local_array(Type elem, int elem_x, NameRef name, int count,
int line)
{
local_array_object(elem, elem_x, &count, line, 0);
}
static int function_declarator(Type ret_type, int ret_ext, NameRef name,
int line);
static void global_variable(Type type, int ext, NameRef name, int count,
int line);
static int static_local;
static void block_extern(Type type, int ext, NameRef name, int count,
int line)
{
int g = name_global(name), sym;
const Sym *global;
((Sym *) ((char *) sym_table + (sym)))->quals = global->quals;
}
static void storage_declarators(int storage, Type base, int bx,
unsigned char bc)
{
for (;
;
) {
int line = tok_line, count, ext;
Type type, stars = declarator_stars(base);
NameRef name = direct_declarator(stars, bx, &type, &ext, &count);
if (tok == TK_LPAREN) {
not_redeclared(name, line);
block_extern(type, ext, name, count, line);
int hole = gen_jump();
}
}
}
static int storage_declaration(Type base, int bx, unsigned char bc)
{
int storage = decl_storage;
if (storage == TK_KW_TYPEDEF) {
storage_declarators(storage, base, bx, bc);
}
}
void declaration(void)
{
Type base;
int bx;
unsigned char bc;
if (decl_storage && storage_declaration(base, bx, bc))
for (;
;
) {
int line = tok_line, count, ext, off, sym, far = 0;
Type type, stars = declarator_stars(base);
NameRef name = direct_declarator(stars, bx, &type, &ext, &count);
if (count) {
local_array(type, ext, name, count, line);
}
off = far ? gen_local_far((((((type) & 0xe0) ? 3 : (int) ((type) & 0x07)) == 6) ? 8 : (((type) & 0xe0) ? 3 : (int) ((type) & 0x07))))
: gen_local((((((type) & 0xe0) ? 3 : (int) ((type) & 0x07)) == 6) ? 8 : (((type) & 0xe0) ? 3 : (int) ((type) & 0x07))));
}
while (tok != TK_RBRACE && tok != TK_EOF) {
}
}
typedef struct {
int sym, argoff;
}
StructParam;
static NameRef main_name;
static void body_end(int fn)
{
const Sym *s = ((Sym *) ((char *) sym_table + (fn)));
if (s->name == main_name && s->type == ((Type) 3)) {
}
if (tok == TK_KW_VOID && lex_rparen_follows()) {
for (;
;
) {
}
}
}
static int push_global(NameRef name, int kind, int at)
{
int sym;
sym = static_local ? sym_push_local(name, kind, at)
: sym_push(name, kind, at);
}
static int init_address;
static void walk_fn_add(int fn, int offset)
{
if (init_address) {
}
}
static void global_variable(Type type, int ext, NameRef name, int count,
int line)
{
}
typedef struct {
int *placed;
}
Taken;
static int item_of(const Object *o, int at)
{
int low = 0, high = o->nitems - 1;
while (low < high) {
}
}
static int item_end(const Object *o, int i)
{
}
static void take_items(Object *op, Taken *t, const char *name, const char *path)
{
Object o = *op;
int nqueue = 0, i, r, want_bss = !name, new_bss = 0, want_now = 0;
for (i = 0;
i < o.nitems;
i++) {
}
}
int main(int argc, char **argv)
{
int nobjs = 0, to_object = 0, to_archive = 0;
int i;
for (i = 1;
i < argc;
i++) {
if (argv[i][0] == '-' && argv[i][1] == 'o') {
}
}
if (to_archive) {
}
}