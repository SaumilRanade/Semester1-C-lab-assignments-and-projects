/*
 * csyntax.c  -  C Syntax Checker & Code Analyzer  (UTF-8 + ANSI menu edition)
 *
 * Build:  gcc -std=c11 -Wall -Wextra -O2 -o csyntax csyntax.c
 * Run  :  ./csyntax            (menu)      or      ./csyntax file.c
 *
 * What it does
 *   1. Tokenises a text file (comments, strings, chars, numbers, #directives).
 *   2. Checks "medium depth" syntax: balanced () [] {}, literals, preprocessor
 *      lines, keyword/statement structure (if/for/while/do/switch/case/else,
 *      break/continue/goto/return), missing semicolons, bad operator use.
 *   3. Offers a report: comments, logic statements, loops, nested loops,
 *      variables, structs, arrays, pointers, functions, directives, ...
 *
 * NOTE: this is a heuristic checker, not a full C compiler front-end.
 *       Macro-heavy code and K&R-style definitions can produce false alarms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <locale.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* ════════════════════════════  ANSI + BOX UI  ════════════════════════════ */

#define RST  "\033[0m"
#define BOLD "\033[1m"
#define DIM  "\033[2m"
#define RED  "\033[91m"
#define GRN  "\033[92m"
#define YEL  "\033[93m"
#define BLU  "\033[94m"
#define MAG  "\033[95m"
#define CYN  "\033[96m"

#define BOX_W 62                       /* inner text width of every box */

static void init_terminal(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(65001);         /* CP_UTF8 */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (GetConsoleMode(h, &m)) SetConsoleMode(h, m | 0x0004); /* VT processing */
#else
    setlocale(LC_ALL, "");
#endif
}

static void rep(const char *s, int n) { while (n-- > 0) fputs(s, stdout); }

/* visible length: skips ANSI escapes, counts UTF-8 code points */
static int vis_len(const char *s)
{
    int n = 0;
    while (*s) {
        if ((unsigned char)*s == 0x1B) {
            while (*s && *s != 'm') s++;
            if (*s) s++;
            continue;
        }
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
        s++;
    }
    return n;
}

/* copy at most maxcols code points; adds "…" when clipped */
static void clip(char *dst, size_t dsz, const char *src, int maxcols)
{
    size_t k = 0;
    int cols = 0;
    const unsigned char *p = (const unsigned char *)src;
    while (*p && *p != '\n' && *p != '\r') {
        int l = (*p < 0x80) ? 1 : (*p >= 0xF0) ? 4 : (*p >= 0xE0) ? 3 : (*p >= 0xC0) ? 2 : 1;
        if (cols >= maxcols) {
            if (k + 4 < dsz) { memcpy(dst + k, "…", 3); k += 3; }
            break;
        }
        if (k + (size_t)l + 4 >= dsz) break;
        if (*p == '\t') { dst[k++] = ' '; p++; cols++; continue; }
        for (int q = 0; q < l && *p; q++) dst[k++] = (char)*p++;
        cols++;
    }
    dst[k] = '\0';
}

static void box_top(void)  { fputs(CYN "╔", stdout); rep("═", BOX_W + 2); fputs("╗" RST "\n", stdout); }
static void box_mid(void)  { fputs(CYN "╠", stdout); rep("═", BOX_W + 2); fputs("╣" RST "\n", stdout); }
static void box_thin(void) { fputs(CYN "╟", stdout); rep("─", BOX_W + 2); fputs("╢" RST "\n", stdout); }
static void box_bot(void)  { fputs(CYN "╚", stdout); rep("═", BOX_W + 2); fputs("╝" RST "\n", stdout); }

static void box_text(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    int pad = BOX_W - vis_len(buf);
    if (pad < 0) pad = 0;
    printf(CYN "║" RST " %s", buf);
    rep(" ", pad);
    printf(" " CYN "║" RST "\n");
}

static void box_center(const char *color, const char *text)
{
    int v = vis_len(text), l = (BOX_W - v) / 2, r = BOX_W - v - l;
    if (l < 0) l = 0;
    if (r < 0) r = 0;
    printf(CYN "║" RST " ");
    rep(" ", l);
    printf("%s%s" RST, color, text);
    rep(" ", r);
    printf(" " CYN "║" RST "\n");
}

static void box_section(const char *title)
{
    box_mid();
    box_text(BOLD YEL "◆ %s" RST, title);
    box_thin();
}

/* "  label ········· value" */
static void box_kv(const char *label, long v)
{
    char dots[256] = "";
    int n = BOX_W - 11 - vis_len(label);
    if (n < 1) n = 1;
    if (n > 80) n = 80;
    for (int i = 0; i < n; i++) strcat(dots, "·");
    box_text("  %s " DIM "%s" RST " " GRN "%7ld" RST, label, dots, v);
}

static void box_kvs(const char *label, const char *val, const char *color)
{
    char dots[256] = "";
    int n = BOX_W - 11 - vis_len(label);
    if (n < 1) n = 1;
    if (n > 80) n = 80;
    for (int i = 0; i < n; i++) strcat(dots, "·");
    box_text("  %s " DIM "%s" RST " %s%7s" RST, label, dots, color, val);
}

/* ═══════════════════════════════  DATA  ══════════════════════════════════ */

typedef enum { T_IDENT, T_KEYWORD, T_NUMBER, T_STRING, T_CHAR, T_PUNCT, T_EOF } TokType;

typedef struct {
    TokType type;
    char text[72];
    int line, col;
    int ppBefore;                      /* a #directive sat between prev token and this one */
} Token;

typedef struct { int line, col; char msg[220]; } Diag;

typedef struct {
    int totalLines, codeLines, commentOnlyLines, blankLines;
    int lineComments, blockComments;
    int includes, defines, otherDirectives;
    int ifs, elses, elseIfs, switches, cases, defaults, ternaries;
    int andOps, orOps, notOps;
    int gotos, labels, breaks, continues, returns;
    int fors, whiles, dos, nestedLoops, maxLoopNest;
    int functions, prototypes;
    int globalVars, localVars, pointers, arrays, structVars, members;
    int structs, unions, enums, typedefs;
    int maxBlockDepth;
    int strings, chars;
    long tokens;
    int hasMain;
} Stats;

#define MAX_DIAG 500

static char *g_src = NULL;
static size_t g_len = 0;
static char g_path[512] = "";
static Token *g_tok = NULL;
static int g_ntok = 0, g_captok = 0;
static Diag *g_diag = NULL;
static int g_ndiag = 0, g_capdiag = 0, g_dropped = 0;
static Stats g_st;
static unsigned char *g_lf = NULL;     /* per-line flags: 1 = code, 2 = comment */
static int g_lfN = 0;
static char g_td[256][64];
static int g_ntd = 0;
static int g_loaded = 0, g_utf8ok = 1, g_bom = 0;

static const char *KW[] = {
    "auto","break","case","char","const","continue","default","do","double","else",
    "enum","extern","float","for","goto","if","inline","int","long","register",
    "restrict","return","short","signed","sizeof","static","struct","switch",
    "typedef","union","unsigned","void","volatile","while","_Bool","_Complex",
    "_Imaginary","_Alignas","_Alignof","_Atomic","_Generic","_Noreturn",
    "_Static_assert","_Thread_local", NULL
};
static const char *STD_TD[] = {
    "size_t","ssize_t","ptrdiff_t","FILE","bool","wchar_t","int8_t","int16_t","int32_t",
    "int64_t","uint8_t","uint16_t","uint32_t","uint64_t","intptr_t","uintptr_t",
    "time_t","va_list","off_t","clock_t", NULL
};

