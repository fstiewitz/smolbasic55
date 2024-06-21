// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include <algorithm>
#include <cassert>
#include <cstring>
#include "asm.h"
#include "features.h"
#include "util.h"

static long sd;
static long tmp_count = 0;
static long max_tmp_count = 0;
static long loop_control_vars = 0;
static long max_loop_control_vars = 0;

long gosub_depth = 16;

void cast(eval_ret from, eval_ret to);

void reset_tmp_count(long v) {
    tmp_count = v;
}

stack_layout_t pushStack() {
    stack_layout_t st = {
            .sd = sd,
            .tmp_count = tmp_count,
            .max_tmp_count = max_tmp_count,
    };
    tmp_count = 0;
    max_tmp_count = 0;
    return st;
}

void popStack(stack_layout_t st) {
    tmp_count = st.tmp_count;
    max_tmp_count = st.max_tmp_count;
    sd = st.sd;
}

long get_tmp_count() {
    return tmp_count;
}

long get_max_tmp_count() {
    return max_tmp_count;
}

long add_tmp(type_t type) {
    long r = tmp_count;
    long s = 0;
    switch (type) {
        case CHAR:
            s = 1;
            break;
        case SHORT:
            s = 2;
            break;
        case FLOAT:
        case INT:
            s = 4;
            break;
        case PTR:
        case DOUBLE:
        case LONG:
            s = 8;
            break;
    }
    if (tmp_count % s) {
        tmp_count += s - (tmp_count % s);
    }
    r = tmp_count;
    tmp_count += s;
    max_tmp_count = std::max(max_tmp_count, tmp_count);
    return r;
}

long add_loop_vars() {
    long r = loop_control_vars;
    loop_control_vars += 16;
    max_loop_control_vars = std::max(max_loop_control_vars, loop_control_vars);
    return r;
}

void proc_end() {
    od << "\tld ra, " << std::to_string(sd - 8) << "(sp)" << std::endl;
    od << "\tld fp, " << std::to_string(sd - 16) << "(sp)" << std::endl;
    od << "\tld s1, " << std::to_string(sd - 24) << "(sp)" << std::endl;
    od << "\tld s2, " << std::to_string(sd - 32) << "(sp)" << std::endl;
    od << "\taddi sp, sp, " << std::to_string(sd) << std::endl;
    od << "\tret" << std::endl;
}

void proc_start(bool reserve_gosub) {
    od << "\taddi sp, sp, " << std::to_string(-sd) << std::endl;
    od << "\tsd ra, " << std::to_string(sd - 8) << "(sp)" << std::endl;
    od << "\tsd fp, " << std::to_string(sd - 16) << "(sp)" << std::endl;
    od << "\tsd s1, " << std::to_string(sd - 24) << "(sp)" << std::endl;
    od << "\tsd s2, " << std::to_string(sd - 32) << "(sp)" << std::endl;
    od << "\taddi s1, sp, " << std::to_string(sd - 32 - (reserve_gosub ? gosub_depth * 8 : 0)) << std::endl;
    od << "\tli s2, 0" << std::endl;
}

void proc_main_start() {
    od << ".section .text" << std::endl;
    od << "main:" << std::endl;
    od << ".global main" << std::endl;
    sd = 8 + 4 * 8 + gosub_depth * 8 + max_tmp_count + max_loop_control_vars;
    if (sd % 16) {
        sd += 8;
    }
    proc_start();
}

void proc_main_end(long r) {
    od << "\tli a0, " << std::to_string(r) << std::endl;
    proc_end();
}

void
asm_data(const std::map<std::string, std::pair<long, long>> &varDims, const std::map<long, std::string> &stringMap,
         const std::vector<std::pair<double, std::string>> &dataItems) {
    od << ".section .data" << std::endl;
    for (auto &[n, d]: varDims) {
        long s;
        switch (eval_ret_from_suffix(n.back())) {
            case NUMBERC:
                s = 1;
                break;
            case NUMBERS:
                s = 2;
                break;
            case NUMBERI:
                s = 4;
                break;
            case NUMBERL:
                s = 8;
                break;
            case NUMBERF:
                s = 4;
                break;
            case NUMBERD:
            case STRING:
            case NUMBERP:
                s = 8;
                break;
        }
        od << ".balign " << std::to_string(s) << std::endl;
        od << tr(n) << ":" << std::endl;
        if (d.second && d.first) {
            od << "\t.fill " << std::to_string((d.first + (1 - option_base)) * (d.second + (1 - option_base)))
               << ", " << std::to_string(s) << ", 0" << std::endl;
        } else if (d.first) {
            od << "\t.fill " << std::to_string(d.first + (1 - option_base)) << ", " << std::to_string(s) << ", 0"
               << std::endl;
        } else {
            od << "\t.fill 1, " << std::to_string(s) << ", 0" << std::endl;
        }
    }
    for (auto &[l, s]: stringMap) {
        od << "STR__" + std::to_string(l) << ":" << std::endl;
        od << "\t.byte ";
        for (auto &c: s) {
            char buf[3];
            snprintf(buf, 3, "%2x", c);
            od << "0x" << buf << ", ";
        }
        od << "0x0" << std::endl;
    }
    od << ".global DATA__begin" << std::endl;
    od << ".global DATA__end" << std::endl;
    od << "DATA__begin:" << std::endl;
    long i = 0;
    for (auto f: dataItems) {
        od << ".dword DATA_PTR__" << std::to_string(i++) << std::endl;
    }
    od << "DATA__end:" << std::endl;
    i = 0;
    for (auto &[d, s]: dataItems) {
        od << "DATA_PTR__" << std::to_string(i++) << ":" << std::endl;
        char buffer[32];
        snprintf(buffer, 32, "0x%016lx", *(unsigned long *) (&d));
        od << "\t.dword " << buffer << std::endl;
        od << "\t.byte ";
        for (auto &c: s) {
            char buf[3];
            snprintf(buf, 3, "%2x", c);
            od << "0x" << buf << ", ";
        }
        od << "0x0" << std::endl;
    }
}

void proc_sub_start() {
    sd = 8 + 4 * 8 + max_tmp_count;
    if (sd % 16) {
        sd += 8;
    }
    proc_start(false);
}

