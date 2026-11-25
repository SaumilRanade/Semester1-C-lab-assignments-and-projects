/* ===================== PART 1 (Aniket): SETUP, HELPERS, MAIN =====================
   Wireless Link Testing - Signal Strength Threshold Test Suite (any number of readings)
   The user types the data file name and the threshold. The readings (1 to MAX_READINGS)
   are read from that file. After that a menu offers:
   1 Detailed report, 2 Summary report, 3 Exit.

   Compile: gcc -o signal_test_suite signal_test_suite_v2.c -lm
   Run    : ./signal_test_suite          (Windows: .\signal_test_suite.exe)

   WHAT CHANGED COMPARED WITH THE FIXED 50-READING VERSION
   --------------------------------------------------------
   Part 1 : TEST_CASES (50) is gone. Arrays hold MAX_READINGS values and the real
            count n comes from the file. main asks for the file name and asks again
            if the file is bad. detailed_report / summary_report get the file name.
            animate_tests speed now depends on n.
   Part 2 : load_readings reads ANY number of readings (1 to MAX_READINGS) and rejects
            a line with extra text after the number. New read_filename().
   Part 3 : new t_critical_95(df) so the confidence interval works for any n.
   Part 4 : print_table "No." column is 4 digits wide.
   Part 5 : no change.
   Part 6 : print_trace groups readings when there are more than TRACE_COLS (60) so it
            still fits on the screen. print_statistics works for any n (n = 1 is handled).
   Part 7 : no change (print_dynamics and print_verdict already work for any n).
   ================================================================================= */

#define _POSIX_C_SOURCE 200809L          /* needed for nanosleep on Linux/Mac */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define IS_TTY() _isatty(_fileno(stdout))
#else
#include <unistd.h>
#define IS_TTY() isatty(STDOUT_FILENO)
#endif

#define MAX_READINGS 1000        /* most readings the program accepts              */
#define DATA_FILE    "signal_readings.txt"   /* default file name                   */
#define MIN_DBM      -120.0
#define MAX_DBM      0.0
#define EPSILON      0.000000001
#define Z_95         1.96        /* normal value used for the Wilson interval      */
#define RELIABLE_PCT 95.0
#define MARGINAL_PCT 80.0
#define CHART_HEIGHT 10
#define TRACE_HEIGHT 20
#define TRACE_COLS   60          /* most columns drawn in the signal trace         */
#define INNER_W      71          /* width between the left and right borders       */

/* Class numbers for each reading */
#define ABOVE 0
#define EQUAL 1
#define BELOW 2

/* Colour codes (ANSI). They are only used when the output is a real terminal. */
#define RESET  "\x1b[0m"
#define BOLD   "\x1b[1m"
#define RED    "\x1b[31m"
#define GREEN  "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN   "\x1b[36m"

const char *CLASS_LABEL[3] = {"ABOVE threshold", "EQUAL to threshold", "BELOW threshold"};
const char *CLASS_SHORT[3] = {"ABOVE", "EQUAL", "BELOW"};
const char *CLASS_COLOR[3] = {GREEN, YELLOW, RED};

int use_color = 0;               /* 1 = print colours, 0 = plain text */

/* ---- prototypes: the contract between all 7 parts ---- */
const char *paint(const char *code);
void   pause_ms(int ms);
void   repeat(const char *s, int n);
void   draw_line(const char *left, const char *mid, const char *right, const int w[], int cols);
void   print_title(const char *text, const char *color);
void   print_row(const char *label, const char *value, int label_w, int value_w, const char *color);
void   read_filename(char *name, int size);
double read_threshold(void);
int    load_readings(const char *path, double r[], int max);
int    classify(double reading, double threshold);
void   sort_array(double a[], int n);
double percentile(double sorted[], int n, double p);
double t_critical_95(int df);
const char *grade_of(double pct);
const char *grade_color(double pct);
void   print_setup(const char *path, double r[], int n, double thr);
void   print_ledger(double r[], int cls[], int n);
void   animate_tests(double r[], int cls[], int n);
void   print_table(double r[], int cls[], int n, double thr);
void   print_summary(int n, int count[]);
void   print_class_chart(int count[], int n);
void   print_histogram(double r[], int n, double thr);
void   print_trace(double r[], int cls[], int n, double thr);
void   print_statistics(double r[], int n, double thr);
void   print_dynamics(double r[], int cls[], int n, double thr);
void   print_verdict(int pass, int n);

/* ---- menu prototypes ---- */
int    show_menu(void);
void   detailed_report(double r[], int cls[], int n, double thr, int count[], const char *path);
void   summary_report(double r[], int n, double thr, int count[], const char *path);