static int in_list(const char **L, const char *s)
{
    for (int i = 0; L[i]; i++) if (!strcmp(L[i], s)) return 1;
    return 0;
}
static int is_keyword(const char *s) { return in_list(KW, s); }
static int is_type_kw(const char *s)
{
    static const char *L[] = {"char","short","int","long","float","double","void","signed",
                              "unsigned","_Bool","_Complex","_Imaginary", NULL};
    return in_list(L, s);
}
static int is_qual_kw(const char *s)
{
    static const char *L[] = {"const","volatile","restrict","static","extern","register","auto",
                              "inline","_Thread_local","_Noreturn","_Atomic", NULL};
    return in_list(L, s);
}
static int is_ctl_kw(const char *s)
{
    static const char *L[] = {"if","else","for","while","do","switch","case","default",
                              "break","continue","return","goto", NULL};
    return in_list(L, s);
}
static int starts_stmt_kw(const char *s)
{
    static const char *L[] = {"return","if","for","while","do","switch","break","continue","goto",
                              "else","case","default","int","char","short","long","float","double",
                              "void","signed","unsigned","struct","union","enum","static","extern",
                              "typedef","_Bool", NULL};
    return in_list(L, s);
}
static int is_typedef_name(const char *s)
{
    for (int i = 0; i < g_ntd; i++) if (!strcmp(g_td[i], s)) return 1;
    return in_list(STD_TD, s);
}
static void add_typedef(const char *s)
{
    if (g_ntd < 256 && !is_typedef_name(s)) { snprintf(g_td[g_ntd], 64, "%s", s); g_ntd++; }
}

/* ═════════════════════════  DIAGNOSTICS / TOKENS  ════════════════════════ */

static void add_diag(int line, int col, const char *fmt, ...)
{
    if (g_ndiag >= MAX_DIAG) { g_dropped++; return; }
    if (g_ndiag == g_capdiag) {
        g_capdiag = g_capdiag ? g_capdiag * 2 : 64;
        g_diag = realloc(g_diag, (size_t)g_capdiag * sizeof(Diag));
        if (!g_diag) { fputs("out of memory\n", stderr); exit(1); }
    }
    Diag *d = &g_diag[g_ndiag];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(d->msg, sizeof d->msg, fmt, ap);
    va_end(ap);
    d->line = line;
    d->col = col;
    if (g_ndiag > 0 && g_diag[g_ndiag - 1].line == line && !strcmp(g_diag[g_ndiag - 1].msg, d->msg))
        return;
    g_ndiag++;
}

static void add_tok(TokType ty, const char *s, size_t len, int line, int col, int pp)
{
    if (g_ntok == g_captok) {
        g_captok = g_captok ? g_captok * 2 : 1024;
        g_tok = realloc(g_tok, (size_t)g_captok * sizeof(Token));
        if (!g_tok) { fputs("out of memory\n", stderr); exit(1); }
    }
    Token *t = &g_tok[g_ntok++];
    if (len > sizeof t->text - 1) len = sizeof t->text - 1;
    memcpy(t->text, s, len);
    t->text[len] = '\0';
    t->type = ty;
    t->line = line;
    t->col = col;
    t->ppBefore = pp;
    g_st.tokens++;
}

static void mark(int line, int flag)
{
    if (g_lf && line >= 1 && line <= g_lfN) g_lf[line] |= (unsigned char)flag;
}

static void trim(char *s)
{
    size_t n = strlen(s), a = 0;
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    while (s[a] && isspace((unsigned char)s[a])) a++;
    if (a) memmove(s, s + a, strlen(s + a) + 1);
}

static int utf8_valid(const unsigned char *s, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        int l;
        if (c < 0x80) l = 1;
        else if (c >= 0xC2 && c <= 0xDF) l = 2;
        else if (c >= 0xE0 && c <= 0xEF) l = 3;
        else if (c >= 0xF0 && c <= 0xF4) l = 4;
        else return 0;
        if (i + (size_t)l > n) return 0;
        for (int k = 1; k < l; k++) if ((s[i + (size_t)k] & 0xC0) != 0x80) return 0;
        i += (size_t)l;
    }
    return 1;
}

/* ══════════════════════════════  LEXER  ══════════════════════════════════ */

static int is_idstart(unsigned char c) { return isalpha(c) || c == '_' || c >= 0x80; }
static int is_idchar(unsigned char c)  { return isalnum(c) || c == '_' || c >= 0x80; }

static int number_ok(const char *t)
{
    size_t n = strlen(t), i = 0;
    if (n >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        int digits = 0;
        for (i = 2; i < n; i++) {
            char c = t[i];
            if (isxdigit((unsigned char)c)) { digits++; continue; }
            if (c == '.') continue;
            if (c == 'p' || c == 'P') {
                i++;
                if (i < n && (t[i] == '+' || t[i] == '-')) i++;
                while (i < n && isdigit((unsigned char)t[i])) i++;
                break;
            }
            break;
        }
        for (; i < n; i++) if (!strchr("uUlLfF", t[i])) return 0;
        return digits > 0;
    }
    if (n >= 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) {
        for (i = 2; i < n; i++) if (t[i] != '0' && t[i] != '1') break;
        for (; i < n; i++) if (!strchr("uUlL", t[i])) return 0;
        return n > 2;
    }
    for (i = 0; i < n; i++) {
        char c = t[i];
        if (isdigit((unsigned char)c) || c == '.') continue;
        if (c == 'e' || c == 'E') { if (i + 1 < n && (t[i + 1] == '+' || t[i + 1] == '-')) i++; continue; }
        break;
    }
    for (; i < n; i++) if (!strchr("uUlLfF", t[i])) return 0;
    return 1;
}

/* scans a quoted literal starting at s[*pi] (the quote). returns 1 if terminated */
static int scan_quoted(const char *s, size_t n, size_t *pi, int *line, size_t *ls, char q)
{
    size_t i = *pi + 1;
    int ok = 0;
    while (i < n) {
        char c = s[i];
        if (c == '\\' && i + 1 < n) {
            if (s[i + 1] == '\n') { (*line)++; *ls = i + 2; i += 2; continue; }
            if (s[i + 1] == '\r' && i + 2 < n && s[i + 2] == '\n') { (*line)++; *ls = i + 3; i += 3; continue; }
            i += 2;
            continue;
        }
        if (c == q) { i++; ok = 1; break; }
        if (c == '\n') break;
        i++;
    }
    *pi = i;
    return ok;
}

static const char *DIRS[] = {
    "include","define","undef","if","ifdef","ifndef","else","elif","elifdef","elifndef",
    "endif","error","warning","pragma","line","ident","include_next","embed","assert",
    "unassert","sccs", NULL
};
static const char *P3[] = {"<<=", ">>=", "...", NULL};
static const char *P2[] = {"->","++","--","<<",">>","<=",">=","==","!=","&&","||","+=","-=",
                           "*=","/=","%=","&=","|=","^=","##", NULL};