long asm_save(eval_ret ret, bool as_reference) {
    long tmp;
    if (as_reference) {
        tmp = add_tmp(PTR);
        if (pval) od << "\tsd a0, " << std::to_string(tmp) << "(sp)" << std::endl;
        return tmp;
    } else {
        switch (ret) {
            case NUMBERC:
                tmp = add_tmp(CHAR);
                if (pval) od << "\tsb a0, " << std::to_string(tmp) << "(sp)" << std::endl;
                return tmp;
            case NUMBERS:
                tmp = add_tmp(CHAR);
                if (pval) od << "\tsh a0, " << std::to_string(tmp) << "(sp)" << std::endl;
                return tmp;
            case NUMBERI:
                tmp = add_tmp(INT);
                if (pval) od << "\tsw a0, " << std::to_string(tmp) << "(sp)" << std::endl;
                return tmp;
            case NUMBERP:
            case STRING:
            case NUMBERL:
                tmp = add_tmp(LONG);
                if (pval) od << "\tsd a0, " << std::to_string(tmp) << "(sp)" << std::endl;
                return tmp;
            case NUMBERF:
            case NUMBERD:
                tmp = add_tmp(DOUBLE);
                if (pval) od << "\tfsd fa0, " << std::to_string(tmp) << "(sp)" << std::endl;
                return tmp;
        }
    }
}

extern char comma_sig[9];

void asm_promote_signature() {
    int i = 0;
    int j = 0;
    int s = 0;
    for (auto &c: comma_sig) {
        if (c == 0) break;
        switch (eval_ret_from_comma(c)) {
            case NUMBERC:
            case NUMBERS:
            case NUMBERI:
            case NUMBERL:
                ++j;
                c = 'l';
                break;
            case NUMBERF:
            case NUMBERD:
                ++i;
                c = 'd';
                break;
            case STRING:
            case NUMBERP:
                ++s;
                break;
        }
    }
    if (j == 0) return;
    int t = i + j;
    j += s;
    assert(t + s == strlen(comma_sig));
    if (t >= 8) throw std::runtime_error("cannot promote signature (not enough floating-point registers)\n");
    for (int k = t - 1; k >= 0; --k) {
        char c = comma_sig[k];
        if (c == 'l') {
            od << "\tfcvt.d.l fa" << std::to_string(k) << ", a" << std::to_string(--j) << std::endl;
            c = 'd';
        } else if (c == 'd') {
            od << "\tfmv.d fa" << std::to_string(k) << ", fa" << std::to_string(--i) << std::endl;
        } else {
            --j;
        }
    }
}

eval_ret asm_promote_numeric(eval_ret ret) {
    switch (ret) {
        case NUMBERC:
        case NUMBERS:
        case NUMBERI:
        case NUMBERL:
            return NUMBERL;
        case NUMBERD:
        case NUMBERP:
        case STRING:
            return ret;
            break;
        case NUMBERF:
            od << "\tfcvt.d.s fa0, fa0" << std::endl;
            return NUMBERD;
    }
}

eval_ret asm_eval_cmp_exp(struct exp_t *left, struct exp_t *right, eval_cmp_op op) {
    if (!pval) {
        asm_save(eval_val(right, false), false);
        eval_val(left, false);
        return NUMBERL;
    }
    auto r0 = eval_val(right, false);
    r0 = asm_promote_numeric(r0);
    auto tmp = asm_save(r0, false);
    auto r1 = eval_val(left, false);
    r1 = asm_promote_numeric(r1);
    if (r0 == STRING && r1 == STRING) {
        if (op != EQ && op != NE) throw std::runtime_error("string expressions can only be tested for equality");
        od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tcall strcmp" << std::endl;
        goto cmp;
    } else if (r0 == STRING || r1 == STRING) {
        throw std::runtime_error("string expression expected");
    } else if (r0 == NUMBERL && r1 == NUMBERL) {
        od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tsub a0, a0, a1" << std::endl;
        goto cmp;
    } else if (r0 == NUMBERD && r1 == NUMBERD) {
        od << "\tfld fa1, " << std::to_string(tmp) << "(sp)" << std::endl;
        goto fcmp;
    } else {
        if (r1 == NUMBERL) {
            od << "\tfcvt.d.l fa1, a0" << std::endl;
        } else {
            assert(r0 == NUMBERL);
            od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
            od << "\tfcvt.d.l fa1, a1" << std::endl;
        }
        fcmp:
        switch (op) {
            case LT:
                od << "\tflt.d a0, fa0, fa1" << std::endl;
                break;
            case LE:
                od << "\tfle.d a0, fa0, fa1" << std::endl;
                break;
            case EQ:
                od << "\tfeq.d a0, fa0, fa1" << std::endl;
                break;
            case NE:
                od << "\tfeq.d a0, fa0, fa1" << std::endl;
                od << "\tseqz a0, a0" << std::endl;
                break;
            case GE:
                od << "\tflt.d a0, fa0, fa1" << std::endl;
                od << "\tseqz a0, a0" << std::endl;
                break;
            case GT:
                od << "\tfle.d a0, fa0, fa1" << std::endl;
                od << "\tseqz a0, a0" << std::endl;
                break;
        }
        return NUMBERL;
    }
    cmp:
    switch (op) {
        case LT:
            od << "\tsltz a0, a0" << std::endl;
            break;
        case LE:
            od << "\tsgtz a0, a0" << std::endl;
            od << "\tseqz a0, a0" << std::endl;
            break;
        case EQ:
            od << "\tseqz a0, a0" << std::endl;
            break;
        case NE:
            od << "\tsnez a0, a0" << std::endl;
            break;
        case GE:
            od << "\tsltz a0, a0" << std::endl;
            od << "\tseqz a0, a0" << std::endl;
            break;
        case GT:
            od << "\tsgtz a0, a0" << std::endl;
            break;
    }
    return NUMBERL;
}

eval_ret asm_eval_cmp(struct op_t *o, struct op_t *o1, eval_cmp_op op) {
    return asm_eval_cmp_exp(o->left, o1->right, op);
}

eval_ret asm_eval_math(struct op_t *o, const char *iop, const char *fop, bool check_fp) {
    if (!pval) {
        asm_save(eval_val(o->right, false), false);
        return eval_val(o->left, false);
    }
    auto r0 = eval_val(o->right, false);
    r0 = asm_promote_numeric(r0);
    auto tmp = asm_save(r0, false);
    auto r1 = eval_val(o->left, false);
    r1 = asm_promote_numeric(r1);
    if (r0 == STRING && r1 == STRING) {
        throw std::runtime_error("invalid OP on strings");
    } else if (r0 == STRING || r1 == STRING) {
        throw std::runtime_error("string expression expected");
    } else if (r0 == NUMBERL && r1 == NUMBERL) {
        od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
        goto m;
    } else if (r0 == NUMBERD && r1 == NUMBERD) {
        od << "\tfld fa1, " << std::to_string(tmp) << "(sp)" << std::endl;
        goto fm;
    } else {
        if (r1 == NUMBERL) {
            od << "\tfcvt.d.l fa0, a0" << std::endl;
            od << "\tfld fa1, " << std::to_string(tmp) << "(sp)" << std::endl;
        } else {
            assert(r0 == NUMBERL);
            od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
            od << "\tfcvt.d.l fa1, a1" << std::endl;
        }
        fm:
        od << "\t" << fop << " fa0, fa0, fa1" << std::endl;
        if (check_fp) od << "\tcall MATH__check_fp" << std::endl;
        return NUMBERD;
    }
    m:
    if (iop) {
        od << "\t" << iop << " a0, a0, a1" << std::endl;
        return NUMBERL;
    } else {
        od << "\tfcvt.d.l fa0, a0" << std::endl;
        od << "\tfcvt.d.l fa1, a1" << std::endl;
        goto fm;
    }
}

