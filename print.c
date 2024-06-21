// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<assert.h>

static long c = 0;

#define D 6
#define E 2

#define LT 16
#define ST 4
#define LL (5 * LT)

void PRINT__nl() {
    c = 0;
    fputc('\n', stdout);
    fflush(stdout);
}

void PRINT__string(char* str) {
    if(!str) return;
    long s = strlen(str);
    if(c + s > LL) {
        fputc('\n', stdout);
        c = s;
    } else {
        c += s;
    }
    fputs(str, stdout);
    fflush(stdout);
}

static void PRINT__slurp_zeroes_e(char* buffer) {
    char* e = strchr(buffer, 'E');
    if(!e) return;
    char* b = e + 1;
    int i;
    int j = 1;
    if(*b == '+' || *b == '-') {
        ++b;
        ++j;
    }
    while(*b && *b == '0') ++b;
    if(*b && *(b - 1) == '0') {
        for(i = 0; i < (b - e); ++i) {
            *(e + j + i) = *(b + i);
        }
        *(e + j + i) = 0;
    }
    for (b = e - 1; b != buffer; --b) {
        if (*b != '0') break;
    }
    ++b;
    long d = e - b;
    if(d == 0) return;
    for(i = 0; i < strlen(e); ++i) {
        *(b + i) = *(e + i);
    }
    *(e + strlen(e) - d) = 0;
}

static void PRINT__slurp_zeroes(char* buffer) {
    char* e = strchr(buffer, '.');
    if(!e) return;
    for (char* b = buffer + strlen(buffer) - 2; b != buffer; --b) {
        if (*b == '0') {
            *b = ' ';
            *(b + 1) = 0;
        } else break;
    }
    e = buffer + strlen(buffer) - 2;
    if(*e == '.') {
        *e = ' ';
        *(e + 1) = 0;
    }
}

void PRINT__numberd(double f) {
    if(isinf(f)) {
        if(f < 0) PRINT__string("-INF ");
        else PRINT__string("INF ");
    } else if(isnan(f)) {
        PRINT__string("NAN ");
    } else if(f == 0) {
        PRINT__string(" 0 ");
    } else {
        char buffer[64];
        char *b = buffer;
        if (f < 0) {
            *(b++) = '-';
            f = -f;
        } else {
            *(b++) = ' ';
        }
        snprintf(b, 64 - (b - buffer), "%#.10E ", (double)f);
        char* d = strchr(b, '.');
        char* e = strchr(b, 'E');
        assert(d && e);
        long exp = atol(e + 1);
        if(exp < 0 && (e - d - 6) - exp <= D) {
            snprintf(b, 64 - (b - buffer), "%#6.6F", f);
            d = strchr(b, '.');
            memmove(b, d, strlen(b) - (d - b));
            PRINT__slurp_zeroes(buffer);
        } else if(exp >= 0 && exp + 1 <= D) {
            snprintf(b, 64 - (b - buffer), "%#6.6F", f);
            PRINT__slurp_zeroes(buffer);
        } else {
            snprintf(b, 64 - (b - buffer), "%#6.7E ", f);
            PRINT__slurp_zeroes_e(b);
        }

        PRINT__string(buffer);
    }
    fflush(stdout);
}

void PRINT__numberl(long l) {
    PRINT__numberd((double)l);
    fflush(stdout);
}


void PRINT__tab() {
    long d = LT;
    if(c % LT) {
        d = LT - (c % LT);
    }
    c += d;
    if(c >= LL) {
        fputc('\n', stdout);
        c = 0;
    } else {
        for (long i = 0; i < d; ++i) {
            fputc(' ', stdout);
        }
    }
    fflush(stdout);
}

void TAB__l(long t) {
    if(t <= 0) {
        fprintf(stderr, "warning: invalid TAB argument (%li)\n", t);
        t = 1;
    }
    --t;
    if(c > t) {
        fputc('\n', stdout);
        c = 0;
    }
    if(c < t) {
        for(long i = 0; i < (t - c); ++i) {
            fputc(' ', stdout);
        }
        c = t;
    }
}

void TAB__d(double d) {
    long t = round(d);
    if(d < 0) {
        fprintf(stderr, "warning: invalid TAB argument (%li)\n", t);
        t = 1;
    }
    if(isinf(d)) {
        fprintf(stderr, "warning: invalid TAB argument (%f)\n", d);
        t = 1;
    }
    if(isnan(d)) {
        fprintf(stderr, "warning: invalid TAB argument (%f)\n", d);
        t = 1;
    }
    if(t <= 0) {
        fprintf(stderr, "warning: invalid TAB argument (%li)\n", t);
        t = 1;
    }
    TAB__l(t);
}

double DBG_print(double a, double b, double c, double d) {
    fprintf(stdout, "DBG %f\t%f\t%f\t%f\n", a, b, c, d);
    return a;
}