static void lex(void)
{
    const char *s = g_src;
    size_t n = g_len, i = 0, ls = 0;
    int line = 1, atLineStart = 1, ppFlag = 0;
    int skipping = 0, skipNest = 0;
    int ncond = 0, condLine[64];

    if (n >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
        i = 3; ls = 3; g_bom = 1;
    }

    while (i < n) {
        char c = s[i];

        if (c == '\n') { line++; i++; ls = i; atLineStart = 1; continue; }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { i++; continue; }
        if (c == '\\' && i + 1 < n && (s[i + 1] == '\n' || (s[i + 1] == '\r' && i + 2 < n && s[i + 2] == '\n'))) {
            i += (s[i + 1] == '\r') ? 3 : 2;
            line++; ls = i;
            continue;
        }

        /* lines disabled by "#if 0" are skipped wholesale */
        if (skipping && atLineStart && c != '#') {
            mark(line, 1);
            while (i < n && s[i] != '\n') i++;
            continue;
        }

        int col = (int)(i - ls) + 1;

        /* ---- comments ---- */
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            g_st.lineComments++;
            mark(line, 2);
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            int sl = line;
            g_st.blockComments++;
            mark(line, 2);
            i += 2;
            int closed = 0;
            while (i < n) {
                if (s[i] == '*' && i + 1 < n && s[i + 1] == '/') { i += 2; closed = 1; break; }
                if (s[i] == '\n') { line++; ls = i + 1; mark(line, 2); }
                i++;
            }
            if (!closed) add_diag(sl, col, "Unterminated block comment (missing '*/')");
            continue;
        }

        /* ---- preprocessor directive ---- */
        if (c == '#' && atLineStart) {
            int dl = line, dc = col;
            mark(line, 1);
            i++;
            while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
            char name[32];
            size_t nl = 0;
            while (i < n && isalpha((unsigned char)s[i]) && nl < sizeof name - 1) name[nl++] = s[i++];
            name[nl] = '\0';

            char rest[512];
            size_t rl = 0;
            while (i < n && s[i] != '\n') {
                if (s[i] == '\\' && ((i + 1 < n && s[i + 1] == '\n') ||
                                     (i + 2 < n && s[i + 1] == '\r' && s[i + 2] == '\n'))) {
                    i += (s[i + 1] == '\r') ? 3 : 2;
                    line++; ls = i; mark(line, 1);
                    if (rl < sizeof rest - 1) rest[rl++] = ' ';
                    continue;
                }
                if (s[i] == '"') {                       /* copy string verbatim */
                    if (rl < sizeof rest - 1) rest[rl++] = s[i];
                    i++;
                    while (i < n && s[i] != '\n' && s[i] != '"') {
                        if (rl < sizeof rest - 1) rest[rl++] = s[i];
                        if (s[i] == '\\' && i + 1 < n && s[i + 1] != '\n') { i++; if (rl < sizeof rest - 1) rest[rl++] = s[i]; }
                        i++;
                    }
                    if (i < n && s[i] == '"') { if (rl < sizeof rest - 1) rest[rl++] = '"'; i++; }
                    continue;
                }
                if (s[i] == '/' && i + 1 < n && s[i + 1] == '/') {
                    g_st.lineComments++; mark(line, 2);
                    while (i < n && s[i] != '\n') i++;
                    break;
                }
                if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
                    int sl = line, sc = (int)(i - ls) + 1, closed = 0;
                    g_st.blockComments++; mark(line, 2);
                    i += 2;
                    while (i < n) {
                        if (s[i] == '*' && i + 1 < n && s[i + 1] == '/') { i += 2; closed = 1; break; }
                        if (s[i] == '\n') { line++; ls = i + 1; mark(line, 2); }
                        i++;
                    }
                    if (!closed) add_diag(sl, sc, "Unterminated block comment (missing '*/')");
                    if (rl < sizeof rest - 1) rest[rl++] = ' ';
                    continue;
                }
                if (rl < sizeof rest - 1) rest[rl++] = s[i];
                i++;
            }
            rest[rl] = '\0';
            trim(rest);
            ppFlag = 1;

            int isIf = !strcmp(name, "if") || !strcmp(name, "ifdef") || !strcmp(name, "ifndef");
            int isElse = !strcmp(name, "else") || !strcmp(name, "elif") ||
                         !strcmp(name, "elifdef") || !strcmp(name, "elifndef");

            if (skipping) {
                if (isIf) skipNest++;
                else if (!strcmp(name, "endif")) {
                    if (skipNest > 0) skipNest--;
                    else { skipping = 0; if (ncond > 0) ncond--; }
                } else if (isElse && skipNest == 0) skipping = 0;
                continue;
            }

            if (nl == 0) {
                if (rl > 0) add_diag(dl, dc, "Invalid preprocessing directive after '#'");
            } else if (!in_list(DIRS, name)) {
                add_diag(dl, dc, "Unknown preprocessor directive '#%s'", name);
            } else if (!strcmp(name, "include") || !strcmp(name, "include_next")) {
                g_st.includes++;
                int ok = (rest[0] == '<' && strchr(rest, '>')) ||
                         (rest[0] == '"' && strrchr(rest, '"') > rest) ||
                         (is_idstart((unsigned char)rest[0]) && !strchr(rest, '.'));
                if (!ok) add_diag(dl, dc, "Malformed #include (expected <file> or \"file\")");
            } else if (!strcmp(name, "define")) {
                g_st.defines++;
                if (!is_idstart((unsigned char)rest[0])) add_diag(dl, dc, "#define needs a macro name");
            } else {
                g_st.otherDirectives++;
                if (!strcmp(name, "undef") && !is_idstart((unsigned char)rest[0]))
                    add_diag(dl, dc, "#undef needs a macro name");
                if ((!strcmp(name, "ifdef") || !strcmp(name, "ifndef")) && !is_idstart((unsigned char)rest[0]))
                    add_diag(dl, dc, "#%s needs a macro name", name);
                if ((!strcmp(name, "if") || !strcmp(name, "elif")) && rest[0] == '\0')
                    add_diag(dl, dc, "#%s needs a condition", name);
                if (isIf) {
                    if (ncond < 64) condLine[ncond] = dl;
                    ncond++;
                    if (!strcmp(name, "if") && !strcmp(rest, "0")) { skipping = 1; skipNest = 0; }
                } else if (isElse) {
                    if (ncond == 0) add_diag(dl, dc, "#%s without a matching #if", name);
                } else if (!strcmp(name, "endif")) {
                    if (ncond == 0) add_diag(dl, dc, "#endif without a matching #if");
                    else ncond--;
                }
            }
            continue;
        }

        /* ---- string / char literal (with optional L, u, U, u8 prefix) ---- */
        {
            size_t qpos = i;
            int isLit = 0;
            if (c == '"' || c == '\'') isLit = 1;
            else if (c == 'L' || c == 'u' || c == 'U') {
                size_t pl = (c == 'u' && i + 1 < n && s[i + 1] == '8') ? 2 : 1;
                if (i + pl < n && (s[i + pl] == '"' || s[i + pl] == '\'')) { isLit = 1; qpos = i + pl; }
            }
            if (isLit) {
                char q = s[qpos];
                int sl = line;
                size_t ni = qpos;
                int ok = scan_quoted(s, n, &ni, &line, &ls, q);
                if (!ok) add_diag(sl, col, q == '"' ? "Unterminated string literal" : "Unterminated character constant");
                else if (q == '\'' && ni - qpos == 2) add_diag(sl, col, "Empty character constant ''");
                add_tok(q == '"' ? T_STRING : T_CHAR, q == '"' ? "\"\"" : "''", 2, sl, col, ppFlag);
                if (q == '"') g_st.strings++; else g_st.chars++;
                mark(sl, 1);
                ppFlag = 0; atLineStart = 0;
                i = ni;
                continue;
            }
        }

        /* ---- identifier / keyword ---- */
        if (is_idstart((unsigned char)c)) {
            size_t st = i;
            while (i < n && is_idchar((unsigned char)s[i])) i++;
            char buf[72];
            size_t len = i - st;
            if (len > sizeof buf - 1) len = sizeof buf - 1;
            memcpy(buf, s + st, len);
            buf[len] = '\0';
            add_tok(is_keyword(buf) ? T_KEYWORD : T_IDENT, buf, len, line, col, ppFlag);
            mark(line, 1);
            ppFlag = 0; atLineStart = 0;
            continue;
        }

        /* ---- number ---- */
        if (isdigit((unsigned char)c) || (c == '.' && i + 1 < n && isdigit((unsigned char)s[i + 1]))) {
            size_t st = i;
            int hex = (c == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X'));
            i++;
            while (i < n) {
                char d = s[i];
                if (is_idchar((unsigned char)d) || d == '.') { i++; continue; }
                if ((d == '+' || d == '-') &&
                    (s[i - 1] == 'p' || s[i - 1] == 'P' || (!hex && (s[i - 1] == 'e' || s[i - 1] == 'E')))) { i++; continue; }
                break;
            }
            char buf[72];
            size_t len = i - st;
            if (len > sizeof buf - 1) len = sizeof buf - 1;
            memcpy(buf, s + st, len);
            buf[len] = '\0';
            if (!number_ok(buf)) add_diag(line, col, "Malformed numeric literal '%s'", buf);
            add_tok(T_NUMBER, buf, len, line, col, ppFlag);
            mark(line, 1);
            ppFlag = 0; atLineStart = 0;
            continue;
        }

        /* ---- punctuator ---- */
        {
            size_t plen = 0;
            for (int k = 0; P3[k]; k++) if (i + 3 <= n && !strncmp(s + i, P3[k], 3)) { plen = 3; break; }
            if (!plen) for (int k = 0; P2[k]; k++) if (i + 2 <= n && !strncmp(s + i, P2[k], 2)) { plen = 2; break; }
            if (!plen && strchr("{}[]();,.:?~!%^&*-+=|<>/#", c)) plen = 1;
            if (plen) {
                add_tok(T_PUNCT, s + i, plen, line, col, ppFlag);
                mark(line, 1);
                i += plen;
                ppFlag = 0; atLineStart = 0;
                continue;
            }
        }

        /* ---- anything else ---- */
        if (isprint((unsigned char)c)) add_diag(line, col, "Unexpected character '%c'", c);
        else add_diag(line, col, "Unexpected byte 0x%02X", (unsigned char)c);
        mark(line, 1);
        i++;
        atLineStart = 0;
    }

    for (int k = 0; k < ncond && k < 64; k++)
        add_diag(condLine[k], 1, "Unterminated conditional directive (#if opened here has no #endif)");

    add_tok(T_EOF, "<EOF>", 5, line, 1, 0);
}

