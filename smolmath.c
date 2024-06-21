// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include "smolmath.h"

#include <ctype.h>
#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <setjmp.h>

static char *trim(char *i) {
    while (*i && isspace(*i)) ++i;
    return i;
}

static jmp_buf err_jmp;

void smolmath_log(struct exp_t *root) {
    if (!root) return;
    else if (root->type == V) {
        struct val_t *v = (struct val_t *) root->data;
        switch (v->type) {
            case L:
                fprintf(stderr, "%li", v->l);
                break;
            case F:
                fprintf(stderr, "%G", v->f);
                break;
            case N:
                fprintf(stderr, "%s", v->ns);
                break;
            case S:
                fprintf(stderr, "\"%s\"", v->ns);
                break;
        }
    } else {
        assert(root->type == OP);
        struct op_t *o = (struct op_t *) root->data;
        if (o->op == '[') {
            smolmath_log(o->left);
            fprintf(stderr, "[");
            smolmath_log(o->right);
            fprintf(stderr, "]");
        } else if (o->op == '{') {
            fprintf(stderr, "(");
            smolmath_log(o->left);
            fprintf(stderr, " {");
            assert(o->right && o->right->type == OP);
            struct op_t *o1 = (struct op_t *) o->right->data;
            smolmath_log(o1->left);
            fprintf(stderr, "} ");
            smolmath_log(o1->right);
            fprintf(stderr, ")");
        } else {
            fprintf(stderr, "(");
            smolmath_log(o->left);
            fprintf(stderr, " %c ", o->op);
            smolmath_log(o->right);
            fprintf(stderr, ")");
        }
    }
}

static struct exp_t *insert(struct exp_t *root, struct exp_t *el, int force_right) {
    if (!root) {
        return el;
    } else if (root->type == V) {
        /*if (el->type != OP) {
            smolmath_log(root);
            fprintf(stderr, "\n");
            smolmath_log(el);
            fprintf(stderr, "\n");
        }*/
        if(el->type != OP) longjmp(err_jmp, 1);
        struct op_t *o = (struct op_t *) el->data;
        assert(!o->left);
        o->left = root;
        return el;
    }
    struct op_t *o = (struct op_t *) root->data;
    int p0 = o->precedence;
    int p1 = 1;
    if (el->type == OP) {
        p1 = ((struct op_t *) el->data)->precedence;
    }
    if (((p0 == 99 && p0 < p1) || (p0 != 99 && p0 <= p1)) && !force_right) {
        assert(el->type == OP);
        struct op_t *o1 = (struct op_t *) el->data;
        o1->left = root;
        return el;
    } else {
        o->right = insert(o->right, el, force_right);
    }
    return root;
}

