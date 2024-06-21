// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#ifndef SMOLMATH_SMOLMATH_H
#define SMOLMATH_SMOLMATH_H

#ifdef __cplusplus
extern "C" {
#endif

struct exp_t;

struct op_t {
    char op;
    int precedence;
    struct exp_t *left;
    struct exp_t *right;
};

#define ISNUM(T) ((T) == L || (T) == F)
struct val_t {
    enum {
        L, F, N, S
    } type;
    char suffix;
    union {
        long l;
        double f;
        char *ns;
    };
};

struct exp_t {
    enum {
        V, OP
    } type;
    char data[];
};

typedef void (*smolmath_cb)(struct exp_t *exp);

void smolmath_log(struct exp_t *exp);

int smolmath_parse(char *line, const char *precedence, char minus, const char *alnum, smolmath_cb cb);

#ifdef __cplusplus
};
#endif

#endif //SMOLMATH_SMOLMATH_H