/* ═════════════════════════  ANALYSER / PARSER  ═══════════════════════════ */

#define MAXSTK 1024

typedef enum { K_NONE, K_FUNC, K_BLOCK, K_LOOP, K_SWITCH, K_STRUCT, K_ENUM, K_INIT } BKind;
typedef enum { PK_OTHER, PK_IF, PK_FOR, PK_WHILE, PK_DOWHILE, PK_SWITCH } PKind;
typedef struct { char open; int line, col; BKind kind; PKind pk; int semis; int td; int isStruct; } Ent;

static Ent stk[MAXSTK];
static int sp = 0;
static int ifPend[MAXSTK + 2];
static int nbSp[16], nbn = 0;                    /* brace-less loop bodies */
static char lbl[128][64]; static int nlbl = 0;   /* labels in current function */
static char gtn[128][64]; static int gtLine[128], gtCol[128], ngt = 0;

static Token *TK(int i)
{
    if (i < 0) i = 0;
    if (i >= g_ntok) i = g_ntok - 1;
    return &g_tok[i];
}
static int isP(int i, const char *s) { Token *t = TK(i); return t->type == T_PUNCT && !strcmp(t->text, s); }
static int isKw(int i, const char *s) { Token *t = TK(i); return t->type == T_KEYWORD && !strcmp(t->text, s); }

static int top_brace(void)
{
    for (int k = sp - 1; k >= 0; k--) if (stk[k].open == '{') return k;
    return -1;
}
static int in_kind(BKind k)
{
    for (int i = 0; i < sp; i++) if (stk[i].open == '{' && stk[i].kind == k) return 1;
    return 0;
}
static int loop_depth(void)
{
    int d = nbn;
    for (int i = 0; i < sp; i++) if (stk[i].open == '{' && stk[i].kind == K_LOOP) d++;
    return d;
}
static int block_depth(void)
{
    int d = 0;
    for (int i = 0; i < sp; i++)
        if (stk[i].open == '{' && (stk[i].kind == K_FUNC || stk[i].kind == K_BLOCK ||
                                   stk[i].kind == K_LOOP || stk[i].kind == K_SWITCH)) d++;
    return d;
}
static void count_loop(void)
{
    int d = loop_depth();
    if (d > 0) g_st.nestedLoops++;
    if (d + 1 > g_st.maxLoopNest) g_st.maxLoopNest = d + 1;
}

static int skip_balanced(int j)
{
    int depth = 0;
    for (; j < g_ntok && g_tok[j].type != T_EOF; j++) {
        if (g_tok[j].type != T_PUNCT) continue;
        char c = g_tok[j].text[0];
        if (c == '(' || c == '[' || c == '{') depth++;
        else if (c == ')' || c == ']' || c == '}') { depth--; if (depth <= 0) return j + 1; }
    }
    return j;
}

/* IDENT that probably names a type: "Foo bar", "Foo *bar;" ... returns 2 for macro-like prefixes */
static int ident_type_guess(int j)
{
    Token *n = TK(j + 1);
    if (n->type == T_IDENT) return 1;
    if (n->type == T_KEYWORD && (is_type_kw(n->text) || is_qual_kw(n->text) ||
        !strcmp(n->text, "struct") || !strcmp(n->text, "union") || !strcmp(n->text, "enum"))) return 2;
    if (isP(j + 1, "*")) {
        int k = j + 1;
        while (isP(k, "*")) k++;
        if (TK(k)->type == T_IDENT &&
            (isP(k + 1, ";") || isP(k + 1, "=") || isP(k + 1, ",") || isP(k + 1, "["))) return 1;
    }
    return 0;
}

static int parse_specifiers(int j, int *td, int *isStruct, int *bodyKind, int *hasType)
{
    *td = *isStruct = *bodyKind = *hasType = 0;
    for (;;) {
        Token *t = TK(j);
        if (t->type == T_KEYWORD) {
            if (!strcmp(t->text, "typedef")) { *td = 1; j++; }
            else if (is_qual_kw(t->text)) j++;
            else if (is_type_kw(t->text)) { *hasType = 1; j++; }
            else if (!strcmp(t->text, "struct") || !strcmp(t->text, "union") || !strcmp(t->text, "enum")) {
                int kind = !strcmp(t->text, "struct") ? 1 : !strcmp(t->text, "union") ? 2 : 3;
                *hasType = 1;
                if (kind != 3) *isStruct = 1;
                j++;
                if (TK(j)->type == T_IDENT) j++;
                if (isP(j, "{")) { *bodyKind = kind; return j; }
            } else break;
        } else if (t->type == T_IDENT) {
            if (!*hasType) {
                int g = (is_typedef_name(t->text)) ? 1 : ident_type_guess(j);
                if (g == 1) { *hasType = 1; j++; continue; }
                if (g == 2) { j++; continue; }               /* macro-ish prefix, e.g. EXPORT int f() */
            }
            if (t->text[0] == '_' && t->text[1] == '_' && isP(j + 1, "(")) { j = skip_balanced(j + 1); continue; }
            break;
        } else break;
    }
    return j;
}

/* mode: 0 = global, 1 = local, 2 = struct member. returns index where it stopped */
static int parse_declarators(int j, int td, int isStruct, int mode)
{
    for (;;) {
        int stars = 0, isArr = 0, haveName = 0;
        char name[72] = "";
        while (j < g_ntok) {
            if (isP(j, "*")) { stars++; j++; }
            else if (isKw(j, "const") || isKw(j, "volatile") || isKw(j, "restrict")) j++;
            else break;
        }
        if (isP(j, "(") && isP(j + 1, "*")) {              /* function pointer / ptr-to-array */
            int k = j + 1;
            while (isP(k, "*")) { k++; stars++; }
            if (TK(k)->type == T_IDENT) { snprintf(name, sizeof name, "%s", TK(k)->text); haveName = 1; }
            j = skip_balanced(j);
            while (isP(j, "(") || isP(j, "[")) j = skip_balanced(j);
            if (!stars) stars = 1;
        } else if (TK(j)->type == T_IDENT) {
            snprintf(name, sizeof name, "%s", TK(j)->text);
            haveName = 1;
            j++;
            if (isP(j, "(")) {                              /* function declarator */
                j = skip_balanced(j);
                if (td) { add_typedef(name); g_st.typedefs++; }
                else if (!isP(j, "{")) g_st.prototypes++;
                return j;
            }
            while (isP(j, "[")) { isArr = 1; j = skip_balanced(j); }
            if (isP(j, ":")) {                               /* bit-field width */
                while (j < g_ntok && TK(j)->type != T_EOF && !isP(j, ",") && !isP(j, ";")) j++;
            }
        } else return j;

        if (haveName) {
            if (td) { add_typedef(name); g_st.typedefs++; }
            else {
                if (mode == 2) g_st.members++;
                else {
                    if (mode == 0) g_st.globalVars++; else g_st.localVars++;
                    if (isStruct && !stars && !isArr) g_st.structVars++;
                }
                if (stars) g_st.pointers++;
                if (isArr) g_st.arrays++;
            }
        }
        if (isP(j, "=")) {                                   /* skip initializer */
            int nest = 0;
            j++;
            while (j < g_ntok && TK(j)->type != T_EOF) {
                Token *t = TK(j);
                if (t->type == T_PUNCT) {
                    char c = t->text[0];
                    if (c == '(' || c == '[' || c == '{') nest++;
                    else if (c == ')' || c == ']' || c == '}') { if (nest == 0) break; nest--; }
                    else if (nest == 0 && (c == ',' || c == ';') && t->text[1] == '\0') break;
                }
                j++;
            }
        }
        if (isP(j, ",")) { j++; continue; }
        return j;
    }
}

static int is_binop(const char *p)
{
    static const char *L[] = {"=","+=","-=","*=","/=","%=","&=","|=","^=","<<=",">>=","&&","||",
                              "==","!=","<=",">=","/","%","|","^","<<",">>","<",">","+","-","&",
                              ".","->","!","~", NULL};
    return in_list(L, p);
}

