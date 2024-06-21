// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#ifndef SMOLBASIC55_ASM_H
#define SMOLBASIC55_ASM_H

#include <fstream>
#include <map>
#include<vector>
#include<string>
#include <variant>
#include "eval.h"

enum type_t {
    CHAR,
    SHORT,
    INT,
    LONG,
    PTR,
    DOUBLE,
    FLOAT
};

void proc_start(bool reserve_gosub = true);
void proc_main_start();
void proc_sub_start();
void proc_main_end(long r);
void proc_end();

void proc_sub_store_args(const std::vector<std::string> &arg_names);

void asm_call(const std::string& n);
void asm_if_jump(long d);
void asm_jump_label(const std::string& label);
void asm_set_label(const std::string& label);
long asm_save(eval_ret ret, bool as_reference);
void asm_demote(eval_ret ret);
eval_ret asm_promote(eval_ret ret);
void asm_promote_signature();
eval_ret asm_eval_cmp_exp(struct exp_t *left, struct exp_t *right, eval_cmp_op op);
eval_ret asm_eval_cmp(struct op_t *o, struct op_t *o1, eval_cmp_op op);
eval_ret asm_eval_math(struct op_t *o, const char *iop, const char *fop, bool check_fp = false);
eval_ret asm_eval_power(struct op_t *o);
void asm_process_comma(struct op_t *o);
eval_ret eval_val(struct exp_t *exp, bool as_reference);

void asm_for_init(struct exp_t *var, struct exp_t *init, struct exp_t *limit, struct exp_t *step, long lv0, long lv1);
void asm_for_cond(struct exp_t *var, long lv0, long lv1, const std::string& end);
void asm_for_step(struct exp_t *exp, long step_var);

void asm_print(const std::vector<std::variant<char, exp_t*>> &items);

void asm_read(const std::vector<exp_t*> &items);

void asm_input(const std::vector<exp_t*> &items, const std::string& start);

void asm_on_goto(exp_t *v, const std::vector<long>& l);

void asm_gosub(long d);
void asm_return();

void asm_assign_simple(struct exp_t *exp, std::string_view vn);
eval_ret asm_assign_complex(struct exp_t *var, eval_ret val, long val_tmp);

void asm_data(
        const std::map<std::string, std::pair<long, long>> &varDims,
        const std::map<long, std::string> &stringMap,
        const std::vector<std::pair<double, std::string>> &dataItems);

void reset_tmp_count(long v = 0);
long get_tmp_count();
long get_max_tmp_count();
long add_tmp(type_t type);
long add_loop_vars();

struct stack_layout_t {
    long sd;
    long tmp_count = 0;
    long max_tmp_count = 0;
};

stack_layout_t pushStack();
void popStack(stack_layout_t st);

extern std::ofstream od;
extern long gosub_depth;

#endif //SMOLBASIC55_ASM_H