eval_ret asm_eval_power(struct op_t *o) {
    if (!pval) {
        add_tmp(DOUBLE);
        eval_val(o->right, false);
        return eval_val(o->left, false);
    }
    auto r0 = eval_val(o->right, false);
    r0 = asm_promote_numeric(r0);
    auto tmp = asm_save(r0, false);
    auto r1 = eval_val(o->left, false);
    r1 = asm_promote_numeric(r1);
    if (r0 == STRING && r1 == STRING) {
        throw std::runtime_error("invalid OP on strings");
    } else if (r0 == STRING || r1 == STRING) {
        throw std::runtime_error("string expression expected");
    } else if (r0 == NUMBERL && r1 == NUMBERL) {
        od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tcall pow__ll" << std::endl;
        return NUMBERD;
    } else if (r0 == NUMBERD && r1 == NUMBERD) {
        od << "\tfld fa1, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tcall pow__dd" << std::endl;
        return NUMBERD;
    } else if (r1 == NUMBERL) {
        od << "\tfld fa0, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tcall pow__ld" << std::endl;
        return NUMBERD;
    } else {
        assert(r0 == NUMBERL);
        od << "\tld a0, " << std::to_string(tmp) << "(sp)" << std::endl;
        od << "\tcall pow__dl" << std::endl;
        return NUMBERD;
    }
}