/* returns 1 if the stack top matched; otherwise tries to recover */
static int recover_close(int i, char open)
{
    Token *t = TK(i);
    int k;
    for (k = sp - 1; k >= 0; k--) if (stk[k].open == open) break;
    if (k < 0) { add_diag(t->line, t->col, "Unmatched '%c' (nothing to close)", t->text[0]); return 0; }
    add_diag(stk[sp - 1].line, stk[sp - 1].col, "'%c' opened here is never closed (found '%c' at line %d first)",
             stk[sp - 1].open, t->text[0], t->line);
    sp = k + 1;
    return 1;
}

static void analyze(void)
{
    sp = 0; nbn = 0; nlbl = 0; ngt = 0;
    memset(ifPend, 0, sizeof ifPend);
    int aggIdx = -1, aggKind = 0, aggTd = 0, aggStruct = 0;
    int loopBodyIdx = -1, switchBodyIdx = -1, ctlCloseIdx = -1, doSemiIdx = -1, whileTailIdx = -1;
    int doStack[64], doLine[64], dsp = 0;
    int skipDeclUntil = -1, inDecl = 0, lastCloseKind = K_NONE, lastCloseIdx = -1;

    if (g_ntok <= 1) { add_diag(1, 1, "File contains no C code"); return; }

    for (int i = 0; i < g_ntok && g_tok[i].type != T_EOF; i++) {
        Token *t = &g_tok[i];
        Token *pv = i > 0 ? &g_tok[i - 1] : NULL;
        Token *nx = &g_tok[i + 1 < g_ntok ? i + 1 : g_ntok - 1];
        const char *p = t->text;
        int isPunct = (t->type == T_PUNCT);
        int inParen = (sp > 0 && stk[sp - 1].open != '{');
        int tb = top_brace();
        BKind tk = tb >= 0 ? stk[tb].kind : K_NONE;
        int prevCtl = (i > 0 && ctlCloseIdx == i - 1);
        int isLoopBrace = 0, isSwitchBrace = 0;

        if (i == loopBodyIdx) {
            if (isPunct && p[0] == '{') isLoopBrace = 1;
            else if (nbn < 16) nbSp[nbn++] = sp;
        }
        if (i == switchBodyIdx && isPunct && p[0] == '{') isSwitchBrace = 1;
        if (i == doSemiIdx && !(isPunct && p[0] == ';'))
            add_diag(pv->line, pv->col, "Missing ';' after the do-while condition");

        /* ---------- statement start / declarations / labels ---------- */
        int stmtStart = 0;
        if (i == 0) stmtStart = 1;
        else if (t->ppBefore && !inParen) stmtStart = 1;
        else if (pv->type == T_PUNCT) {
            if (!strcmp(pv->text, ";")) stmtStart = !inParen || stk[sp - 1].pk == PK_FOR;
            else if (!strcmp(pv->text, "{")) stmtStart = 1;
            else if (!strcmp(pv->text, "}")) stmtStart = (lastCloseIdx == i - 1 && lastCloseKind != K_INIT &&
                                                          lastCloseKind != K_STRUCT && lastCloseKind != K_ENUM);
            else if (!strcmp(pv->text, "(")) stmtStart = inParen && stk[sp - 1].pk == PK_FOR;
        }

        if (stmtStart && i > skipDeclUntil && tk != K_INIT && tk != K_ENUM &&
            !(inParen && stk[sp - 1].pk != PK_FOR) &&
            (t->type == T_KEYWORD || t->type == T_IDENT)) {
            int td, isS, body, hasT;
            int j = parse_specifiers(i, &td, &isS, &body, &hasT);
            if (hasT && j > i) {
                inDecl = 1;
                int mode = (tb < 0) ? 0 : (tk == K_STRUCT ? 2 : 1);
                if (body) { aggIdx = j; aggKind = body; aggTd = td; aggStruct = isS; }
                else skipDeclUntil = parse_declarators(j, td, isS, mode);
            }
        }

        if (stmtStart && t->type == T_IDENT && isP(i + 1, ":") && !isP(i + 2, ":") &&
            sp > 0 && !inParen && tk != K_STRUCT && tk != K_INIT) {
            g_st.labels++;
            if (nlbl < 128) snprintf(lbl[nlbl++], 64, "%.63s", p);
        }

        if (sp == 0 && stmtStart && !inDecl && t->type == T_IDENT && nx->type == T_PUNCT &&
            (!strcmp(nx->text, "=") || !strcmp(nx->text, "+=") || !strcmp(nx->text, "-=") ||
             !strcmp(nx->text, "++") || !strcmp(nx->text, "--")))
            add_diag(t->line, t->col, "Statement '%s %s ...' outside of a function (only declarations allowed here)",
                     p, nx->text);

        /* ---------- heuristics for missing ';' and stray operands ---------- */
        int funcCtx = (sp > 0 && !inParen && (tk == K_FUNC || tk == K_BLOCK || tk == K_LOOP || tk == K_SWITCH));
        if (pv && !t->ppBefore) {
            if (pv->type == T_NUMBER && !(funcCtx && t->type == T_KEYWORD && starts_stmt_kw(p)) &&
                                        (t->type == T_IDENT || t->type == T_KEYWORD || t->type == T_NUMBER ||
                                         t->type == T_STRING || t->type == T_CHAR))
                add_diag(pv->line, pv->col, "Missing operator or ';' between '%s' and '%s'", pv->text,
                         t->type == T_STRING ? "\"...\"" : t->type == T_CHAR ? "'...'" : t->text);
            else if (pv->type == T_STRING && (t->type == T_NUMBER || t->type == T_CHAR))
                add_diag(pv->line, pv->col, "Missing operator or ';' after string literal");
            else if (pv->type == T_CHAR && (t->type == T_IDENT || t->type == T_NUMBER || t->type == T_STRING || t->type == T_CHAR))
                add_diag(pv->line, pv->col, "Missing operator or ';' after character constant");
            else if (pv->type == T_IDENT && (t->type == T_NUMBER || t->type == T_CHAR))
                add_diag(pv->line, pv->col, "Missing operator or ';' between '%s' and '%s'", pv->text,
                         t->type == T_CHAR ? "'...'" : t->text);

            if (funcCtx) {
                int prevOperand = pv->type == T_NUMBER || pv->type == T_STRING || pv->type == T_CHAR ||
                                  pv->type == T_IDENT ||
                                  (pv->type == T_PUNCT && (!strcmp(pv->text, "]") || !strcmp(pv->text, "++") ||
                                                           !strcmp(pv->text, "--") ||
                                                           (!strcmp(pv->text, ")") && !prevCtl)));
                if (prevOperand && t->type == T_KEYWORD && starts_stmt_kw(p))
                    add_diag(pv->line, pv->col + (int)strlen(pv->text), "Missing ';' after '%s' (before '%s' on line %d)",
                             pv->type == T_STRING ? "\"...\"" : pv->type == T_CHAR ? "'...'" : pv->text, p, t->line);
                else if (pv->type == T_IDENT && t->type == T_IDENT && !inDecl)
                    add_diag(pv->line, pv->col, "Missing ';' or operator between '%s' and '%s'", pv->text, p);
            }
        }

        /* ═══════════════════════════ punctuators ═══════════════════════════ */
        if (isPunct) {
            if (p[0] == '(' && p[1] == '\0') {
                PKind pk = PK_OTHER;
                if (pv && pv->type == T_KEYWORD) {
                    if (!strcmp(pv->text, "if")) pk = PK_IF;
                    else if (!strcmp(pv->text, "for")) pk = PK_FOR;
                    else if (!strcmp(pv->text, "switch")) pk = PK_SWITCH;
                    else if (!strcmp(pv->text, "while")) pk = (whileTailIdx == i) ? PK_DOWHILE : PK_WHILE;
                }
                if (sp >= MAXSTK) { add_diag(t->line, t->col, "Nesting too deep - analysis stopped"); break; }
                stk[sp] = (Ent){'(', t->line, t->col, K_NONE, pk, 0, 0, 0};
                sp++;
            } else if (p[0] == '[' && p[1] == '\0') {
                if (sp >= MAXSTK) { add_diag(t->line, t->col, "Nesting too deep - analysis stopped"); break; }
                stk[sp] = (Ent){'[', t->line, t->col, K_NONE, PK_OTHER, 0, 0, 0};
                sp++;
            } else if (p[0] == '{' && p[1] == '\0') {
                BKind kind;
                inDecl = 0;
                if (i == aggIdx) {
                    kind = (aggKind == 3) ? K_ENUM : K_STRUCT;
                    if (aggKind == 1) g_st.structs++; else if (aggKind == 2) g_st.unions++; else g_st.enums++;
                } else if (isLoopBrace) kind = K_LOOP;
                else if (isSwitchBrace) kind = K_SWITCH;
                else if (inParen || isP(i - 1, "=") || tk == K_INIT || tk == K_ENUM ||
                         (pv && isP(i - 1, ")") && !prevCtl && sp > 0)) kind = K_INIT;
                else if (sp == 0) {
                    kind = K_FUNC;
                    if (pv && (pv->type == T_IDENT || pv->type == T_KEYWORD))
                        add_diag(t->line, t->col, "Unexpected '{' at file scope (missing parameter list or ';'?)");
                } else kind = K_BLOCK;

                if (sp >= MAXSTK) { add_diag(t->line, t->col, "Nesting too deep - analysis stopped"); break; }
                stk[sp] = (Ent){'{', t->line, t->col, kind, PK_OTHER, 0, (i == aggIdx) ? aggTd : 0,
                                (i == aggIdx) ? aggStruct : 0};
                sp++;
                ifPend[sp] = 0;
                if (kind == K_FUNC) {
                    g_st.functions++;
                    nlbl = 0; ngt = 0;
                    if (isP(i - 1, ")")) {
                        int k = i - 1, d = 0;
                        for (; k >= 0; k--) {
                            if (isP(k, ")")) d++;
                            else if (isP(k, "(")) { d--; if (d == 0) break; }
                        }
                        if (k > 0 && g_tok[k - 1].type == T_IDENT && !strcmp(g_tok[k - 1].text, "main")) g_st.hasMain = 1;
                    }
                }
                int bd = block_depth();
                if (bd > g_st.maxBlockDepth) g_st.maxBlockDepth = bd;
            } else if ((p[0] == ')' || p[0] == ']') && p[1] == '\0') {
                char open = (p[0] == ')') ? '(' : '[';
                int ok = (sp > 0 && stk[sp - 1].open == open);
                if (!ok) ok = recover_close(i, open);
                if (!ok) continue;
                Ent e = stk[sp - 1];
                sp--;
                if (open == '(') {
                    if (e.pk == PK_FOR && e.semis != 2)
                        add_diag(e.line, e.col, "'for' header needs exactly two ';' (found %d)", e.semis);
                    if ((e.pk == PK_IF || e.pk == PK_WHILE || e.pk == PK_DOWHILE || e.pk == PK_SWITCH) &&
                        pv && isP(i - 1, "("))
                        add_diag(e.line, e.col, "Empty condition in parentheses");
                    if (e.pk == PK_FOR || e.pk == PK_WHILE) { loopBodyIdx = i + 1; ctlCloseIdx = i; }
                    else if (e.pk == PK_IF) ctlCloseIdx = i;
                    else if (e.pk == PK_SWITCH) { switchBodyIdx = i + 1; ctlCloseIdx = i; }
                    else if (e.pk == PK_DOWHILE) doSemiIdx = i + 1;
                }
            } else if (p[0] == '}' && p[1] == '\0') {
                int ok = (sp > 0 && stk[sp - 1].open == '{');
                if (!ok) ok = recover_close(i, '{');
                if (!ok) continue;
                Ent e = stk[sp - 1];
                if (pv && !t->ppBefore && e.kind != K_INIT && e.kind != K_ENUM) {
                    if (prevCtl) add_diag(t->line, t->col, "Expected a statement before '}'");
                    else if (pv->type == T_KEYWORD && (!strcmp(pv->text, "else") || !strcmp(pv->text, "do")))
                        add_diag(pv->line, pv->col, "Expected a statement after '%s'", pv->text);
                    else if (!(pv->type == T_PUNCT && (!strcmp(pv->text, ";") || !strcmp(pv->text, "{") ||
                                                       !strcmp(pv->text, "}") || !strcmp(pv->text, ":"))))
                        add_diag(pv->line, pv->col + (int)strlen(pv->text), "Missing ';' before '}' (after '%s')",
                                 pv->type == T_STRING ? "\"...\"" : pv->type == T_CHAR ? "'...'" : pv->text);
                }
                sp--;
                inDecl = 0;
                lastCloseKind = e.kind;
                lastCloseIdx = i;
                while (nbn > 0 && nbSp[nbn - 1] >= sp) nbn--;
                if (e.kind == K_FUNC) {
                    for (int g = 0; g < ngt; g++) {
                        int found = 0;
                        for (int l = 0; l < nlbl; l++) if (!strcmp(gtn[g], lbl[l])) { found = 1; break; }
                        if (!found) add_diag(gtLine[g], gtCol[g], "goto target '%s' has no label in this function", gtn[g]);
                    }
                    nlbl = 0; ngt = 0;
                } else if (e.kind == K_STRUCT || e.kind == K_ENUM) {
                    int tb2 = top_brace();
                    int mode = (tb2 < 0) ? 0 : (stk[tb2].kind == K_STRUCT ? 2 : 1);
                    skipDeclUntil = parse_declarators(i + 1, e.td, e.isStruct, mode);
                    inDecl = 1;
                }
            } else if (p[0] == ';' && p[1] == '\0') {
                if (inParen) {
                    if (stk[sp - 1].open == '(' && stk[sp - 1].pk == PK_FOR) stk[sp - 1].semis++;
                    else {
                        add_diag(stk[sp - 1].line, stk[sp - 1].col,
                                 "'%c' is not closed before the ';' on line %d", stk[sp - 1].open, t->line);
                        sp = tb + 1;                      /* recover: drop the open ( or [ */
                        inDecl = 0;
                    }
                } else {
                    inDecl = 0;
                    while (nbn > 0 && nbSp[nbn - 1] >= sp) nbn--;
                    if (tk == K_INIT || tk == K_ENUM) add_diag(t->line, t->col, "Unexpected ';' inside an initializer");
                }
            } else if (!strcmp(p, "?")) g_st.ternaries++;
            else if (!strcmp(p, "&&")) g_st.andOps++;
            else if (!strcmp(p, "||")) g_st.orOps++;
            else if (!strcmp(p, "!")) g_st.notOps++;

            if (is_binop(p) && p[0] != '.' ) {
                Token *n = nx;
                if (n->type == T_EOF || (n->type == T_PUNCT && (!strcmp(n->text, ")") || !strcmp(n->text, "]") ||
                                         !strcmp(n->text, "}") || !strcmp(n->text, ";") || !strcmp(n->text, ","))))
                    add_diag(t->line, t->col, "Expected an expression after '%s'", p);
            }
            if (!strcmp(p, ",")) {
                if (nx->type == T_PUNCT && (!strcmp(nx->text, ")") || !strcmp(nx->text, ";") || !strcmp(nx->text, "]")))
                    add_diag(t->line, t->col, "Expected an expression after ','");
            }
            continue;
        }

        /* ═══════════════════════════ keywords ═══════════════════════════ */
        if (t->type == T_KEYWORD) {
            if (sp == 0 && is_ctl_kw(p)) {
                add_diag(t->line, t->col, "'%s' statement outside of a function body", p);
                continue;
            }
            if (!strcmp(p, "if")) {
                g_st.ifs++;
                ifPend[sp]++;
                if (!isP(i + 1, "(")) add_diag(t->line, t->col, "Expected '(' after 'if'");
            } else if (!strcmp(p, "else")) {
                g_st.elses++;
                if (isKw(i + 1, "if")) g_st.elseIfs++;
                if (prevCtl) add_diag(t->line, t->col, "Expected a statement between ')' and 'else'");
                if (ifPend[sp] > 0) ifPend[sp]--;
                else add_diag(t->line, t->col, "'else' without a matching 'if'");
                if (isP(i + 1, "}") || nx->type == T_EOF) add_diag(t->line, t->col, "Expected a statement after 'else'");
            } else if (!strcmp(p, "for")) {
                g_st.fors++;
                count_loop();
                if (!isP(i + 1, "(")) add_diag(t->line, t->col, "Expected '(' after 'for'");
            } else if (!strcmp(p, "while")) {
                int tail = (dsp > 0 && doStack[dsp - 1] == sp && pv &&
                            (isP(i - 1, ";") || isP(i - 1, "}")));
                if (tail) { dsp--; whileTailIdx = i + 1; }
                else { g_st.whiles++; count_loop(); }
                if (!isP(i + 1, "(")) add_diag(t->line, t->col, "Expected '(' after 'while'");
            } else if (!strcmp(p, "do")) {
                g_st.dos++;
                count_loop();
                if (dsp < 64) { doStack[dsp] = sp; doLine[dsp] = t->line; dsp++; }
                loopBodyIdx = i + 1;
                if (isP(i + 1, "}") || nx->type == T_EOF) add_diag(t->line, t->col, "Expected a statement after 'do'");
            } else if (!strcmp(p, "switch")) {
                g_st.switches++;
                if (!isP(i + 1, "(")) add_diag(t->line, t->col, "Expected '(' after 'switch'");
            } else if (!strcmp(p, "case")) {
                g_st.cases++;
                if (!in_kind(K_SWITCH)) add_diag(t->line, t->col, "'case' label outside of a switch");
                if (isP(i + 1, ":")) add_diag(t->line, t->col, "Expected a constant expression after 'case'");
                int d = 0, found = 0;
                for (int j = i + 1; j < g_ntok && g_tok[j].type != T_EOF; j++) {
                    Token *q = &g_tok[j];
                    if (q->type != T_PUNCT) continue;
                    if (!strcmp(q->text, "(") || !strcmp(q->text, "[")) d++;
                    else if (!strcmp(q->text, ")") || !strcmp(q->text, "]")) d--;
                    else if (d <= 0 && (!strcmp(q->text, ";") || !strcmp(q->text, "{") || !strcmp(q->text, "}"))) break;
                    else if (d <= 0 && !strcmp(q->text, ":")) { found = 1; break; }
                }
                if (!found) add_diag(t->line, t->col, "Missing ':' after 'case' expression");
            } else if (!strcmp(p, "default")) {
                g_st.defaults++;
                if (!in_kind(K_SWITCH)) add_diag(t->line, t->col, "'default' label outside of a switch");
                if (!isP(i + 1, ":")) add_diag(t->line, t->col, "Expected ':' after 'default'");
            } else if (!strcmp(p, "break")) {
                g_st.breaks++;
                if (!(in_kind(K_LOOP) || in_kind(K_SWITCH) || nbn > 0))
                    add_diag(t->line, t->col, "'break' outside of a loop or switch");
                if (!isP(i + 1, ";")) add_diag(t->line, t->col, "Expected ';' after 'break'");
            } else if (!strcmp(p, "continue")) {
                g_st.continues++;
                if (!(in_kind(K_LOOP) || nbn > 0)) add_diag(t->line, t->col, "'continue' outside of a loop");
                if (!isP(i + 1, ";")) add_diag(t->line, t->col, "Expected ';' after 'continue'");
            } else if (!strcmp(p, "goto")) {
                g_st.gotos++;
                if (nx->type != T_IDENT || !isP(i + 2, ";"))
                    add_diag(t->line, t->col, "'goto' must be followed by a label name and ';'");
                else if (ngt < 128) {
                    snprintf(gtn[ngt], 64, "%.63s", nx->text);
                    gtLine[ngt] = nx->line; gtCol[ngt] = nx->col; ngt++;
                }
            } else if (!strcmp(p, "return")) g_st.returns++;
        }
    }

    /* unclosed brackets / do-loops */
    for (int k = 0; k < sp; k++)
        add_diag(stk[k].line, stk[k].col, "Unclosed '%c' - no matching '%c' before end of file",
                 stk[k].open, stk[k].open == '(' ? ')' : stk[k].open == '[' ? ']' : '}');
    for (int k = 0; k < dsp; k++) add_diag(doLine[k], 1, "'do' without a matching 'while'");
}

