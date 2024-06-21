// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<stdio.h>
#include<stdlib.h>
#include<math.h>

struct dt {
    double n;
    char s[];
};

extern struct dt *DATA__begin;
extern struct dt *DATA__end;

static long ix = 0;

void RESTORE() {
    ix = 0;
}

void READ__numberd(double* f) {
    long count = &DATA__end - &DATA__begin;
    if(ix < 0 || ix >= count) {
        fprintf(stderr, "error: insufficient data for READ\n");
        exit(1);
    }
    struct dt *d = *(&DATA__begin + (ix++));
    double v = d->n;
    if(isnan(v)) {
        // fprintf(stderr, "error: reading string into numeric variable (%s)\n", d->s);
        fprintf(stderr, "error: reading string into numeric variable\n");
        exit(1);
    }
    *f = v;
}

void READ__string(char** c) {
    long count = &DATA__end - &DATA__begin;
    if(ix < 0 || ix >= count) {
        fprintf(stderr, "OUT OF NUMBER DATA\n");
        exit(1);
    }
    *c = (&DATA__begin)[ix++]->s;
}
