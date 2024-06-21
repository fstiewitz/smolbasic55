// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<ctype.h>
#include<string.h>

static char line_buffer[128];
static char* line_buffer_ptr = 0;
long INPUT__reset = 0;

void INPUT__start() {
    INPUT__reset = 0;
    while(0 == fgets(line_buffer, sizeof(line_buffer), stdin)) {
        if(feof(stdin)) {
            fprintf(stderr, "could not read INPUT\n");
            exit(EXIT_FAILURE);
        }
    }
    line_buffer_ptr = line_buffer;
}

void INPUT__end() {}


#define RETURN(f, i) do {INPUT__reset=i; return f;}while(0)

double INPUT__numberd() {
    INPUT__reset = 0;
    double f = 0;
    assert(line_buffer_ptr);
    if(*line_buffer_ptr == 0) {
        fprintf(stderr, "INSUFFICIENT DATA, PLEASE ENTER AGAIN\n");
        RETURN(f, 1);
    }
    while(*line_buffer_ptr && isspace(*line_buffer_ptr)) ++line_buffer_ptr;
    char* c = strchr(line_buffer_ptr, ',');
    char* end;
    if(!c) {
        end = line_buffer_ptr + strlen(line_buffer_ptr);
    } else {
        end = c;
    }

    char oc = *end;
    *end = 0;
    char* e;
    f = strtod(line_buffer_ptr, &e);
    while(*e && isspace(*e)) ++e;
    if(end != e) {
        fprintf(stderr, "INVALID INPUT DATA, PLEASE ENTER AGAIN\n");
        RETURN(f, 1);
    }

    if(oc) {
        assert(oc == ',');
        line_buffer_ptr = end + 1;
    } else {
        line_buffer_ptr = end;
    }
    RETURN(f, 0);
}