static int diag_cmp(const void *a, const void *b)
{
    const Diag *x = a, *y = b;
    if (x->line != y->line) return x->line - y->line;
    return x->col - y->col;
}

/* ═════════════════════════════  FILE HANDLING  ═══════════════════════════ */

static void reset_state(void)
{
    free(g_src);  g_src = NULL; g_len = 0;
    free(g_tok);  g_tok = NULL; g_ntok = g_captok = 0;
    free(g_diag); g_diag = NULL; g_ndiag = g_capdiag = g_dropped = 0;
    free(g_lf);   g_lf = NULL; g_lfN = 0;
    memset(&g_st, 0, sizeof g_st);
    g_ntd = 0; g_loaded = 0; g_utf8ok = 1; g_bom = 0;
}

static int process_file(const char *path)
{
    reset_state();
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 0; }
    g_src = malloc((size_t)sz + 1);
    if (!g_src) { fclose(f); return 0; }
    g_len = fread(g_src, 1, (size_t)sz, f);
    g_src[g_len] = '\0';
    fclose(f);
    snprintf(g_path, sizeof g_path, "%s", path);

    g_utf8ok = utf8_valid((const unsigned char *)g_src, g_len);
    int lines = 0;
    for (size_t i = 0; i < g_len; i++) if (g_src[i] == '\n') lines++;
    if (g_len > 0 && g_src[g_len - 1] != '\n') lines++;
    g_st.totalLines = lines;
    g_lfN = lines;
    g_lf = calloc((size_t)lines + 2, 1);

    lex();
    analyze();

    for (int l = 1; l <= lines; l++) {
        unsigned char fl = g_lf ? g_lf[l] : 0;
        if (fl & 1) g_st.codeLines++;
        else if (fl & 2) g_st.commentOnlyLines++;
        else g_st.blankLines++;
    }
    qsort(g_diag, (size_t)g_ndiag, sizeof(Diag), diag_cmp);
    g_loaded = 1;
    return 1;
}

