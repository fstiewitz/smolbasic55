// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#define _GNU_SOURCE
#include<string.h>
#include<stdio.h>
#include<stdlib.h>

double STRLEN__s(char* a) {
    return strlen(a);
}

void EXIT__s(char* s) {
    fprintf(stderr, "EXITING WITH REASON: %s\n", s);
    exit(EXIT_FAILURE);
}

const char* STRERRORNAME$__d(double i) {
    return strerrorname_np((int)i);
}

const char* STRERRORDESC$__d(double i) {
    return strerrordesc_np((int)i);
}