void asm_process_comma(struct op_t *o) {
    auto tmp_start = get_tmp_count();
    struct op_t *r = o;
    memset(comma_sig, 0, 9);
    while (o) {
        add_tmp(DOUBLE);
        if (o->right->type == exp_t::V) break;
        if (((struct op_t *) o->right->data)->op != ',') break;
        o = (struct op_t *) o->right->data;
    }
    o = r;
    auto tmp_i = tmp_start;
    int i = 0;
    while (o) {
        if (i >= 8) throw std::runtime_error("INVALID NUMBER OF CALL ARGUMENTS");
        switch (eval_val(o->left, false)) {
            case STRING:
                if (pval)
                    od << "\tsd a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'S';
                break;
            case NUMBERL:
                if (pval)
                    od << "\tsd a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'l';
                break;
            case NUMBERD:
                if (pval)
                    od << "\tfsd fa0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'd';
                break;
            case NUMBERC:
                if (pval)
                    od << "\tsb a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'c';
                break;
            case NUMBERS:
                if (pval)
                    od << "\tsh a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 's';
                break;
            case NUMBERI:
                if (pval)
                    od << "\tsw a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'i';
                break;
            case NUMBERF:
                if (pval)
                    od << "\tfsw fa0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'f';
                break;
            case NUMBERP:
                if (pval)
                    od << "\tsd a0, " << std::to_string(tmp_i) << "(sp)" << std::endl;
                comma_sig[i++] = 'p';
                break;
        }
        tmp_i += 8;
        if (o->right->type == exp_t::V) goto final;
        if (((struct op_t *) o->right->data)->op != ',') goto final;
        o = (struct op_t *) o->right->data;
        continue;
        final:
        switch (eval_val(o->right, false)) {
            case NUMBERL:
                comma_sig[i++] = 'l';
                break;
            case NUMBERD:
                comma_sig[i++] = 'd';
                break;
            case STRING:
                comma_sig[i++] = 'S';
                break;
            case NUMBERC:
                comma_sig[i++] = 'c';
                break;
            case NUMBERS:
                comma_sig[i++] = 's';
                break;
            case NUMBERI:
                comma_sig[i++] = 'i';
                break;
            case NUMBERF:
                comma_sig[i++] = 'f';
                break;
            case NUMBERP:
                comma_sig[i++] = 'p';
                break;
        }
        break;
    }
    comma_sig[i] = 0;
    /*
     * a0: last element
     * tmp_start ... tmp_count - 1: head elements
     */
    if (pval) {
        int j = 0;
        int k = 0;
        for (i = 0; i < 1 + (get_tmp_count() - tmp_start) / 8; ++i) {
            if (comma_sig[i] == 'd') ++j;
            else ++k;
        }
        if (comma_sig[0] == 'd') {
            if (j > 1) od << "\tfmv.d fa" << std::to_string(j - 1) << ", fa0" << std::endl;
        } else if (comma_sig[1] == 'f') {
            if (j > 1) od << "\tfmv.s fa" << std::to_string(j - 1) << ", fa0" << std::endl;
        } else {
            if (k > 1) od << "\tmv a" << std::to_string(k - 1) << ", a0" << std::endl;
        }
        j = 0;
        k = 0;
        for (i = 0; i < (get_tmp_count() - tmp_start) / 8; ++i) {
            switch (eval_ret_from_comma(comma_sig[i])) {
                case NUMBERC:
                    od << "\tlb a" << std::to_string(k++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
                case NUMBERS:
                    od << "\tlh a" << std::to_string(k++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
                case NUMBERI:
                    od << "\tlw a" << std::to_string(k++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
                case NUMBERL:
                case NUMBERP:
                case STRING:
                    od << "\tld a" << std::to_string(k++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
                case NUMBERF:
                    od << "\tflw fa" << std::to_string(j++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
                case NUMBERD:
                    od << "\tfld fa" << std::to_string(j++) << ", " << std::to_string(tmp_start + i * 8) << "(sp)"
                       << std::endl;
                    break;
            }
        }
    }
}

eval_ret eval_val(struct exp_t *exp, bool as_reference) {
    if (pval) {
        od << "// ";
        smolmath_log_od(exp);
        od << std::endl;
    }
    skip_val = false;
    if (!exp) {
        throw std::runtime_error("syntax error");
    } else if (exp->type == exp_t::V) {
        auto *v = reinterpret_cast<val_t *>(exp->data);
        switch (v->type) {
            case val_t::L:
                ASSERT(!as_reference);
                if (pval) {
                    od << "\tli a0, " << std::to_string(v->l) << std::endl;
                    auto r = eval_ret_from_suffix(v->suffix);
                    switch (r) {
                        case NUMBERC:
                        case NUMBERS:
                        case NUMBERI:
                        case NUMBERL:
                            break;
                        case NUMBERF:
                            od << "\tfcvt.s.l fa0, a0" << std::endl;
                            break;
                        case NUMBERD:
                            od << "\tfcvt.d.l fa0, a0" << std::endl;
                            break;
                        case STRING:
                        case NUMBERP:
                            break;
                    }
                    return r;
                } else return eval_ret_from_suffix(v->suffix);
            case val_t::F:
                ASSERT(!as_reference);
                if (!pval) return NUMBERD;
                {
                    char buffer[32];
                    snprintf(buffer, 32, "0x%lx", *(unsigned long *) (&v->f));
                    od << "\tli a0, " << buffer << std::endl;
                }
                od << "\tfmv.d.x fa0, a0" << std::endl;
                if (v->suffix == '!') {
                    od << "\tfcvt.s.d fa0, fa0" << std::endl;
                }
                return eval_ret_from_suffix(v->suffix);
            case val_t::N:
                if (local_variables.contains(v->ns)) {
                    auto l = local_variables[v->ns];
                    if (as_reference) {
                        if (pval) od << "\taddi a0, sp, " << std::to_string(l) << std::endl;
                        return eval_ret_from_suffix(v->suffix);
                    } else {
                        auto r = eval_ret_from_suffix(v->suffix);
                        if (pval) {
                            switch (r) {
                                case NUMBERC:
                                    od << "\tlb a0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                                case NUMBERS:
                                    od << "\tlh a0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                                case NUMBERI:
                                    od << "\tlw a0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                                case STRING:
                                case NUMBERP:
                                case NUMBERL:
                                    od << "\tld a0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                                case NUMBERF:
                                    od << "\tflw fa0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                                case NUMBERD:
                                    od << "\tfld fa0, " << std::to_string(l) << "(sp)" << std::endl;
                                    break;
                            }
                        }
                        return r;
                    }
                } else if (defns.contains(v->ns)) {
                    if (current_def && *current_def == v->ns) {
                        throw std::runtime_error("undefined function " + *current_def);
                    }
                    if (strlen(defns[v->ns].data()) != 0) {
                        throw std::runtime_error("invalid number of arguments for function " + std::string(v->ns));
                    }
                    if (pval) {
                        od << "\tcall " << v->ns << std::endl;
                    }
                    return eval_ret_from_suffix(v->suffix);
                } else if (known_funcs.contains(v->ns)) {
                    if (pval) {
                        od << "\tcall " << v->ns << std::endl;
                    }
                    return known_funcs[v->ns];
                } else if (promoting_funcs.contains(v->ns)) {
                    throw std::runtime_error("syntax error");
                } else if (!is_var_name(v->ns)) {
                    if (features.external) {
                        if (pval) {
                            od << "\tcall " << v->ns << std::endl;
                        }
                        return eval_ret_from_suffix(v->suffix);
                    } else {
                        throw std::runtime_error("undefined function " + *current_def);
                    }
                }
                if (var_dims.contains(v->ns)) {
                    auto p = var_dims.at(v->ns);
                    ASSERT(!p.first && !p.second);
                }
                var_dims[v->ns] = std::make_pair(0, 0);
                if (as_reference) {
                    if (pval) od << "\tla a0, " << v->ns << std::endl;
                    return eval_ret_from_suffix(v->suffix);
                } else {
                    auto r = eval_ret_from_suffix(v->suffix);
                    if (pval) {
                        switch (r) {
                            case NUMBERC:
                                od << "\tlb a0, " << tr(v->ns) << std::endl;
                                break;
                            case NUMBERS:
                                od << "\tlh a0, " << tr(v->ns) << std::endl;
                                break;
                            case NUMBERI:
                                od << "\tlw a0, " << tr(v->ns) << std::endl;
                                break;
                            case STRING:
                            case NUMBERP:
                            case NUMBERL:
                                od << "\tld a0, " << tr(v->ns) << std::endl;
                                break;
                            case NUMBERF:
                                od << "\tflw fa0, " << tr(v->ns) << ",a1" << std::endl;
                                break;
                            case NUMBERD:
                                od << "\tfld fa0, " << tr(v->ns) << ",a1" << std::endl;
                                break;
                        }
                    }
                    return r;
                }
            case val_t::S:
                ASSERT(!as_reference);
                if (!string_buf.contains(std::string(v->ns))) {
                    for (char *c = v->ns; *c != 0; ++c) {
                        if (islower(*c)) {
                            throw std::runtime_error("invalid characters found");
                        }
                    }
                    string_map[string_ix] = v->ns;
                    string_buf[v->ns] = string_ix++;
                }
                if (pval) od << "\tla a0, STR__" << std::to_string(string_buf[std::string(v->ns)]) << std::endl;
                return STRING;
        }
    } else {
        auto *o = reinterpret_cast<op_t *>(exp->data);
        if (o->op == ':') {
            if (!o->left) {
                return eval_val(o->right, as_reference);
            }
            auto v = is_name(o->left);
            if (!v) {
                goto e;
            }
            if (is_var_name(*v)) {
                std::string vn = std::string(*v);
                if (o->right->type == exp_t::V) {
                    v:
                    std::pair<long, long> p;
                    if (var_dims.contains(vn)) {
                        p = var_dims.at(vn);
                        if (p.first == 0 || p.second != 0) {
                            throw std::runtime_error("type mismatch for variable " + vn
                                                     +
                                                     "\n info: it was previously used or DIM as a two-dimension array");
                        }
                    } else {
                        if (isdigit(vn[1])) {
                            throw std::runtime_error("numeric variable used as array " + vn);
                        }
                        p = var_dims[vn] = std::make_pair(10, 0);
                    }
                    if (!pval) {
                        eval_val(o->right, false);
                        return eval_ret_from_suffix(vn.back());
                    }
                    switch (eval_val(o->right, false)) {
                        case NUMBERD:
                            od << "\tfcvt.l.d a0, fa0" << std::endl;
                            break;
                        case NUMBERL:
                            break;
                        case STRING:
                            throw std::runtime_error("string index");
                        case NUMBERC:
                        case NUMBERS:
                        case NUMBERI:
                            break;
                        case NUMBERF:
                            od << "\tfcvt.l.s a0, fa0" << std::endl;
                            break;
                        case NUMBERP:
                            throw std::runtime_error("pointer index");
                    }
                    od << "\tli a1, " << std::to_string(p.first) << std::endl;
                    od << "\tli a2, " << std::to_string(option_base) << std::endl;
                    od << "\tcall ARRAY__chk_bound1" << std::endl;
                    od << "\tla a1, " << vn << std::endl;
                    od << "\tadd a0, a0, a1" << std::endl;
                    if (!as_reference) {
                        if (vn.back() == '$') od << "\tld a0, 0(a0)" << std::endl;
                        else od << "\tfld fa0, 0(a0)" << std::endl;
                    }
                    return eval_ret_from_suffix(vn.back());
                } else {
                    auto *op = reinterpret_cast<op_t *>(o->right->data);
                    if (op->op != ',') {
                        goto v;
                    }
                    ASSERT(!is_comma(op->left) && !is_comma(op->right));
                    std::pair<long, long> p;
                    if (var_dims.contains(vn)) {
                        p = var_dims.at(vn);
                        if (p.first == 0 || p.second == 0) {
                            throw std::runtime_error("type mismatch for variable " + vn
                                                     +
                                                     "\n info: it was previously used or DIM as a one-dimension array");
                        }
                    } else {
                        if (isdigit(vn[1])) {
                            throw std::runtime_error("numeric variable used as array " + vn);
                        }
                        p = var_dims[vn] = std::make_pair(10, 10);
                    }
                    auto tmp = add_tmp(LONG);
                    if (!pval) {
                        eval_val(op->left, false);
                        eval_val(op->right, false);
                        return eval_ret_from_suffix(vn.back());
                    }
                    switch (eval_val(op->right, false)) {
                        case NUMBERD:
                            od << "\tfcvt.l.d a0, fa0" << std::endl;
                        case NUMBERC:
                        case NUMBERS:
                        case NUMBERI:
                        case NUMBERL:
                        save:
                            od << "\tsd a0, " << std::to_string(tmp) << "(sp)" << std::endl;
                            break;
                        case STRING:
                            throw std::runtime_error("string index");
                            break;
                        case NUMBERF:
                            od << "\tfcvt.l.s a0, fa0" << std::endl;
                            goto save;
                        case NUMBERP:
                            throw std::runtime_error("pointer index");
                    }
                    switch (eval_val(op->left, false)) {
                        case NUMBERD:
                            od << "\tfcvt.l.d a0, fa0" << std::endl;
                        case NUMBERL:
                            break;
                        case STRING:
                            throw std::runtime_error("string index");
                        case NUMBERC:
                        case NUMBERS:
                        case NUMBERI:
                            break;
                        case NUMBERF:
                            od << "\tfcvt.l.s a0, fa0" << std::endl;
                            break;
                        case NUMBERP:
                            throw std::runtime_error("pointer index");
                            break;
                    }
                    od << "\tld a1, " << std::to_string(tmp) << "(sp)" << std::endl;
                    od << "\tli a2, " << std::to_string(p.first) << std::endl;
                    od << "\tli a3, " << std::to_string(p.second) << std::endl;
                    od << "\tli a4, " << std::to_string(option_base) << std::endl;
                    od << "\tcall ARRAY__chk_bound2" << std::endl;
                    od << "\tla a1, " << vn << std::endl;
                    od << "\tadd a0, a0, a1" << std::endl;
                    auto r = eval_ret_from_suffix(vn.back());
                    if (!as_reference) {
                        switch (r) {
                            case NUMBERC:
                                od << "\tlb a0, 0(a0)" << std::endl;
                                break;
                            case NUMBERS:
                                od << "\tlh a0, 0(a0)" << std::endl;
                                break;
                            case NUMBERI:
                                od << "\tlw a0, 0(a0)" << std::endl;
                                break;
                            case NUMBERL:
                            case NUMBERP:
                            case STRING:
                                od << "\tld a0, 0(a0)" << std::endl;
                                break;
                            case NUMBERF:
                                od << "\tflw fa0, 0(a0)" << std::endl;
                                break;
                            case NUMBERD:
                                od << "\tfld fa0, 0(a0)" << std::endl;
                                break;
                        }
                    }
                    return r;
                }
            } else {
                if (v->starts_with("DEREF")) {
                    auto r = eval_val(o->right, false);
                    if (r != NUMBERP) {
                        throw std::runtime_error("DEREF argument must be a pointer");
                    }
                    if (as_reference) {
                        return eval_ret_from_suffix(v->back());
                    }
                    switch (v->back()) {
                        case '~':
                            if (pval) {
                                od << "\tmovb 0(%rdi), %dil" << std::endl;
                            }
                            return NUMBERC;
                        case '%':
                            if (pval) {
                                od << "\tmovw 0(%rdi), %di" << std::endl;
                            }
                            return NUMBERS;
                        case '|':
                            if (pval) {
                                od << "\tmovd 0(%rdi), %edi" << std::endl;
                            }
                            return NUMBERI;
                        case '&':
                            if (pval) {
                                od << "\tmovq 0(%rdi), %rdi" << std::endl;
                            }
                            return NUMBERL;
                        case '@':
                            if (pval) {
                                od << "\tmovq 0(%rdi), %rdi" << std::endl;
                            }
                            return NUMBERP;
                        case '!':
                            if (pval) {
                                od << "\tmovd 0(%rdi), %xmm0" << std::endl;
                            }
                            return NUMBERF;
                        case '$':
                            if (pval) {
                                od << "\tmovq 0(%rdi), %rdi" << std::endl;
                            }
                            return STRING;
                        default:
                            if (pval) {
                                od << "\tmovq 0(%rdi), %xmm0" << std::endl;
                            }
                            return NUMBERD;
                    }
                }
                ASSERT(!as_reference);
                if (*v == "PTR") {
                    eval_val(o->right, true);
                    return NUMBERP;
                }
                eval_args(o->right);
                if (env == PRINT && *v == "TAB") {
                    if (!pval) return NUMBERL;
                    skip_val = true;
                    od << "\tcall " << *v << "__" << comma_sig << std::endl;
                    return STRING;
                } else if (v->starts_with("CAST")) {
                    switch (v->back()) {
                        case '~':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERC);
                            return NUMBERC;
                        case '%':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERS);
                            return NUMBERS;
                        case '|':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERI);
                            return NUMBERI;
                        case '&':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERL);
                            return NUMBERL;
                        case '@':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERP);
                            return NUMBERP;
                        case '!':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERF);
                            return NUMBERF;
                        case '$':
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), STRING);
                            return STRING;
                        default:
                            if (pval) cast(eval_ret_from_comma(comma_sig[0]), NUMBERD);
                            return NUMBERD;
                    }
                }
                if (current_def && *current_def == *v) {
                    throw std::runtime_error("undefined function " + *current_def);
                }
                if (pval) {
                    if (defns.contains(std::string(*v))) {
                        auto &sig = defns.at(std::string(*v));
                        if (strlen(sig.data()) != strlen(comma_sig)) {
                            throw std::runtime_error("invalid number of arguments for function " + std::string(*v));
                        }
                        if (strlen(sig.data()) == 0) {
                            throw std::runtime_error("syntax error");
                        }
                        if (strlen(comma_sig) > 1 && !features.fulldef) {
                            throw std::runtime_error("syntax error");
                        }
                        {
                            char *c0 = sig.data();
                            char *c1 = comma_sig;
                            while (*c0) {
                                if (*c0 == 'S' || *c1 == 'S') {
                                    if (*c0 != *c1) {
                                        throw std::runtime_error("type mismatch (" + std::string(sig.data()) + " <> " +
                                                                 std::string(comma_sig) + ")");
                                    }
                                }
                                ++c0;
                                ++c1;
                            }
                        }
                        asm_promote_signature();
                        od << "\tcall " << *v << std::endl;
                        return eval_ret_from_suffix(v->back());
                    } else if (promoting_funcs.contains(*v)) {
                        if (strcmp(comma_sig, "l") == 0) {
                            od << "\tfcvt.d.l fa0, a0" << std::endl;
                        } else if (strcmp(comma_sig, "d") != 0) {
                            throw std::runtime_error("syntax error");
                        }
                        od << "\tcall " << *v << "__d" << std::endl;
                    } else if (v->size() == 3 && (*v)[0] == 'F' && (*v)[1] == 'N') {
                        if (features.external) {
                            asm_promote_signature();
                            od << "\tcall " << *v << "__" << comma_sig << std::endl;
                            return eval_ret_from_suffix(v->back());
                        } else {
                            throw std::runtime_error("undefined function " + std::string(*v));
                        }
                    } else {
                        if (comma_sig[0]) od << "\tcall " << *v << "__" << comma_sig << std::endl;
                        else od << "\tcall " << *v << std::endl;
                    }
                } else {
                    if (v->size() == 3 && (*v)[0] == 'F' && (*v)[1] == 'N') {
                        if (!defns.contains(std::string(*v)) && !features.external) {
                            throw std::runtime_error("undefined function " + std::string(*v));
                        }
                    }
                }
                if (known_funcs.contains(*v)) {
                    return known_funcs[*v];
                }
                return eval_ret_from_suffix(v->back());
            }
        }
        ASSERT(!as_reference);
        if (o->op == '+' && !o->left) {
            return eval_val(o->right, as_reference);
        } else if (o->op == '<' && o->left) {
            if (o->right && o->right->type == exp_t::OP) {
                auto *o1 = reinterpret_cast<op_t *>(o->right->data);
                if (o1->op == '>' && !o1->left && o1->right) {
                    return asm_eval_cmp(o, o1, NE);
                } else goto lt;
            } else {
                lt:
                return asm_eval_cmp(o, o, LT);
            }
        } else if (o->op == '=' && o->left && o->right) {
            if (o->left->type == exp_t::OP) {
                auto *o1 = reinterpret_cast<op_t *>(o->left->data);
                if (o1->op == '>' && !o1->right && o1->left) {
                    return asm_eval_cmp(o1, o, GE);
                } else if (o1->op == '<' && !o1->right && o1->left) {
                    return asm_eval_cmp(o1, o, LE);
                } else goto eq;
            }
            eq:
            return asm_eval_cmp(o, o, EQ);
        } else if (o->op == '>' && o->left && o->right) {
            return asm_eval_cmp(o, o, GT);
        } else if (o->op == '-') {
            if (o->left) {
                return asm_eval_math(o, "sub", "fsub.d");
            } else {
                if (!pval) {
                    return eval_val(o->right, false);
                }
                switch (eval_val(o->right, false)) {
                    case NUMBERC:
                    case NUMBERS:
                    case NUMBERI:
                    case NUMBERL:
                        od << "\tsub a0, x0, a0" << std::endl;
                        return NUMBERL;
                    case NUMBERF:
                        od << "\tfcvt.s.l fa1, x0" << std::endl;
                        od << "\tfsub.s fa0, fa1, fa0" << std::endl;
                        return NUMBERF;
                        break;
                    case NUMBERD:
                        od << "\tfcvt.d.l fa1, x0" << std::endl;
                        od << "\tfsub.d fa0, fa1, fa0" << std::endl;
                        return NUMBERD;
                    case STRING:
                    case NUMBERP:
                        throw std::runtime_error("invalid OP");
                }
            }
        } else if (o->op == '+' && o->left && o->right) return asm_eval_math(o, "add", "fadd.d");
        else if (o->op == '*' && o->left && o->right) return asm_eval_math(o, "mul", "fmul.d");
        else if (o->op == '/' && o->left && o->right) return asm_eval_math(o, nullptr, "fdiv.d", true);
        else if (o->op == '^' && o->left && o->right) return asm_eval_power(o);
        e:
        // smolmath_log(exp); fprintf(stderr, "\n");
        throw std::runtime_error("syntax error");
    }
}

