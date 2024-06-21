// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include <cassert>
#include <optional>
#include <set>
#include <array>
#include <deque>
#include "util.h"
#include "eval.h"
#include "asm.h"
#include "features.h"

bool pval = false;
bool skip_val = false;
char comma_sig[9];
std::map<std::string, long> local_variables{};
std::optional<std::string> current_def = std::nullopt;
std::map<std::string, std::array<char, 9>> defns{};
std::map<std::string, std::pair<long, long>> var_dims{};
std::map<long, std::string> string_map;
std::vector<std::pair<double, std::string>> data_items{};
std::set<std::string_view> promoting_funcs = {"ABS", "ATN", "COS", "EXP", "INT", "LOG", "SGN", "SIN", "SQR", "TAN"};
env_t env;
long string_ix = 0;
std::map<std::string, long> string_buf{};
std::map<std::string_view, eval_ret> known_funcs = {
        {"RND", NUMBERD}
};
long option_base = 0;
long tmp_labels = 0;
long line_no;

std::deque<std::tuple<exp_t *, std::string, std::string, long, long>> for_stack{};
std::vector<std::pair<long, long>> for_blocks{};

std::optional<std::pair<long, long>> line_in_for(long l) {
    for (auto &p: for_blocks) {
        auto [s, e] = p;
        if (l > s && l <= e) return p;
    }
    return std::nullopt;
}


void smolmath_log_od(struct exp_t *root) {
    if (!root) return;
    else if (root->type == exp_t::V) {
        auto *v = (struct val_t *) root->data;
        switch (v->type) {
            case val_t::L:
                od << std::to_string(v->l);
                break;
            case val_t::F:
                od << std::to_string(v->l);
                break;
            case val_t::N:
                od << v->ns;
                break;
            case val_t::S:
                od << "\"" << v->ns << "\"";
                break;
        }
    } else {
        assert(root->type == exp_t::OP);
        auto *o = (struct op_t *) root->data;
        if (o->op == '[') {
            smolmath_log_od(o->left);
            od << "[";
            smolmath_log_od(o->right);
            od << "]";
        } else if (o->op == '{') {
            od << "(";
            smolmath_log_od(o->left);
            od << "{";
            assert(o->right && o->right->type == exp_t::OP);
            auto *o1 = (struct op_t *) o->right->data;
            smolmath_log_od(o1->left);
            od << "} ";
            smolmath_log_od(o1->right);
            od << ")";
        } else {
            od << "(";
            smolmath_log_od(o->left);
            od << " " << o->op << " ";
            smolmath_log_od(o->right);
            od << ")";
        }
    }
}

void eval_args(struct exp_t *exp) {
    if (exp == nullptr) {
        comma_sig[0] = 0;
        return;
    }
    if (exp->type == exp_t::OP) {
        auto *o = reinterpret_cast<op_t *>(exp->data);
        if (o->op == ',') {
            asm_process_comma(o);
            return;
        }
    }
    switch (eval_val(exp, false)) {
        case STRING:
            comma_sig[0] = 'S';
            break;
        case NUMBERL:
            comma_sig[0] = 'l';
            break;
        case NUMBERD:
            comma_sig[0] = 'd';
            break;
        case NUMBERC:
            comma_sig[0] = 'c';
            break;
        case NUMBERS:
            comma_sig[0] = 's';
            break;
        case NUMBERI:
            comma_sig[0] = 'i';
            break;
        case NUMBERF:
            comma_sig[0] = 'f';
            break;
        case NUMBERP:
            comma_sig[0] = 'p';
            break;
    }
    comma_sig[1] = 0;
}

eval_ret eval_ret_from_suffix(char c) {
    if (features.type == 0) {
        if (c == '$') return STRING;
        else if (ispunct(c)) throw std::runtime_error("invalid type");
        else return NUMBERD;
    }
    switch (c) {
        case '~':
            return NUMBERC;
        case '%':
            return NUMBERS;
        case '|':
            return NUMBERI;
        case '&':
            return NUMBERL;
        case '@':
            return NUMBERP;
        case '!':
            return NUMBERF;
        case '$':
            return STRING;
        default:
            return NUMBERD;
    }
}

eval_ret eval_ret_from_comma(char c) {
    switch (c) {
        case 'c':
            return NUMBERC;
        case 's':
            return NUMBERS;
        case 'i':
            return NUMBERI;
        case 'l':
            return NUMBERL;
        case 'f':
            return NUMBERF;
        case 'd':
            return NUMBERD;
        case 'S':
            return STRING;
        case 'p':
            return NUMBERP;
        default:
            throw std::runtime_error("unknown type in comma_sig");
    }
}