int smolmath_parse(char *line, const char *precedence, char minus, const char *alnum, smolmath_cb cb) {
    struct exp_t *root = 0;
    if(setjmp(err_jmp) == 0) {
        line = trim(line);
        char *i = line;
        unsigned int val_count = 0;
        unsigned int op_count = 0;
        unsigned int nesting_level = 0;
        unsigned int ix_count = 0;
        unsigned int n_size = 0;

        int prev_was_value = 0;
        int prev_was_name = 0;
        while (*i) {
            i = trim(i);
            if (!*i) break;
            if (*i == minus && (!prev_was_value)) {
                if (isdigit(*(i + 1))) {
                    ++i;
                    goto digit;
                }
            }
            if (isdigit(*i) || *i == '.') {
                digit:
                ++val_count;
                while (*i && (isdigit(*i) || *i == '.')) ++i;
                if (*i == 'E') {
                    ++i;
                    if (*i == '+' || *i == '-') ++i;
                    while (*i && (isdigit(*i))) ++i;
                }
                prev_was_name = 0;
                if (prev_was_value) {
                    ++ix_count;
                }
                prev_was_value = 1;
                if(strchr(alnum, *i)) ++i;
                continue;
            } else if (*i == '(' || *i == '[' || *i == '{' || *i == ')' || *i == ']' || *i == '}') {
                if ((*i == '[' || *i == '{') || (*i == '(' && prev_was_name)) ++ix_count;
                if (*i == '{') ++ix_count;
                ++nesting_level;
                ++i;
                if (*i == ')') prev_was_value = 1;
                continue;
            } else if (isalpha(*i)) {
                ++val_count;
                ++n_size;
                while (*i && (isalnum(*i) || strchr(alnum, *i))) {
                    ++n_size;
                    ++i;
                }
                ++n_size;
                prev_was_name = 1;
                prev_was_value = 1;
                continue;
            } else if (*i == '"') {
                ++val_count;
                ++i;
                int esc = 0;
                while (*i) {
                    if (esc) {
                        ++n_size;
                        esc = 0;
                        ++i;
                        continue;
                    }
                    if (*i == '"') {
                        ++n_size;
                        break;
                    }
                    if (*i == '\\') {
                        esc = 1;
                    } else {
                        ++n_size;
                    }
                    ++i;
                }
                if (*i) ++i;
                prev_was_name = 0;
                prev_was_value = 0;
                continue;
            } else ++op_count;
            ++i;
            prev_was_name = 0;
            prev_was_value = 0;
        }

        char *nesting = 0;
        char *vals = 0;
        char *ops = 0;
        char *ixs = 0;
        char *sbuf = 0;
        if (nesting_level) nesting = alloca(nesting_level + nesting_level * sizeof(struct exp_t *));
        if (val_count) vals = alloca(val_count * (sizeof(struct exp_t) + sizeof(struct val_t)));
        if (op_count) ops = alloca(op_count * (sizeof(struct exp_t) + sizeof(struct op_t)));
        if (ix_count) ixs = alloca(ix_count * (sizeof(struct exp_t) + sizeof(struct op_t)));
        if (n_size) sbuf = alloca(n_size);
        val_count = 0;
        op_count = 0;
        nesting_level = 0;
        ix_count = 0;
        n_size = 0;

        i = line;
        struct exp_t *e = 0;
        int ppv;
        int invert = 0;
        prev_was_value = 0;
        while (*i) {
            i = trim(i);
            if (!*i) break;
            invert = 0;
            if (*i == minus && !prev_was_value) {
                if (isdigit(*(i + 1))) {
                    ++i;
                    ppv = prev_was_value;
                    invert = 1;
                    goto ndigit;
                }
            }
            ppv = prev_was_value;
            prev_was_value = isdigit(*i) || *i == '.' || isalpha(*i) || strchr(alnum, *i) || *i == ')';
            if (isdigit(*i) || *i == '.') {
                char *s;
                ndigit:
                s = i;
                int is_f = 0;
                while (*i && (isdigit(*i))) {
                    ++i;
                }
                if (*i == '.') {
                    is_f = 1;
                    ++i;
                    while (*i && (isdigit(*i))) {
                        ++i;
                    }
                }
                if (*i == 'E') {
                    if (is_f == 0) {
                        is_f = 1;
                    }
                    ++i;
                    if (*i == '+') ++i;
                    if (*i == '-') {
                        ++i;
                    }
                    while (*i && (isdigit(*i))) {
                        ++i;
                    }
                }
                e = (struct exp_t *) (vals + (val_count++) * (sizeof(struct exp_t) + sizeof(struct val_t)));
                e->type = V;
                struct val_t *val = (struct val_t *) e->data;
                if(*i && strchr(alnum, *i)) {
                    val->suffix = *i;
                } else {
                    val->suffix = ' ';
                }
                if (!is_f) {
                    char *end;
                    val->type = L;
                    val->l = strtol(s, &end, 10);
                    if (invert) val->l = -val->l;
                    if (end != i) {
                        fprintf(stderr, "conversion error\n");
                        return 1;
                    } else if (errno == ERANGE) {
                        fprintf(stderr, "warning: numeric constant overflow\n");
                        errno = 0;
                    }
                } else {
                    char *end;
                    val->type = F;
                    val->f = strtod(s, &end);
                    if (invert) val->f = -val->f;
                    if (end != i) {
                        fprintf(stderr, "conversion error\n");
                        return 1;
                    } else if (errno == ERANGE) {
                        fprintf(stderr, "warning: numeric constant overflow\n");
                        errno = 0;
                    }
                }
                if(val->suffix != ' ') ++i;
                if (ppv) {
                    struct exp_t *ix = (struct exp_t *) (ixs +
                                                         (ix_count++) * (sizeof(struct exp_t) + sizeof(struct op_t)));
                    ix->type = OP;
                    struct op_t *o = (struct op_t *) ix->data;
                    o->op = ',';
                    o->precedence = 2;
                    o->left = 0;
                    o->right = e;
                    e = ix;
                }
                root = insert(root, e, 0);
                prev_was_value = 1;
                continue;
            } else if (*i == '(' || *i == '[' || *i == '{') {
                char *frame = nesting + (nesting_level++) * (1 + sizeof(struct exp_t *));
                *frame = *i;
                if (*i == '(' && e && e->type == V && ((struct val_t *) e->data)->type == N) *frame = ':';
                *(struct exp_t **) (frame + 1) = root;
                root = 0;
            } else if (*i == ']' || *i == ')' || *i == '}') {
                char *frame;
                unnest:
                frame = nesting + (--nesting_level) * (1 + sizeof(struct exp_t *));
                if ((*i == ']' && *frame != '[') || (*i == '}' && *frame != '{') ||
                    (*i == ')' && (!(*frame == '(' || *frame == ':')))) {
                    printf("bracket mismatch: %c ... %c\n", *frame, *i);
                    return 1;
                }
                if (*i == '}') {
                    struct exp_t *ix = (struct exp_t *) (ixs +
                                                         (ix_count++) * (sizeof(struct exp_t) + sizeof(struct op_t)));
                    ix->type = OP;
                    struct op_t *o = (struct op_t *) ix->data;
                    o->op = '}';
                    o->precedence = 2;
                    o->left = root;
                    o->right = 0;
                    root = ix;

                    ix = (struct exp_t *) (ixs + (ix_count++) * (sizeof(struct exp_t) + sizeof(struct op_t)));
                    ix->type = OP;
                    o = (struct op_t *) ix->data;
                    o->op = '{';
                    o->precedence = 2;
                    o->left = 0;
                    o->right = root;
                    root = ix;

                    root = insert(*(struct exp_t **) (frame + 1), root, 0);
                } else if (*i == ']' || *frame == ':') {
                    struct exp_t *ix = (struct exp_t *) (ixs +
                                                         (ix_count++) * (sizeof(struct exp_t) + sizeof(struct op_t)));
                    ix->type = OP;
                    struct op_t *o = (struct op_t *) ix->data;
                    if (*i == ']') o->op = *i;
                    else o->op = *frame;
                    o->precedence = 2;
                    o->left = 0;
                    o->right = root;
                    root = ix;
                    root = insert(*(struct exp_t **) (frame + 1), root, 0);
                } else {
                    if (root && root->type == OP) {
                        ((struct op_t *) root->data)->precedence = 1;
                    }
                    root = insert(*(struct exp_t **) (frame + 1), root, 1);
                }
                e = 0;
            } else if (isalpha(*i)) {
                char *s = i;
                while (*i && (isalnum(*i) || strchr(alnum, *i))) ++i;
                e = (struct exp_t *) (vals + (val_count++) * (sizeof(struct exp_t) + sizeof(struct val_t)));
                e->type = V;
                struct val_t *val = (struct val_t *) e->data;
                val->type = N;
                if(strchr(alnum, *(i - 1))) val->suffix = *(i - 1);
                else val->suffix = ' ';
                val->ns = sbuf + n_size;
                n_size += (i - s) + 1;
                memcpy(val->ns, s, i - s);
                val->ns[i - s] = 0;
                root = insert(root, e, 0);
                continue;
            } else if (*i == '"') {
                ++i;
                int esc = 0;
                e = (struct exp_t *) (vals + (val_count++) * (sizeof(struct exp_t) + sizeof(struct val_t)));
                e->type = V;
                struct val_t *val = (struct val_t *) e->data;
                val->type = S;
                val->ns = sbuf + n_size;
                char *s = val->ns;
                while (*i) {
                    if (esc) {
                        *(s++) = *i;
                        esc = 0;
                        ++i;
                        continue;
                    }
                    if (*i == '"') break;
                    if (*i == '\\') {
                        esc = 1;
                    } else {
                        *(s++) = *i;
                    }
                    ++i;
                }
                *(s++) = 0;
                if (*i) ++i;
                n_size = s - sbuf;
                root = insert(root, e, 0);
                continue;
            } else {
                e = (struct exp_t *) (ops + (op_count++) * (sizeof(struct exp_t) + sizeof(struct op_t)));
                e->type = OP;
                struct op_t *o = (struct op_t *) e->data;
                o->op = *i;
                o->precedence = 3;
                char *c = strchr(precedence, *i);
                if (c) {
                    o->precedence = 3 + (int) (c - precedence) / 2;
                } else if (*i == ',' || *i == ';') {
                    o->precedence = 99;
                }
                o->left = 0;
                o->right = 0;
                root = insert(root, e, 0);
            }
            ++i;
        }

        if (nesting_level) {
            fprintf(stderr, "error: syntax error (unmatched parentheses)\n");
            return 1;
        }
    } else {
        return 1;
    }

    if (cb) {
        cb(root);
    }
    return 0;
}