static void get_src_line(int line, char *out, size_t osz)
{
    out[0] = '\0';
    if (!g_src || line < 1) return;
    const char *p = g_src;
    int l = 1;
    while (*p && l < line) { if (*p == '\n') l++; p++; }
    if (l != line) return;
    while (*p == ' ' || *p == '\t') p++;
    clip(out, osz, p, BOX_W - 8);
}

/* ═══════════════════════════════  SCREENS  ═══════════════════════════════ */

static void show_menu(void)
{
    printf("\n");
    box_top();
    box_center(BOLD YEL, "C  SYNTAX  CHECKER  &  CODE  ANALYZER");
    box_center(DIM, "UTF-8 edition  ·  medium-depth validation");
    box_mid();
    box_text(" " BOLD GRN "[1]" RST "  ▶  Check a C source file");
    box_text(" " BOLD GRN "[2]" RST "  ▶  Show analysis report (last file)");
    box_text(" " BOLD GRN "[3]" RST "  ▶  Show syntax errors (last file)");
    box_text(" " BOLD GRN "[4]" RST "  ▶  About / what is checked");
    box_text(" " BOLD RED "[0]" RST "  ✘  Exit");
    box_bot();
}

static void show_verdict(void)
{
    char tmp[256];
    printf("\n");
    box_top();
    if (g_ndiag == 0) box_center(BOLD GRN, "✔  VALID C SYNTAX  ✔");
    else {
        char b[96];
        snprintf(b, sizeof b, "✘  INVALID  -  %d issue%s found", g_ndiag + g_dropped, (g_ndiag + g_dropped) == 1 ? "" : "s");
        box_center(BOLD RED, b);
    }
    box_mid();
    clip(tmp, sizeof tmp, g_path, BOX_W - 9);
    box_text(BOLD "File  " RST "%s", tmp);
    box_text(BOLD "Size  " RST "%zu bytes   " BOLD "Lines  " RST "%d   " BOLD "Tokens  " RST "%ld",
             g_len, g_st.totalLines, g_st.tokens);
    box_text(BOLD "Text  " RST "%s%s", g_utf8ok ? GRN "valid UTF-8" RST : YEL "not valid UTF-8 (check encoding)" RST,
             g_bom ? "  (BOM detected)" : "");
    box_bot();
}