/* ---- main ---- */
int main(void) {
    double readings[MAX_READINGS];
    int cls[MAX_READINGS];
    int count[3] = {0, 0, 0};
    char data_file[260];
    double threshold;
    int tty, i, n, choice;

#ifdef _WIN32
    /* make the Windows console show the box characters and understand colours */
    HANDLE h;
    DWORD mode;
    SetConsoleOutputCP(65001);
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004);   /* enable ANSI colour codes */
#endif

    tty = IS_TTY();
    use_color = tty;                         /* colours and animation only on a terminal */

    print_title("WIRELESS LINK TESTING - SIGNAL THRESHOLD TEST SUITE", CYAN);
    printf("\n");

    /* 1. ask for the data file and load it; ask again if the file is bad */
    while (1) {
        read_filename(data_file, (int)sizeof data_file);
        n = load_readings(data_file, readings, MAX_READINGS);
        if (n > 0)
            break;
        printf("Please try again.\n\n");
    }
    printf("Loaded %d reading(s) from \"%s\".\n\n", n, data_file);

    /* 2. the threshold typed by the user */
    threshold = read_threshold();

    /* 3. classify every reading and count each class */
    for (i = 0; i < n; i++) {
        cls[i] = classify(readings[i], threshold);
        count[cls[i]]++;
    }

    /* 4. menu loop: 1 = detailed report, 2 = summary report, 3 = exit */
    do {
        choice = show_menu();
        if (choice == 1)
            detailed_report(readings, cls, n, threshold, count, data_file);
        else if (choice == 2)
            summary_report(readings, n, threshold, count, data_file);
    } while (choice != 3);

    printf("\nExiting. Goodbye!\n");
    return 0;
}

/* ---- menu ---- */

/* show the menu until a valid choice (1-3) is typed */
int show_menu(void) {
    char line[64];
    char extra;
    int c;

    while (1) {
        printf("\n");
        print_title("MAIN MENU", CYAN);
        printf("  1. Detailed report\n");
        printf("  2. Summary report\n");
        printf("  3. Exit\n\n");
        printf("Enter your choice (1-3): ");
        if (fgets(line, sizeof line, stdin) == NULL)
            return 3;                        /* input ended: exit */
        if (sscanf(line, "%d %c", &c, &extra) == 1 && c >= 1 && c <= 3)
            return c;
        printf("  Invalid choice. Please type 1, 2 or 3.\n");
    }
}

/* option 1: the full report with every section */
void detailed_report(double r[], int cls[], int n, double thr, int count[], const char *path) {
    if (IS_TTY()) {                          /* live progress bar (terminal only) */
        printf("\n");
        animate_tests(r, cls, n);
    }
    print_setup(path, r, n, thr);
    print_table(r, cls, n, thr);
    print_ledger(r, cls, n);
    print_summary(n, count);
    print_class_chart(count, n);
    print_trace(r, cls, n, thr);
    print_histogram(r, n, thr);
    print_statistics(r, n, thr);
    print_dynamics(r, cls, n, thr);
    print_verdict(count[ABOVE] + count[EQUAL], n);
}

/* option 2: short report - setup, pass/fail summary and final verdict */
void summary_report(double r[], int n, double thr, int count[], const char *path) {
    print_setup(path, r, n, thr);
    print_summary(n, count);
    print_verdict(count[ABOVE] + count[EQUAL], n);
}

/* ---- small helpers ---- */

/* returns the colour code, or "" when colours are off */
const char *paint(const char *code) {
    if (use_color)
        return code;
    return "";
}

/* wait for ms milliseconds (used by the progress bar) */
void pause_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)ms * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* print the text s, n times */
void repeat(const char *s, int n) {
    int i;
    for (i = 0; i < n; i++)
        printf("%s", s);
}

/* draw a horizontal table line, e.g. ┌────┬────┐ */
void draw_line(const char *left, const char *mid, const char *right, const int w[], int cols) {
    int i;
    printf("%s", left);
    for (i = 0; i < cols; i++) {
        repeat("─", w[i]);
        if (i < cols - 1)
            printf("%s", mid);
        else
            printf("%s", right);
    }
    printf("\n");
}

/* a title inside a double-line box, centred */
void print_title(const char *text, const char *color) {
    int len = strlen(text);
    int left = (INNER_W - len) / 2;
    int right = INNER_W - len - left;

    printf("╔"); repeat("═", INNER_W); printf("╗\n");
    printf("║%*s%s%s%s%*s║\n", left, "", paint(color), text, paint(RESET), right, "");
    printf("╚"); repeat("═", INNER_W); printf("╝\n");
}

/* one row of a 2 column table: label on the left, value on the right */
void print_row(const char *label, const char *value, int label_w, int value_w, const char *color) {
    printf("│ %-*s │ %s%*s%s │\n", label_w - 2, label, paint(color), value_w - 2, value, paint(RESET));
}


/* ===================== PART 2 (Aarya): INPUT ===================== */

/* ask for the data file name. Pressing Enter keeps the default file name.
   Quotes around the name (added when a file is dragged into the console) are removed. */
