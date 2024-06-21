// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<stdio.h>
#include<stdlib.h>

void GOSUB__err_overflow() {
    fprintf(stderr, "error: GOSUB stack overflow\n");
    exit(1);
}

void GOSUB__err_underflow() {
    fprintf(stderr, "error: GOSUB stack underflow\n");
    exit(1);
}

void ONGOTO__err_notfound(long i, long m, long line) {
    // fprintf(stderr, "%li: error: ON ... GOTO not matched (got: %li for range [1,%li])\n", line, i, m);
    fprintf(stderr, "%li: error: index out of range\n", line);
    exit(1);
}