void proc_sub_store_args(const std::vector<std::string> &arg_names) {
    int ix = 0;
    int fx = 0;
    for (auto &f: arg_names) {
        switch (eval_ret_from_suffix(f.back())) {
            case NUMBERC:
                od << "\tsb a" << std::to_string(ix++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
            case NUMBERS:
                od << "\tsh a" << std::to_string(ix++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
            case NUMBERI:
                od << "\tsw a" << std::to_string(ix++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
            case STRING:
            case NUMBERP:
            case NUMBERL:
                od << "\tsd a" << std::to_string(ix++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
            case NUMBERF:
                od << "\tfsw fa" << std::to_string(fx++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
            case NUMBERD:
                od << "\tfsd fa" << std::to_string(fx++) << ", " << std::to_string(local_variables[f]) << "(sp)"
                   << std::endl;
                break;
        }
    }
}

void asm_demote(eval_ret ret) {
    switch (ret) {
        case NUMBERF:
            od << "\tfcvt.l.s a0, fa0" << std::endl;
            break;
        case NUMBERD:
            od << "\tfcvt.l.d a0, fa0" << std::endl;
            break;
        case STRING:
            throw std::runtime_error("cannot demote STRING value");
        case NUMBERP:
            throw std::runtime_error("cannot demote POINTER value");
        default:
            break;
    }
}

eval_ret asm_promote(eval_ret ret) {
    switch (ret) {
        case NUMBERC:
        case NUMBERS:
        case NUMBERI:
        case NUMBERL:
            od << "\tfcvt.d.l fa0, a0" << std::endl;
        case NUMBERD:
            return NUMBERD;
        case NUMBERP:
        case STRING:
            return ret;
        case NUMBERF:
            od << "\tfcvt.d.s fa0, fa0" << std::endl;
            return NUMBERF;
    }
}

void asm_jump_label(const std::string &label) {
    od << "\tj " << label << std::endl;
}

void asm_set_label(const std::string &label) {
    od << label << ":" << std::endl;
}

void asm_if_jump(long d) {
    auto tl = std::string(".T") + std::to_string(tmp_labels++);
    od << "beqz a0, " << tl << std::endl;
    od << "j .L" << std::to_string(d) << std::endl;
    od << tl << ":" << std::endl;
}

void asm_for_step(struct exp_t *exp, long step_var) {
    bool is_l = false;
    switch (eval_val(exp, true)) {
        case NUMBERC:
        case NUMBERS:
        case NUMBERI:
        case NUMBERL:
            is_l = true;
            break;
        case NUMBERF:
        case NUMBERD:
            break;
        case STRING:
            throw std::runtime_error("cannot use STRING data in FOR control");
        case NUMBERP:
            throw std::runtime_error("cannot use POINTER data in FOR control");
    }
    if (is_l) {
        od << "\tld a1, 0(a0)" << std::endl;
        od << "\tfcvt.d.l fa0, a1" << std::endl;
    } else {
        od << "\tfld fa0, 0(a0)" << std::endl;
    }
    od << "\tfld fa1, " << std::to_string(get_max_tmp_count() + step_var) << "(sp)" << std::endl;
    od << "\tfadd.d fa0, fa0, fa1" << std::endl;
    if (is_l) {
        od << "\tfcvt.l.d a1, fa0" << std::endl;
        od << "\tsd a1, 0(a0)" << std::endl;
    } else {
        od << "\tfsd fa0, 0(a0)" << std::endl;
    }
}

void cast(eval_ret from, eval_ret to) {
    switch (from) {
        case NUMBERC:
        case NUMBERS:
        case NUMBERI:
        case NUMBERL:
            switch (to) {
                case NUMBERC:
                case NUMBERS:
                case NUMBERI:
                case NUMBERL:
                    break;
                case NUMBERF:
                    od << "\tfcvt.s.l fa0, a0" << std::endl;
                    break;
                case NUMBERD:
                    od << "\tfcvt.d.l fa0, a0" << std::endl;
                    break;
                case STRING:
                case NUMBERP:
                    throw std::runtime_error("invalid cast");
            }
            break;
        case NUMBERF:
            switch (to) {
                case NUMBERC:
                case NUMBERS:
                case NUMBERI:
                case NUMBERL:
                    od << "\tfcvt.l.s a0, fa0" << std::endl;
                    break;
                case NUMBERF:
                    break;
                case NUMBERD:
                    od << "\tfcvt.d.s fa0, fa0" << std::endl;
                    break;
                case STRING:
                case NUMBERP:
                    throw std::runtime_error("invalid cast");
            }
            break;
        case NUMBERD:
            switch (to) {
                case NUMBERC:
                case NUMBERS:
                case NUMBERI:
                case NUMBERL:
                    od << "\tfcvt.l.d a0, fa0" << std::endl;
                    break;
                case NUMBERF:
                    od << "\tfcvt.s.d fa0, fa0" << std::endl;
                    break;
                case NUMBERD:
                    break;
                case STRING:
                case NUMBERP:
                    throw std::runtime_error("invalid cast");
            }
            break;
        case NUMBERP:
        case STRING:
            switch (to) {
                case NUMBERC:
                case NUMBERS:
                case NUMBERI:
                case NUMBERL:
                case NUMBERF:
                case NUMBERD:
                    throw std::runtime_error("invalid cast");
                case STRING:
                case NUMBERP:
                    break;
            }
            break;
    }
}

void asm_assign_simple(struct exp_t *exp, std::string_view vn) {
    auto r = eval_val(exp, false);
    auto to = eval_ret_from_suffix(vn.back());
    if (r != to) {
        cast(r, to);
    }
    switch (to) {
        case NUMBERC:
            od << "\tsb a0, " << vn << ", a1" << std::endl;
            break;
        case NUMBERS:
            od << "\tsh a0, " << vn << ", a1" << std::endl;
            break;
        case NUMBERI:
            od << "\tsw a0, " << vn << ", a1" << std::endl;
            break;
        case NUMBERL:
            od << "\tsd a0, " << vn << ", a1" << std::endl;
            break;
        case NUMBERF:
            od << "\tfsw fa0, " << vn << ", a1" << std::endl;
            break;
        case NUMBERD:
            od << "\tfsd fa0, " << vn << ", a1" << std::endl;
            break;
        case STRING:
        case NUMBERP:
            od << "\tsd a0, " << vn << ", a1" << std::endl;
            break;
    }
}

void asm_read_tmp(eval_ret t, long tmp) {
    switch (t) {
        case NUMBERC:
            od << "lb a0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
        case NUMBERS:
            od << "lh a0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
        case NUMBERI:
            od << "lw a0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
        case STRING:
        case NUMBERP:
        case NUMBERL:
            od << "ld a0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
        case NUMBERF:
            od << "flw fa0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
        case NUMBERD:
            od << "fld fa0, " << std::to_string(tmp) << "(sp)" << std::endl;
            break;
    }
}


eval_ret asm_assign_complex(struct exp_t *var, eval_ret val, long val_tmp) {
    auto to = eval_val(var, true);
    od << "mv a1, a0" << std::endl;
    asm_read_tmp(val, val_tmp);
    if (val != to) {
        cast(val, to);
    }
    switch (to) {
        case NUMBERC:
            od << "sd a0, 0(a1)" << std::endl;
            break;
        case NUMBERS:
            od << "sh a0, 0(a1)" << std::endl;
            break;
        case NUMBERI:
            od << "sw a0, 0(a1)" << std::endl;
            break;
        case NUMBERL:
            od << "sd a0, 0(a1)" << std::endl;
            break;
        case NUMBERF:
            od << "fsw fa0, 0(a1)" << std::endl;
            break;
        case NUMBERD:
            od << "fsd fa0, 0(a1)" << std::endl;
            break;
        case NUMBERP:
        case STRING:
            od << "sd a0, 0(a1)" << std::endl;
            break;
    }
    return to;
}

void asm_print(const std::vector<std::variant<char, exp_t *>> &items) {
    for (auto &i: items) {
        if (auto *c = std::get_if<char>(&i)) {
            if (*c == ',') {
                od << "\tcall PRINT__tab" << std::endl;
            } else {
                ASSERT(*c == ';');
            }
        } else {
            env = PRINT;
            auto e = std::get<exp_t *>(i);
            if (!e) {
                continue;
            }
            auto r = eval_val(e, false);
            if (skip_val) continue;
            switch (r) {
                case NUMBERC:
                case NUMBERS:
                case NUMBERI:
                case NUMBERL:
                    od << "\tcall PRINT__numberl" << std::endl;
                    break;
                case NUMBERD:
                nd:
                    od << "\tcall PRINT__numberd" << std::endl;
                    break;
                case STRING:
                    od << "\tcall PRINT__string" << std::endl;
                    break;
                case NUMBERF:
                    od << "\tfcvt.d.s fa0, fa0" << std::endl;
                    goto nd;
                case NUMBERP:
                    od << "\tcall PRINT__ptr" << std::endl;
                    break;
            }
        }
    }
    if (items.empty() || items.back().index() != 0) {
        od << "\tcall PRINT__nl\n";
    }
}

void asm_for_init(struct exp_t *var, struct exp_t *init, struct exp_t *limit, struct exp_t *step, long lv0, long lv1) {
    // set limit var
    cast(eval_val(limit, false), NUMBERD);
    od << "\tfsd fa0, " << std::to_string(get_max_tmp_count() + lv0) << "(sp)" << std::endl;
    // set increment var
    if (step) {
        cast(eval_val(step, false), NUMBERD);
    } else {
        od << "\tli a0, 1" << std::endl;
        od << "\tfcvt.d.l fa0, a0" << std::endl;
    }
    od << "\tfsd fa0, " << std::to_string(get_max_tmp_count() + lv1) << "(sp)" << std::endl;
    // set var to init
    pval = false;
    auto to = eval_val(var, true);
    pval = true;
    cast(eval_val(init, false), to);
    auto tmp = asm_save(to, false);
    switch (asm_assign_complex(var, to, tmp)) {
        case NUMBERC:
        case NUMBERS:
        case NUMBERI:
        case NUMBERL:
        case NUMBERF:
        case NUMBERD:
            break;
        case STRING:
            throw std::runtime_error("cannot use STRING data as FOR control");
        case NUMBERP:
            throw std::runtime_error("cannot use POINTER data as FOR control");
    }
}

void asm_for_cond(struct exp_t *var, long lv0, long lv1, const std::string &end) {
    cast(eval_val(var, false), NUMBERD);
    od << "\tfld fa1, " << std::to_string(get_max_tmp_count() + lv0) << "(sp)" << std::endl;
    od << "\tfld fa2, " << std::to_string(get_max_tmp_count() + lv1) << "(sp)" << std::endl;
    od << "\tfsub.d fa0, fa0, fa1" << std::endl;
    od << "\tli a0, 1" << std::endl;
    od << "\tfcvt.d.l fa3, a0" << std::endl;
    od << "\tfsgnj.d fa3, fa3, fa2" << std::endl;
    od << "\tfmul.d fa0, fa0, fa3" << std::endl;
    od << "\tfsub.d fa3, fa3, fa3" << std::endl;
    od << "\tflt.d a0, fa3, fa0" << std::endl;
    od << "\tbnez a0, " << end << std::endl;
}

void asm_read(const std::vector<exp_t *> &items) {
    for (auto &i: items) {
        switch (eval_val(i, true)) {
            case NUMBERL:
                od << "\tcall READ__numberl" << std::endl;
                break;
            case NUMBERD:
                od << "\tcall READ__numberd" << std::endl;
                break;
            case STRING:
                od << "\tcall READ__string" << std::endl;
                break;
            case NUMBERC:
                od << "\tcall READ__numberc" << std::endl;
                break;
            case NUMBERS:
                od << "\tcall READ__numbers" << std::endl;
                break;
            case NUMBERI:
                od << "\tcall READ__numberi" << std::endl;
                break;
            case NUMBERF:
                od << "\tcall READ__numberf" << std::endl;
                break;
            case NUMBERP:
                od << "\tcall READ__ptr" << std::endl;
                break;
        }
    }
}

void asm_input(const std::vector<exp_t *> &items, const std::string &start) {
    od << "\tcall INPUT__start" << std::endl;
    std::vector<eval_ret> rt{};
    std::vector<std::pair<eval_ret, long>> tmps{};
    rt.reserve(items.size());
    tmps.reserve(items.size());
    pval = false;
    for (auto i = 0; i < items.size(); ++i) {
        auto r = eval_val(items[i], true);
        rt.push_back(r);
        switch (r) {
            case NUMBERL:
                tmps.emplace_back(r, add_tmp(LONG));
                od << "\tcall INPUT__numberl" << std::endl;
                od << "\tsd a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERD:
                tmps.emplace_back(r, add_tmp(DOUBLE));
                od << "\tcall INPUT__numberd" << std::endl;
                od << "\tfsd fa0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case STRING:
                tmps.emplace_back(r, add_tmp(PTR));
                od << "\tcall INPUT__string" << std::endl;
                od << "\tsd a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERC:
                tmps.emplace_back(r, add_tmp(CHAR));
                od << "\tcall INPUT__numberc" << std::endl;
                od << "\tsb a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERS:
                tmps.emplace_back(r, add_tmp(SHORT));
                od << "\tcall INPUT__numbers" << std::endl;
                od << "\tsh a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERI:
                tmps.emplace_back(r, add_tmp(INT));
                od << "\tcall INPUT__numberi" << std::endl;
                od << "\tsw a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERF:
                tmps.emplace_back(r, add_tmp(FLOAT));
                od << "\tcall INPUT__numberf" << std::endl;
                od << "\tfsw fa0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
            case NUMBERP:
                tmps.emplace_back(r, add_tmp(PTR));
                od << "\tcall INPUT__ptr" << std::endl;
                od << "\tsd a0, " << std::to_string(tmps[i].second) << "(sp)" << std::endl;
                break;
        }
        od << "\tld a0, INPUT__reset" << std::endl;
        od << "\tbnez a0, " << start << std::endl;
    }
    pval = true;
    for (auto i = 0; i < items.size(); ++i) {
        asm_assign_complex(items[i], tmps[i].first, tmps[i].second);
    }
    od << "\tcall INPUT__end" << std::endl;
}

void asm_on_goto(exp_t *v, const std::vector<long> &items) {
    asm_demote(eval_val(v, false));
    int ix = 1;
    for (auto el: items) {
        if (auto o = line_in_for(el)) {
            if (!(o->first <= line_no && o->second >= line_no)) {
                throw std::runtime_error("jump into FOR block");
            }
        }
        od << "\tli a1, " << std::to_string(ix++) << std::endl;
        od << "\tbeq a0, a1, .L" << std::to_string(el) << std::endl;
    }
    od << "\tli a1, " << std::to_string(items.size()) << std::endl;
    od << "\tli a2, " << std::to_string(line_no) << std::endl;
    od << "\tcall ONGOTO__err_notfound" << std::endl;
}

void asm_gosub(long d) {
    auto tl = std::string(".T") + std::to_string(tmp_labels++);
    auto tl1 = std::string(".T") + std::to_string(tmp_labels++);
    od << "\tli a0, " << std::to_string(gosub_depth) << std::endl;
    od << "\tbne a0, s2, " << tl << std::endl;
    od << "\tcall GOSUB__err_overflow" << std::endl;
    od << tl << ":" << std::endl;
    od << "\tlla a0, " << tl1 << std::endl;
    od << "\tsd a0, 0(s1)" << std::endl;
    od << "\taddi s1, s1, 8" << std::endl;
    od << "\taddi s2, s2, 1" << std::endl;
    od << "\tj .L" << std::to_string(d) << std::endl;
    od << tl1 << ":" << std::endl;
}

void asm_return() {
    auto tl = std::string(".T") + std::to_string(tmp_labels++);
    od << "\tbnez s2, " << tl << std::endl;
    od << "\tcall GOSUB__err_underflow" << std::endl;
    od << tl << ":" << std::endl;
    od << "\taddi s1, s1, -8" << std::endl;
    od << "\taddi s2, s2, -1" << std::endl;
    od << "\tld a0, 0(s1)" << std::endl;
    od << "\tjr a0" << std::endl;
}

void asm_call(const std::string &n) {
    od << "\tcall " << n << std::endl;
}