void read_filename(char *name, int size) {
    char line[260];
    char *p;
    int len;

    printf("Enter data file name (press Enter for %s): ", DATA_FILE);
    if (fgets(line, sizeof line, stdin) == NULL) {
        printf("\nInput ended. Exiting.\n");
        exit(1);
    }

    p = line;
    while (isspace((unsigned char)*p))               /* skip leading spaces        */
        p++;
    len = (int)strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1]))
        p[--len] = '\0';                             /* cut trailing spaces/newline */

    if (len >= 2 && p[0] == '"' && p[len - 1] == '"') {
        p[len - 1] = '\0';                           /* remove the two quotes      */
        p++;
        len -= 2;
    }

    if (len == 0)
        snprintf(name, size, "%s", DATA_FILE);       /* Enter alone: use default   */
    else
        snprintf(name, size, "%s", p);
}

/* ask for the threshold until a valid number is typed */
double read_threshold(void) {
    char line[256];
    char extra;
    double value;

    while (1) {
        printf("Enter operating threshold (-120 to 0 dBm): ");
        if (fgets(line, sizeof line, stdin) == NULL) {
            printf("\nInput ended. Exiting.\n");
            exit(1);
        }
        /* must be exactly one number and nothing else on the line */
        if (sscanf(line, "%lf %c", &value, &extra) != 1)
            printf("  Invalid value. Please type a number.\n");
        else if (!(value >= MIN_DBM && value <= MAX_DBM))
            printf("  Invalid value: %.2f dBm is outside %.0f to %.0f dBm. Try again.\n",
                   value, MIN_DBM, MAX_DBM);
        else
            return value;
    }
}

/* read ALL readings from the file (one number per line, up to max).
   Returns how many were read (1 or more), or -1 on error.
   Blank lines and lines starting with # are skipped. */
int load_readings(const char *path, double r[], int max) {
    FILE *fp = fopen(path, "r");
    char line[256];
    char extra;
    int count = 0, ignored = 0, lineno = 0;
    double v;

    if (fp == NULL) {
        printf("Error: cannot open data file \"%s\".\n", path);
        printf("Check the name, or put the file in the folder you run the program from.\n");
        return -1;
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        lineno++;
        if (line[0] == '#' || strspn(line, " \t\r\n") == strlen(line))
            continue;                              /* comment or blank line */

        /* must be exactly one number and nothing else on the line */
        if (sscanf(line, "%lf %c", &v, &extra) != 1) {
            printf("Error: %s line %d is not a valid number.\n", path, lineno);
            fclose(fp);
            return -1;
        }
        if (!(v >= MIN_DBM && v <= MAX_DBM)) {
            printf("Error: %s line %d: %.2f dBm is outside %.0f to %.0f dBm.\n",
                   path, lineno, v, MIN_DBM, MAX_DBM);
            fclose(fp);
            return -1;
        }
        if (count < max)
            r[count++] = v;
        else
            ignored++;
    }
    fclose(fp);

    if (count == 0) {
        printf("Error: %s contains no readings.\n", path);
        return -1;
    }
    if (ignored > 0)
        printf("Warning: only the first %d readings are used; %d extra reading(s) were ignored.\n",
               max, ignored);
    return count;
}


/* ============ PART 3 (Ronak): ANALYSIS HELPERS, SETUP TABLE, LEDGER ============ */

/* decide if a reading is ABOVE, EQUAL or BELOW the threshold.
   We never use == on decimals, so a small tolerance is used. */
int classify(double reading, double threshold) {
    double diff = reading - threshold;
    if (diff > EPSILON)
        return ABOVE;
    if (diff < -EPSILON)
        return BELOW;
    return EQUAL;
}

/* simple bubble sort, smallest first */
void sort_array(double a[], int n) {
    int i, j;
    double temp;
    for (i = 0; i < n - 1; i++)
        for (j = 0; j < n - 1 - i; j++)
            if (a[j] > a[j + 1]) {
                temp = a[j];
                a[j] = a[j + 1];
                a[j + 1] = temp;
            }
}

/* percentile of a SORTED array, with linear interpolation */
double percentile(double sorted[], int n, double p) {
    double pos = p / 100.0 * (n - 1);
    int lo = (int)floor(pos);
    int hi = (int)ceil(pos);
    return sorted[lo] + (sorted[hi] - sorted[lo]) * (pos - lo);
}

/* Student t value for a 95% two-sided interval with df degrees of freedom.
   Small df use the standard table; large df use a series that starts at Z_95.
   (For df = 49 this gives 2.0096, the value the 50-reading version used.) */
