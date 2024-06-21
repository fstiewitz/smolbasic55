// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#ifndef SMOLBASIC55_EVAL_H
#define SMOLBASIC55_EVAL_H

#include <map>
#include <vector>
#include <optional>
#include <string>
#include <array>
#include <set>
#include <deque>
#include "smolmath.h"

#define ASSERT(X) if(!(X)) throw std::runtime_error(#X)

enum eval_ret {
    NUMBERC,
    NUMBERS,
    NUMBERI,
    NUMBERL,
    NUMBERF,
    NUMBERD,
    STRING,
    NUMBERP,
};

eval_ret eval_ret_from_suffix(char c);
eval_ret eval_ret_from_comma(char c);

enum env_t {
    PRINT, OTHER
};
enum eval_cmp_op {
    LT,
    LE,
    EQ,
    NE,
    GE,
    GT
};

extern env_t env;
extern bool pval;
extern bool skip_val;
extern std::map<std::string, std::pair<long, long>> var_dims;
extern std::map<long, std::string> string_map;
extern std::vector<std::pair<double, std::string>> data_items;
extern std::map<std::string, long> local_variables;
extern std::optional<std::string> current_def;
extern std::map<std::string, std::array<char, 9>> defns;
extern env_t env;
extern long string_ix;
extern std::map<std::string, long> string_buf;
extern std::map<std::string_view, eval_ret> known_funcs;
extern std::set<std::string_view> promoting_funcs;
extern long option_base;
extern long tmp_labels;
extern long line_no;
extern std::deque<std::tuple<exp_t *, std::string, std::string, long, long>> for_stack;
extern std::vector<std::pair<long, long>> for_blocks;


void smolmath_log_od(struct exp_t *root);
void eval_args(struct exp_t *exp);
std::optional<std::pair<long, long>> line_in_for(long l);

#endif //SMOLBASIC55_EVAL_H