static void show_errors(int limit)
{
    if (!g_loaded) { printf(YEL "\n  No file analysed yet - use option 1.\n" RST); return; }
    printf("\n");
    box_top();
    box_center(BOLD RED, "SYNTAX ERRORS");
    box_mid();
    if (g_ndiag == 0) {
        box_text(GRN "✔ No syntax problems detected." RST);
    } else {
        int shown = (limit > 0 && g_ndiag > limit) ? limit : g_ndiag;
        for (int i = 0; i < shown; i++) {
            char m[300], src[300];
            clip(m, sizeof m, g_diag[i].msg, BOX_W - 18);
            get_src_line(g_diag[i].line, src, sizeof src);
            box_text(RED "✘" RST " " BOLD "L%d:%d" RST "  %s", g_diag[i].line, g_diag[i].col, m);
            if (src[0]) box_text(DIM "    │ %s" RST, src);
            if (i + 1 < shown) box_thin();
        }
        if (shown < g_ndiag || g_dropped)
            box_text(YEL "… %d more not shown (choose option 3 for the full list)" RST, g_ndiag - shown + g_dropped);
        box_thin();
        box_text(DIM "Tip: fix the first error first - later ones are often side effects." RST);
    }
    box_bot();
}

static void show_report(void)
{
    if (!g_loaded) { printf(YEL "\n  No file analysed yet - use option 1.\n" RST); return; }
    Stats *s = &g_st;
    char tmp[256];
    printf("\n");
    box_top();
    box_center(BOLD YEL, "CODE ANALYSIS REPORT");
    box_mid();
    clip(tmp, sizeof tmp, g_path, BOX_W - 9);
    box_text(BOLD "File  " RST "%s", tmp);
    box_text(BOLD "Syntax" RST " %s", g_ndiag == 0 ? GRN "✔ valid" RST : RED "✘ invalid (counts below may be incomplete)" RST);

    box_section("Lines");
    box_kv("Total lines", s->totalLines);
    box_kv("Code lines", s->codeLines);
    box_kv("Comment-only lines", s->commentOnlyLines);
    box_kv("Blank lines", s->blankLines);

    box_section("Comments");
    box_kv("Line comments  //", s->lineComments);
    box_kv("Block comments /* */", s->blockComments);
    box_kv("Total comments", s->lineComments + s->blockComments);

    box_section("Preprocessor");
    box_kv("#include", s->includes);
    box_kv("#define", s->defines);
    box_kv("Other directives", s->otherDirectives);

    box_section("Logic / control statements");
    box_kv("if statements", s->ifs);
    box_kv("else branches", s->elses);
    box_kv("else-if chains", s->elseIfs);
    box_kv("switch statements", s->switches);
    box_kv("case labels", s->cases);
    box_kv("default labels", s->defaults);
    box_kv("Ternary operators  ?:", s->ternaries);
    box_kv("Logical AND  &&", s->andOps);
    box_kv("Logical OR   ||", s->orOps);
    box_kv("Logical NOT  !", s->notOps);
    box_kv("Total decision points", s->ifs + s->switches + s->cases + s->ternaries + s->andOps + s->orOps);
    box_kv("break", s->breaks);
    box_kv("continue", s->continues);
    box_kv("return", s->returns);
    box_kv("goto", s->gotos);
    box_kv("Labels", s->labels);

    box_section("Loops");
    box_kv("for loops", s->fors);
    box_kv("while loops", s->whiles);
    box_kv("do-while loops", s->dos);
    box_kv("Total loops", s->fors + s->whiles + s->dos);
    box_kv("Nested loops", s->nestedLoops);
    box_kv("Deepest loop nesting", s->maxLoopNest);

    box_section("Declarations");
    box_kv("Functions defined", s->functions);
    box_kv("Function prototypes", s->prototypes);
    box_kvs("main() present", s->hasMain ? "yes" : "no", s->hasMain ? GRN : YEL);
    box_kv("Global variables", s->globalVars);
    box_kv("Local variables", s->localVars);
    box_kv("Total variables", s->globalVars + s->localVars);
    box_kv("Pointers declared", s->pointers);
    box_kv("Arrays declared", s->arrays);
    box_kv("struct types defined", s->structs);
    box_kv("union types defined", s->unions);
    box_kv("enum types defined", s->enums);
    box_kv("struct/union variables", s->structVars);
    box_kv("struct/union members", s->members);
    box_kv("typedefs", s->typedefs);

    box_section("Structure");
    box_kv("Deepest block nesting", s->maxBlockDepth);
    box_kv("String literals", s->strings);
    box_kv("Char literals", s->chars);
    box_kv("Tokens", s->tokens);
    box_bot();
}

static void show_about(void)
{
    printf("\n");
    box_top();
    box_center(BOLD YEL, "ABOUT");
    box_mid();
    box_text(BOLD "Checks performed" RST);
    box_text("  ✔ balanced and correctly nested  ( ) [ ] { }");
    box_text("  ✔ unterminated strings, chars and /* comments");
    box_text("  ✔ malformed numbers and #preprocessor lines");
    box_text("  ✔ #if / #endif pairing, #if 0 blocks skipped");
    box_text("  ✔ if / for / while / switch need '(' ... ')'");
    box_text("  ✔ for-header needs exactly two ';'");
    box_text("  ✔ else without if, case/default outside switch");
    box_text("  ✔ break / continue outside a loop or switch");
    box_text("  ✔ goto targets that have no label");
    box_text("  ✔ missing ';' (before '}', before keywords, ...)");
    box_text("  ✔ statements placed outside any function");
    box_thin();
    box_text(BOLD "Limits (heuristic checker, not a compiler)" RST);
    box_text("  • no type checking, no undeclared-name checks");
    box_text("  • macro tricks and K&R definitions may false-alarm");
    box_text("  • both branches of #if/#else are scanned");
    box_text("  • variable counts are declaration-based estimates");
    box_bot();
}

/* ═══════════════════════════════  INPUT  ═════════════════════════════════ */

static int read_line(char *buf, size_t n)
{
    if (!fgets(buf, (int)n, stdin)) return 0;
    size_t l = strlen(buf);
    while (l > 0 && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = '\0';
    return 1;
}

static int ask_yes_no(const char *q)
{
    char b[64];
    for (;;) {
        printf("\n" CYN " ▶ " RST "%s " DIM "[Y/n]" RST " : ", q);
        fflush(stdout);
        if (!read_line(b, sizeof b)) return 0;
        trim(b);
        if (b[0] == '\0' || b[0] == 'y' || b[0] == 'Y') return 1;
        if (b[0] == 'n' || b[0] == 'N') return 0;
        printf(YEL "   Please answer y or n.\n" RST);
    }
}

static void check_file_flow(const char *given)
{
    char path[512];
    if (given) snprintf(path, sizeof path, "%s", given);
    else {
        printf("\n" CYN " ▶ " RST "Path of the C file (e.g. main.c) : ");
        fflush(stdout);
        if (!read_line(path, sizeof path)) return;
    }
    trim(path);
    size_t l = strlen(path);
    if (l >= 2 && ((path[0] == '"' && path[l - 1] == '"') || (path[0] == '\'' && path[l - 1] == '\''))) {
        path[l - 1] = '\0';
        memmove(path, path + 1, l - 1);
    }
    if (path[0] == '\0') { printf(YEL "\n  No path entered.\n" RST); return; }

    if (!process_file(path)) {
        printf(RED "\n  ✘ Cannot open '%s'.\n" RST, path);
        return;
    }
    show_verdict();
    if (g_ndiag > 0) show_errors(12);
    if (ask_yes_no("Display the analysis report?")) show_report();
}

/* ═══════════════════════════════  MAIN  ══════════════════════════════════ */

int main(int argc, char **argv)
{
    init_terminal();
    if (argc > 1) check_file_flow(argv[1]);

    for (;;) {
        char b[64];
        show_menu();
        printf(CYN " ▶ " RST "Choose an option : ");
        fflush(stdout);
        if (!read_line(b, sizeof b)) break;
        trim(b);
        switch (b[0]) {
            case '1': check_file_flow(NULL); break;
            case '2': show_report(); break;
            case '3': show_errors(0); break;
            case '4': show_about(); break;
            case '0':
                printf("\n" GRN "  Goodbye! ✔" RST "\n\n");
                reset_state();
                return 0;
            default:
                printf(YEL "\n  Unknown option '%s' - pick 0-4.\n" RST, b);
        }
    }
    reset_state();
    return 0;
}