double t_critical_95(int df) {
    double table[30] = {12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
                         2.201, 2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086,
                         2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
    double z = Z_95, z3, z5, z7;

    if (df < 1)
        return 0.0;
    if (df <= 30)
        return table[df - 1];

    z3 = z * z * z;
    z5 = z3 * z * z;
    z7 = z5 * z * z;
    return z + (z3 + z) / (4.0 * df)
             + (5.0 * z5 + 16.0 * z3 + 3.0 * z) / (96.0 * df * df)
             + (3.0 * z7 + 19.0 * z5 + 17.0 * z3 - 15.0 * z) / (384.0 * df * df * df);
}

/* grade and its colour from a percentage */
const char *grade_of(double pct) {
    if (pct >= RELIABLE_PCT) return "RELIABLE";
    if (pct >= MARGINAL_PCT) return "MARGINAL";
    return "UNRELIABLE";
}

const char *grade_color(double pct) {
    if (pct >= RELIABLE_PCT) return GREEN;
    if (pct >= MARGINAL_PCT) return YELLOW;
    return RED;
}

/* TEST SETUP table */
void print_setup(const char *path, double r[], int n, double thr) {
    int w[2] = {30, 40};
    double mn = r[0], mx = r[0];
    char val[64];
    int i;

    for (i = 1; i < n; i++) {
        if (r[i] < mn) mn = r[i];
        if (r[i] > mx) mx = r[i];
    }

    printf("\n");
    print_title("TEST SETUP", CYAN);
    draw_line("┌", "┬", "┐", w, 2);
    print_row("Data file", path, w[0], w[1], "");
    sprintf(val, "%d", n);
    print_row("Test cases loaded", val, w[0], w[1], "");
    sprintf(val, "%.1f to %.1f dBm", mn, mx);
    print_row("Reading range in file", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", thr);
    print_row("Operating threshold (input)", val, w[0], w[1], CYAN);
    draw_line("└", "┴", "┘", w, 2);
}

/* B. vertical ledger: readings stacked in one column per class */
void print_ledger(double r[], int cls[], int n) {
    int w[3] = {23, 23, 23};
    int list[3][MAX_READINGS];   /* list[class][k] = index of the k-th reading in that class */
    int cnt[3] = {0, 0, 0};
    int i, k, row, maxrows = 0;
    char cell[48];

    for (i = 0; i < n; i++) {
        list[cls[i]][cnt[cls[i]]] = i;
        cnt[cls[i]]++;
    }
    for (k = 0; k < 3; k++)
        if (cnt[k] > maxrows)
            maxrows = cnt[k];

    printf("\n");
    print_title("B. VERTICAL VIEW - LEDGER STACKED BY CLASS", CYAN);
    draw_line("┌", "┬", "┐", w, 3);
    for (k = 0; k < 3; k++) {
        sprintf(cell, "%s (%d)", CLASS_SHORT[k], cnt[k]);
        printf("│ %s%-21s%s ", paint(CLASS_COLOR[k]), cell, paint(RESET));
    }
    printf("│\n");
    draw_line("├", "┼", "┤", w, 3);

    for (row = 0; row < maxrows; row++) {
        for (k = 0; k < 3; k++) {
            if (row < cnt[k]) {
                sprintf(cell, "#%-2d %8.2f dBm", list[k][row] + 1, r[list[k][row]]);
                printf("│ %s%-21s%s ", paint(CLASS_COLOR[k]), cell, paint(RESET));
            } else {
                printf("│ %-21s ", "");
            }
        }
        printf("│\n");
    }
    draw_line("└", "┴", "┘", w, 3);
}


/* ============ PART 4 (Ishan): PROGRESS BAR, SECTION A TABLE, SUMMARY ============ */

/* live progress bar while the tests "run" (only on a terminal).
   The pause shrinks when n is large, so the whole animation lasts about 1.25 seconds. */
void animate_tests(double r[], int cls[], int n) {
    int i, filled;
    int delay = 1250 / n;

    if (delay < 1)
        delay = 1;

    for (i = 1; i <= n; i++) {
        filled = i * 30 / n;
        printf("\r %sRunning test %2d/%d%s │", paint(BOLD), i, n, paint(RESET));
        printf("%s", paint(CYAN));
        repeat("█", filled);
        printf("%s", paint(RESET));
        printf("%*s│ %3d%% %s%8.2f dBm  %s%s",
               30 - filled, "", i * 100 / n,
               paint(CLASS_COLOR[cls[i - 1]]), r[i - 1],
               cls[i - 1] == BELOW ? "FAIL" : "PASS", paint(RESET));
        fflush(stdout);
        pause_ms(delay);
    }
    printf("\n");
}

/* A. horizontal table: one row per test case with a strength bar */
void print_table(double r[], int cls[], int n, double thr) {
    int w[5] = {6, 14, 12, 20, 16};
    double lo = r[0], hi = r[0];
    int i, len;

    for (i = 1; i < n; i++) {
        if (r[i] < lo) lo = r[i];
        if (r[i] > hi) hi = r[i];
    }

    printf("\n");
    print_title("A. HORIZONTAL VIEW - ALL TEST CASES", CYAN);
    draw_line("┌", "┬", "┐", w, 5);
    printf("│ %-4s │ %-12s │ %-10s │ %-18s │ %-14s │\n",
           "No.", "Reading(dBm)", "Margin(dB)", "Classification", "Strength");
    draw_line("├", "┼", "┤", w, 5);

    for (i = 0; i < n; i++) {
        /* weakest reading gets 1 block, strongest gets 14 blocks */
        if (hi > lo)
            len = 1 + (int)floor((r[i] - lo) / (hi - lo) * 13.0 + 0.5);
        else
            len = 14;

        printf("│ %4d │ %12.2f │ %+10.2f │ %s%-18s%s │ %s",
               i + 1, r[i], r[i] - thr,
               paint(CLASS_COLOR[cls[i]]), CLASS_LABEL[cls[i]], paint(RESET),
               paint(CLASS_COLOR[cls[i]]));
        repeat("█", len);
        printf("%s%*s │\n", paint(RESET), 14 - len, "");
    }
    draw_line("└", "┴", "┘", w, 5);
}

/* SUMMARY: counts, percentages, PASS / FAIL */
void print_summary(int n, int count[]) {
    int w[3] = {30, 20, 19};
    int k, pass = count[ABOVE] + count[EQUAL];

    printf("\n");
    print_title("SUMMARY", CYAN);
    draw_line("┌", "┬", "┐", w, 3);
    printf("│ %-28s │ %18s │ %17s │\n", "Category", "Count", "Percentage");
    draw_line("├", "┼", "┤", w, 3);
    for (k = 0; k < 3; k++)
        printf("│ %s%-28s%s │ %18d │ %16.2f%% │\n",
               paint(CLASS_COLOR[k]), CLASS_LABEL[k], paint(RESET), count[k], 100.0 * count[k] / n);
    draw_line("├", "┼", "┤", w, 3);
    printf("│ %-28s │ %18d │ %16.2f%% │\n", "TOTAL", n, 100.0);
    draw_line("├", "┼", "┤", w, 3);
    printf("│ %s%-28s%s │ %18d │ %16.2f%% │\n",
           paint(GREEN), "PASS (reading >= threshold)", paint(RESET), pass, 100.0 * pass / n);
    printf("│ %s%-28s%s │ %18d │ %16.2f%% │\n",
           paint(RED), "FAIL (reading <  threshold)", paint(RESET), n - pass, 100.0 * (n - pass) / n);
    draw_line("└", "┴", "┘", w, 3);
}


/* ============ PART 5 (Manini): SECTION C BAR CHART, SECTION E HISTOGRAM ============ */

/* C. vertical bar chart of the class distribution */
void print_class_chart(int count[], int n) {
    int cell = 22, bar_w = 10;
    int level[3];
    char label[32];
    int k, row, pad;

    /* each level = 10 percent */
    for (k = 0; k < 3; k++) {
        level[k] = (int)(100.0 * count[k] / n / 10.0 + 0.5);
        if (count[k] > 0 && level[k] == 0)
            level[k] = 1;
    }

    printf("\n");
    print_title("C. VERTICAL BAR CHART - SHARE OF TEST CASES", CYAN);
    printf("\n");

    for (row = CHART_HEIGHT; row >= 1; row--) {
        printf(" %3d%% ┤", row * 10);
        for (k = 0; k < 3; k++) {
            pad = (cell - bar_w) / 2;
            printf("%*s", pad, "");
            if (level[k] >= row) {
                printf("%s", paint(CLASS_COLOR[k]));
                repeat("█", bar_w);
                printf("%s", paint(RESET));
            } else {
                printf("%*s", bar_w, "");
            }
            printf("%*s", cell - bar_w - pad, "");
        }
        printf("\n");
    }
    printf("    0%% └");
    repeat("─", 3 * cell);
    printf("\n");

    /* class names under the bars */
    printf("       ");
    for (k = 0; k < 3; k++) {
        pad = (cell - (int)strlen(CLASS_SHORT[k])) / 2;
        printf("%*s%s%s%s%*s", pad, "", paint(CLASS_COLOR[k]), CLASS_SHORT[k], paint(RESET),
               cell - pad - (int)strlen(CLASS_SHORT[k]), "");
    }
    /* counts and percentages */
    printf("\n       ");
    for (k = 0; k < 3; k++) {
        sprintf(label, "%d (%.1f%%)", count[k], 100.0 * count[k] / n);
        pad = (cell - (int)strlen(label)) / 2;
        printf("%*s%s%*s", pad, "", label, cell - pad - (int)strlen(label), "");
    }
    printf("\n");
}

/* E. histogram with 10 dB bins, threshold bin is marked */
void print_histogram(double r[], int n, double thr) {
    int w[3] = {20, 9, 40};
    int bins[16];
    int nb, b, i, maxc = 0, len;
    double lo = r[0], hi = r[0], b_lo;
    char label[32];
    const char *col;

    for (i = 1; i < n; i++) {
        if (r[i] < lo) lo = r[i];
        if (r[i] > hi) hi = r[i];
    }
    lo = floor(lo / 10.0) * 10.0;
    hi = ceil(hi / 10.0) * 10.0;
    if (hi - lo < 10.0)
        hi = lo + 10.0;
    nb = (int)((hi - lo) / 10.0 + 0.5);     /* number of bins */
    if (nb > 16) nb = 16;

    for (b = 0; b < 16; b++)
        bins[b] = 0;
    for (i = 0; i < n; i++) {
        b = (int)floor((r[i] - lo) / 10.0);
        if (b >= nb) b = nb - 1;            /* top edge belongs to the last bin */
        if (b < 0) b = 0;
        bins[b]++;
    }
    for (b = 0; b < nb; b++)
        if (bins[b] > maxc)
            maxc = bins[b];

    printf("\n");
    print_title("E. HISTOGRAM - 10 dB BINS", CYAN);
    draw_line("┌", "┬", "┐", w, 3);
    printf("│ %-18s │ %7s │ %-38s │\n", "Range (dBm)", "Count", "Distribution");
    draw_line("├", "┼", "┤", w, 3);

    for (b = nb - 1; b >= 0; b--) {         /* strongest bin on top */
        b_lo = lo + 10.0 * b;
        sprintf(label, "%4.0f to %4.0f", b_lo, b_lo + 10.0);
        len = 0;
        if (maxc > 0)
            len = (int)floor(30.0 * bins[b] / maxc + 0.5);
        if (bins[b] > 0 && len == 0)
            len = 1;

        /* red = bin fully below threshold, green = fully above, yellow = holds it */
        if (b_lo + 10.0 <= thr + EPSILON)
            col = RED;
        else if (b_lo >= thr - EPSILON)
            col = GREEN;
        else
            col = YELLOW;

        printf("│ %-18s │ %7d │ %s", label, bins[b], paint(col));
        repeat("█", len);
        printf("%s", paint(RESET));
        if (thr >= b_lo - EPSILON && thr < b_lo + 10.0 - EPSILON)
            printf(" %s◄ thr%s%*s", paint(CYAN), paint(RESET), 38 - len - 6, "");
        else
            printf("%*s", 38 - len, "");
        printf(" │\n");
    }
    draw_line("└", "┴", "┘", w, 3);
}


/* ============ PART 6 (Megha): SECTION D SIGNAL TRACE, STATISTICS ============ */

/* D. vertical signal trace: one column per test case, threshold as a solid line.
   When there are more than TRACE_COLS readings, g readings share one column and the
   average of the group is drawn, so the chart always fits on the screen. */
void print_trace(double r[], int cls[], int n, double thr) {
    double cv[TRACE_COLS];              /* value drawn in each column */
    int    cc[TRACE_COLS];              /* class (colour) of each column */
    double lo, hi, step, centre, sum;
    char axis[TRACE_COLS + 8], tmp[8];
    int g, cols, c, i, cnt, row, thr_row, len, start, last;

    g = (n + TRACE_COLS - 1) / TRACE_COLS;      /* readings per column (1 when n <= 60) */
    cols = (n + g - 1) / g;                     /* number of columns drawn */

    for (c = 0; c < cols; c++) {
        sum = 0.0;
        cnt = 0;
        for (i = c * g; i < n && i < (c + 1) * g; i++) {
            sum += r[i];
            cnt++;
        }
        cv[c] = sum / cnt;
        cc[c] = (g == 1) ? cls[c] : classify(cv[c], thr);
    }

    lo = cv[0];
    hi = cv[0];
    for (c = 1; c < cols; c++) {
        if (cv[c] < lo) lo = cv[c];
        if (cv[c] > hi) hi = cv[c];
    }
    if (thr < lo) lo = thr;
    if (thr > hi) hi = thr;
    lo = floor(lo / 10.0) * 10.0;       /* round the chart limits to 10 dB */
    hi = ceil(hi / 10.0) * 10.0;
    if (hi - lo < 10.0)
        hi = lo + 10.0;

    step = (hi - lo) / TRACE_HEIGHT;    /* dB per chart row */
    thr_row = (int)floor((thr - lo) / step) + 1;
    if (thr_row < 1) thr_row = 1;
    if (thr_row > TRACE_HEIGHT) thr_row = TRACE_HEIGHT;

    printf("\n");
    print_title("D. VERTICAL SIGNAL TRACE vs THRESHOLD", CYAN);
    printf("\n   dBm\n");

    for (row = TRACE_HEIGHT; row >= 1; row--) {
        centre = lo + step * (row - 0.5);

        if (row % 2 == 0)
            printf(" %5.0f ┤", lo + step * row);
        else
            printf("       ┤");

        for (c = 0; c < cols; c++) {
            if (cv[c] >= centre)
                printf("%s█%s", paint(CLASS_COLOR[cc[c]]), paint(RESET));
            else if (row == thr_row)
                printf("%s─%s", paint(CYAN), paint(RESET));
            else
                printf(" ");
        }
        if (row == thr_row)
            printf(" %s◄ THR %.0f dBm%s", paint(CYAN), thr, paint(RESET));
        printf("\n");
    }

    /* bottom axis with a tick every 10 columns */
    printf("       └");
    for (c = 1; c <= cols; c++) {
        if (c % 10 == 0)
            printf("┴");
        else
            printf("─");
    }
    printf("\n");

    /* test case numbers under the axis (column 1 and every 10th column) */
    memset(axis, ' ', sizeof axis);
    axis[cols] = '\0';
    for (c = 1; c <= cols; c++) {
        if (c == 1 || c % 10 == 0) {
            last = c * g;                       /* last test case in this column */
            if (last > n) last = n;
            len = sprintf(tmp, "%d", last);
            start = c - len;
            if (start < 0) start = 0;
            memcpy(axis + start, tmp, len);
        }
    }
    printf("        %s   test case no.\n\n", axis);
    if (g > 1)
        printf("        Each column = %d readings (the average is drawn).\n", g);
    printf("        %s█%s ABOVE   %s█%s EQUAL   %s█%s BELOW   %s─%s threshold\n",
           paint(GREEN), paint(RESET), paint(YELLOW), paint(RESET),
           paint(RED), paint(RESET), paint(CYAN), paint(RESET));
}

/* STATISTICS of the readings (works for any n; n = 1 has no spread) */
void print_statistics(double r[], int n, double thr) {
    int w[2] = {40, 30};
    double sorted[MAX_READINGS];
    double sum = 0, sq = 0, mw = 0;
    double mean, stddev = 0.0, sem = 0.0, ci_lo = 0.0, ci_hi = 0.0, power_mean;
    char val[64], label[64], title[64];
    int i;

    /* sorted copy, so the original order is kept for the other sections */
    for (i = 0; i < n; i++)
        sorted[i] = r[i];
    sort_array(sorted, n);

    for (i = 0; i < n; i++) {
        sum += r[i];
        mw += pow(10.0, r[i] / 10.0);       /* dBm to milliwatts */
    }
    mean = sum / n;
    power_mean = 10.0 * log10(mw / n);      /* average power, back to dBm */

    if (n >= 2) {
        for (i = 0; i < n; i++)
            sq += (r[i] - mean) * (r[i] - mean);
        stddev = sqrt(sq / (n - 1));        /* sample standard deviation */
        sem = stddev / sqrt((double)n);     /* standard error of the mean */
        ci_lo = mean - t_critical_95(n - 1) * sem;
        ci_hi = mean + t_critical_95(n - 1) * sem;
    }

    printf("\n");
    sprintf(title, "STATISTICS OF THE %d READING%s", n, n == 1 ? "" : "S");
    print_title(title, CYAN);
    draw_line("┌", "┬", "┐", w, 2);
    sprintf(val, "%.2f dBm", sorted[0]);                  print_row("Minimum", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", percentile(sorted, n, 5));   print_row("5th percentile", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", percentile(sorted, n, 25));  print_row("25th percentile (Q1)", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", percentile(sorted, n, 50));  print_row("Median", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", percentile(sorted, n, 75));  print_row("75th percentile (Q3)", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", percentile(sorted, n, 95));  print_row("95th percentile", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", sorted[n - 1]);              print_row("Maximum", val, w[0], w[1], "");
    draw_line("├", "┼", "┤", w, 2);
    sprintf(val, "%.2f dBm", mean);                       print_row("Mean (arithmetic, in dBm)", val, w[0], w[1], "");
    sprintf(val, "%.2f dBm", power_mean);                 print_row("Mean power (linear domain)", val, w[0], w[1], "");
    if (n >= 2) {
        sprintf(val, "%.2f dB", stddev);                  print_row("Std. deviation (sample, N-1)", val, w[0], w[1], "");
        sprintf(val, "%.2f dB", sem);                     print_row("Standard error of the mean", val, w[0], w[1], "");
        sprintf(label, "95%% CI of the mean (t, df=%d) dBm", n - 1);
        sprintf(val, "%.2f to %.2f", ci_lo, ci_hi);       print_row(label, val, w[0], w[1], "");
    } else {
        print_row("Std. deviation (sample, N-1)", "n/a (needs 2+ readings)", w[0], w[1], "");
        print_row("Standard error of the mean", "n/a (needs 2+ readings)", w[0], w[1], "");
        print_row("95% CI of the mean (t) dBm", "n/a (needs 2+ readings)", w[0], w[1], "");
    }
    draw_line("├", "┼", "┤", w, 2);
    sprintf(val, "%+.2f dB", mean - thr);                 print_row("Mean link margin", val, w[0], w[1], "");
    sprintf(val, "%+.2f dB", sorted[0] - thr);            print_row("Worst-case margin", val, w[0], w[1], "");
    draw_line("└", "┴", "┘", w, 2);
}


/* ============ PART 7 (Saumil): LINK DYNAMICS, FINAL VERDICT ============ */

/* LINK DYNAMICS: streaks and outage events */
void print_dynamics(double r[], int cls[], int n, double thr) {
    int w[2] = {40, 30};
    int run_ok = 0, run_out = 0, below = 0;
    int longest_ok = 0, longest_ok_start = 0;
    int longest_out = 0, longest_out_start = 0;
    int outage_events = 0, first_fail = 0;
    int deepest_idx = 1, best_idx = 1;
    double deepest = r[0] - thr, best = r[0] - thr, margin, mean_outage;
    char val[64];
    int i;

    for (i = 0; i < n; i++) {
        margin = r[i] - thr;
        if (margin < deepest) { deepest = margin; deepest_idx = i + 1; }
        if (margin > best)    { best = margin;    best_idx = i + 1; }

        if (cls[i] == BELOW) {              /* a FAIL: outage continues */
            below++;
            run_ok = 0;
            run_out++;
            if (run_out == 1)
                outage_events++;            /* a new outage just started */
            if (first_fail == 0)
                first_fail = i + 1;
            if (run_out > longest_out) {
                longest_out = run_out;
                longest_out_start = i - run_out + 2;
            }
        } else {                            /* a PASS: pass streak continues */
            run_out = 0;
            run_ok++;
            if (run_ok > longest_ok) {
                longest_ok = run_ok;
                longest_ok_start = i - run_ok + 2;
            }
        }
    }
    mean_outage = 0.0;
    if (outage_events > 0)
        mean_outage = (double)below / outage_events;

    printf("\n");
    print_title("LINK DYNAMICS - STREAKS AND OUTAGES", CYAN);
    draw_line("┌", "┬", "┐", w, 2);

    if (longest_ok > 0)
        sprintf(val, "%d tests (#%d to #%d)", longest_ok, longest_ok_start, longest_ok_start + longest_ok - 1);
    else
        strcpy(val, "none");
    print_row("Longest PASS streak", val, w[0], w[1], GREEN);

    if (longest_out > 0)
        sprintf(val, "%d tests (#%d to #%d)", longest_out, longest_out_start, longest_out_start + longest_out - 1);
    else
        strcpy(val, "none");
    print_row("Longest outage (FAIL streak)", val, w[0], w[1], RED);

    sprintf(val, "%d", outage_events);
    print_row("Number of outage events", val, w[0], w[1], "");
    sprintf(val, "%.2f tests", mean_outage);
    print_row("Mean outage length", val, w[0], w[1], "");
    if (first_fail > 0)
        sprintf(val, "test #%d", first_fail);
    else
        strcpy(val, "none");
    print_row("First failure at", val, w[0], w[1], "");

    draw_line("├", "┼", "┤", w, 2);
    sprintf(val, "%+.2f dB (test #%d)", best, best_idx);
    print_row("Best margin", val, w[0], w[1], "");
    sprintf(val, "%+.2f dB (test #%d)", deepest, deepest_idx);
    print_row("Deepest fade (worst margin)", val, w[0], w[1], "");
    draw_line("└", "┴", "┘", w, 2);
}

/* FINAL VERDICT: availability, Wilson 95% interval, grade and gauge */
void print_verdict(int pass, int n) {
    int w[2] = {40, 30};
    double p = (double)pass / n;
    double z2 = Z_95 * Z_95;
    double denom = 1.0 + z2 / n;
    double centre = (p + z2 / (2.0 * n)) / denom;
    double half = Z_95 * sqrt(p * (1.0 - p) / n + z2 / (4.0 * n * n)) / denom;
    double w_lo = 100.0 * (centre - half);
    double w_hi = 100.0 * (centre + half);
    double avail = 100.0 * p;
    char val[64], title[64], mark[64], lab[64];
    int filled;

    if (w_lo < 0.0)   w_lo = 0.0;
    if (w_hi > 100.0) w_hi = 100.0;

    sprintf(title, "FINAL VERDICT: LINK IS %s", grade_of(avail));
    printf("\n");
    print_title(title, grade_color(avail));

    draw_line("┌", "┬", "┐", w, 2);
    sprintf(val, "%d of %d tests", pass, n);
    print_row("Tests passed", val, w[0], w[1], "");
    sprintf(val, "%.2f %%", avail);
    print_row("Link availability (point estimate)", val, w[0], w[1], "");
    sprintf(val, "%.2f to %.2f %%", w_lo, w_hi);
    print_row("Wilson 95% interval of availability", val, w[0], w[1], "");
    print_row("Grade - point estimate", grade_of(avail), w[0], w[1], grade_color(avail));
    print_row("Grade - conservative (Wilson lower)", grade_of(w_lo), w[0], w[1], grade_color(w_lo));
    draw_line("└", "┴", "┘", w, 2);

    /* availability gauge: 50 cells = 100%, ^ marks the 80% and 95% limits */
    filled = (int)(avail / 100.0 * 50.0 + 0.5);
    printf("┌"); repeat("─", INNER_W); printf("┐\n");
    printf("│ %s", paint(grade_color(avail)));
    repeat("█", filled);
    printf("%s%*s %6.2f%%%*s│\n", paint(RESET), 50 - filled, "", avail, 12, "");

    memset(mark, ' ', sizeof mark);  mark[50] = '\0';
    memset(lab,  ' ', sizeof lab);   lab[50]  = '\0';
    mark[39] = '^';
    mark[47] = '^';
    memcpy(lab + 38, "80%", 3);
    memcpy(lab + 46, "95%", 3);
    printf("│ %-69s │\n", mark);
    printf("│ %-69s │\n", lab);
    printf("└"); repeat("─", INNER_W); printf("┘\n");
}
