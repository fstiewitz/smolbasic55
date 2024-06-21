// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include <string>
#include <iostream>
#include <functional>
#include <fstream>
#include <cassert>
#include <map>
#include <cstring>
#include <variant>
#include <stdexcept>
#include <charconv>
#include <optional>
#include <set>
#include <stack>
#include <cmath>
#include "eval.h"
#include "smolmath.h"
#include "features.h"
#include "asm.h"
#include "util.h"

std::multimap<long, std::string> inline_asm{};

void trim_left(char **line) {
    while (**line && isspace(**line)) ++*line;
}

std::string trim(std::string inp) {
    if (inp.empty()) return inp;
    long i = 0;
    while (i != inp.size() && isspace(inp[i])) ++i;
    long j = (long) inp.size() - 1;
    while (j >= 0 && isspace(inp[j])) --j;
    return inp.substr(i, j - i + 1);
}

static char word_oc = 0;

char *word(char **line) {
    trim_left(line);
    char *w = *line;
    while (**line && !isspace(**line) && (w == *line || !ispunct(**line))) ++*line;
    if (**line) {
        word_oc = **line;
        **line = 0;
        ++*line;
    } else {
        word_oc = 0;
    }
    return w;
}

std::ifstream fd{};
std::ofstream od{};
std::map<long, std::function<void(long)>> lines{};
bool end_found = false;
bool error = false;
long max_line_no = 0;

std::optional<stack_layout_t> outer_stack{};
int option_declared = 0;

static long slurp_number(char **line) {
    char *s = *line;
    char *e = s;
    if (*e == '-' || *e == '+') ++e;
    while (*e && isdigit(*e)) ++e;
    if (*e == '.') {
        ++e;
        while (*e && isdigit(*e)) ++e;
    }
    if (*e == 'e' || *e == 'E') {
        ++e;
        while (*e && isdigit(*e)) ++e;
    }
    *line = e;
    double d;
    std::from_chars(s, e, d);
    return (long) d;
}


void process() {
    if (!for_stack.empty()) {
        line_no = std::get<4>(for_stack.front());
        throw std::runtime_error("FOR without NEXT");
    }
    if (error) {
        return;
    }
    pval = true;
    proc_main_start();
    long ml = 0;
    for (auto &[l, f]: lines) {
        if (l > ml) ml = l;
    }
    for (auto &[l, f]: lines) {
        line_no = l;
        reset_tmp_count();
        f(l);
        if (l != ml) {
            auto [b, e] = inline_asm.equal_range(l);
            while (b != e) {
                od << b->second << std::endl;
                ++b;
            }
        }
    }
    proc_main_end(0);

    auto [b, e] = inline_asm.equal_range(ml);
    while (b != e) {
        od << b->second << std::endl;
        ++b;
    }

    asm_data(var_dims, string_map, data_items);
}

#define VAR_TYPE_STR "$~%|&@!"
#define PREC_STR "::^^/*+->><<="

void parse_line();

void make_call(struct exp_t *exp) {
    eval_val(exp, false);
    lines[line_no] = [exp](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        eval_val(exp, false);
    };
    parse_line();
}

void make_let(struct exp_t *exp) {
    // smolmath_log(exp); fprintf(stderr, "\n");
    ASSERT(exp->type == exp_t::OP);
    auto *o = reinterpret_cast<op_t *>(exp->data);
    ASSERT(o->op == '=' && o->left && o->right);
    auto vn = is_simple_var(o->left);
    if (vn) {
        std::string vnn = std::string(*vn);
        if (var_dims.contains(vnn)) {
            auto p = var_dims.at(vnn);
            if (p.first || p.second) {
                throw std::runtime_error("type mismatch for variable " + vnn
                                         + "\n info: it was previously used or DIM as a one-dimension array");
            }
        }
        var_dims[vnn] = std::make_pair(0, 0);
        eval_val(o->right, false);
        lines[line_no] = [o, vn = *vn](long) {
            asm_set_label(".L" + std::to_string(line_no));
            env = OTHER;
            asm_assign_simple(o->right, vn);
        };
    } else {
        add_tmp(DOUBLE);
        eval_val(o->right, false);
        eval_val(o->left, true);
        lines[line_no] = [o](long lno) {
            asm_set_label(".L" + std::to_string(line_no));
            env = OTHER;
            auto r = eval_val(o->right, false);
            auto l = asm_save(r, false);
            asm_assign_complex(o->left, r, l);
        };
    }
    parse_line();
}

void make_print(struct exp_t *exp) {
    std::vector<std::variant<char, exp_t *>> items{};
    if (exp) {
        if (exp->type == exp_t::V) {
            items.emplace_back(exp);
        } else {
            while (exp) {
                ASSERT(exp->type == exp_t::OP);
                auto *o = (struct op_t *) exp->data;
                if (o->op != ',' && o->op != ';') {
                    if (exp) eval_val(exp, false);
                    items.emplace_back(exp);
                    break;
                }
                if (o->left) eval_val(o->left, false);
                items.emplace_back(o->left);
                items.emplace_back(o->op);
                struct op_t *op = nullptr;
                if (o->right) op = (struct op_t *) o->right->data;
                if (op && (o->right->type == exp_t::V)) {
                    if (o->right) eval_val(o->right, false);
                    items.emplace_back(o->right);
                    break;
                } else {
                    if (!op) {
                        break;
                    }
                    exp = o->right;
                }
            }
        }
    }
    lines[line_no] = [items = std::move(items)](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        asm_print(items);
    };
    parse_line();
}

std::set<long> line_numbers{};

long dest;

void make_if(struct exp_t *exp) {
    eval_val(exp, false);
    lines[line_no] = [exp, d = dest](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        if (!line_numbers.contains(d)) {
            throw std::runtime_error("non-existing line number (" + std::to_string(dest) + ")");
        }
        ASSERT(NUMBERL == eval_val(exp, false));
        asm_if_jump(d);
    };
    parse_line();
}

char *to;
struct exp_t *var;
struct exp_t *init;
struct exp_t *incr;

void make_for3(struct exp_t *step) {
    auto start = std::string(".T") + std::to_string(tmp_labels++);
    auto end = std::string(".T") + std::to_string(tmp_labels++);
    add_tmp(LONG);
    eval_val(var, false);
    eval_val(init, false);
    asm_eval_cmp_exp(var, incr, GT);
    if (step) eval_val(step, false);
    auto lv0 = add_loop_vars();
    auto lv1 = lv0 + 8;
    lines[line_no] = [lv0, lv1, start, end, v = var, i = init, t = incr, st = step](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        asm_for_init(v, i, t, st, lv0, lv1);
        asm_set_label(start);
        asm_for_cond(v, lv0, lv1, end);
    };
    // smolmath_log(var); fprintf(stderr, "\n");
    // smolmath_log(incr); fprintf(stderr, "\n");
    // smolmath_log(step); fprintf(stderr, "\n");
    {
        ASSERT(var->type == exp_t::V);
        auto *v = reinterpret_cast<val_t *>(var->data);
        ASSERT(v->type == val_t::N);
        std::for_each(for_stack.cbegin(), for_stack.cend(), [v](auto &t) {
            struct exp_t *e = std::get<0>(t);
            ASSERT(e);
            ASSERT(e->type == exp_t::V);
            auto *v0 = reinterpret_cast<val_t *>(e->data);
            ASSERT(v0->type == val_t::N);
            if (strcmp(v0->ns, v->ns) == 0) {
                throw std::runtime_error(
                        "FOR uses the same variable as outer FOR at line " + std::to_string(std::get<4>(t)));
            }
        });
    }
    for_stack.emplace_front(var, start, end, lv1, line_no);
    parse_line();
}

void make_for2(struct exp_t *exp) {
    incr = exp;
    if (to) {
        if (smolmath_parse(to, PREC_STR, '-', VAR_TYPE_STR, make_for3)) {
            throw std::runtime_error("syntax error");
        }
    } else make_for3(nullptr);
}

void make_for1(struct exp_t *exp) {
    ASSERT(exp);
    ASSERT(exp->type == exp_t::OP);
    auto *o = reinterpret_cast<op_t *>(exp->data);
    ASSERT(o->op == '=');
    ASSERT(o->left && o->right);
    var = o->left;
    init = o->right;
    char *t = to;
    to = strstr(to, "STEP");
    if (to) {
        *to = 0;
        to += 4;
    }
    if (smolmath_parse(t, PREC_STR, '-', VAR_TYPE_STR, make_for2)) {
        throw std::runtime_error("syntax error");
    }
}

long to_long(struct exp_t *exp) {
    ASSERT(exp);
    ASSERT(exp->type == exp_t::V);
    auto *v = reinterpret_cast<val_t *>(exp->data);
    if (v->type == val_t::L) return v->l;
    else if (v->type == val_t::F) return (long) v->f;
    smolmath_log(exp);
    fprintf(stderr, "\n");
    throw std::runtime_error("cannot convert to long");
}

void queue_data_string(const std::string &line) {
    data_items.emplace_back(NAN, line);
}

bool is_underflow(std::string line) {
    unsigned long i = line.size();
    while (i > 0) {
        double d = 0;
        auto err = std::from_chars(line.data(), line.data() + i, d);
        if (err.ec == std::errc::invalid_argument || err.ec == std::errc::result_out_of_range) {
            --i;
            continue;
        }
        if (fabs(d) > 1) {
            return false;
        } else return true;
    }
    throw std::runtime_error("unexpected result during number conversion");
}

void queue_data(std::string line) {
    if (line.empty()) {
        throw std::runtime_error("syntax error");
    }
    if (!std::all_of(line.cbegin(), line.cend(), [](char c) {
        return isalnum(c) || strchr("+.-", c) || isspace(c);
    })) {
        throw std::runtime_error("invalid characters found");
    }
    double d = 0;
    std::string o = line;
    if (line.front() == '+') o = line.substr(1);
    auto err = std::from_chars(o.data(), o.data() + line.size(), d);
    switch (err.ec) {
        case std::errc::invalid_argument:
        inv:
            data_items.emplace_back(NAN, line);
            break;
        case std::errc::result_out_of_range:
            if (is_underflow(line)) {
                data_items.emplace_back(0, line);
            } else {
                data_items.emplace_back(line[0] == '-' ? -INFINITY : INFINITY, line);
                fprintf(stderr, "%li: warning: numeric constant overflow\n", line_no);
            }
            break;
        default:
            if (err.ptr != o.data() + o.size()) goto inv;
            data_items.emplace_back(d, line);
            break;
    }
}

void make_data(char *line) {
    while (*line) {
        trim_left(&line);
        if (*line == 0) break;
        std::string cstr{};
        if (*line == '"') {
            ++line;
            auto e = strchr(line, '"');
            ASSERT(e);
            queue_data_string(std::string(line, e - line));
            line = e + 1;
            trim_left(&line);
            if (*line == ',') {
                ++line;
                continue;
            } else if (*line == 0) break;
            throw std::runtime_error("syntax error");
        } else {
            auto e = strchr(line, ',');
            if (e) {
                queue_data(trim(std::string(line, e - line)));
                line = e + 1;
            } else {
                queue_data(trim(line));
                break;
            }
        }
    }
    lines[line_no] = [](long l) {
        asm_set_label(".L" + std::to_string(line_no));
    };
    parse_line();
}

void make_read(struct exp_t *exp) {
    std::vector<exp_t *> items{};
    if (exp->type == exp_t::V) {
        items.emplace_back(exp);
    } else {
        while (exp) {
            ASSERT(exp->type == exp_t::OP);
            auto *o = (struct op_t *) exp->data;
            if (o->op != ',' && o->op != ';') {
                if (exp) eval_val(exp, true);
                items.emplace_back(exp);
                break;
            }
            if (o->left) eval_val(o->left, true);
            items.emplace_back(o->left);
            struct op_t *op = nullptr;
            if (o->right) op = (struct op_t *) o->right->data;
            if (op && (o->right->type == exp_t::V)) {
                if (o->right) eval_val(o->right, true);
                items.emplace_back(o->right);
                break;
            } else {
                if (!op) {
                    break;
                }
                exp = o->right;
            }
        }
    }
    ASSERT(!items.empty());
    lines[line_no] = [items](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        asm_read(items);
    };
    parse_line();
}

void make_input(struct exp_t *exp) {
    std::vector<exp_t *> items{};
    if (exp->type == exp_t::V) {
        items.emplace_back(exp);
    } else {
        while (exp) {
            ASSERT(exp->type == exp_t::OP);
            auto *o = (struct op_t *) exp->data;
            if (o->op != ',' && o->op != ';') {
                if (exp) eval_val(exp, true);
                items.emplace_back(exp);
                break;
            }
            if (o->left) eval_val(o->left, true);
            items.emplace_back(o->left);
            struct op_t *op = nullptr;
            if (o->right) op = (struct op_t *) o->right->data;
            if (op && (o->right->type == exp_t::V)) {
                if (o->right) eval_val(o->right, true);
                items.emplace_back(o->right);
                break;
            } else {
                if (!op) {
                    break;
                }
                exp = o->right;
            }
        }
    }
    ASSERT(!items.empty());
    for (auto &i: items) {
        add_tmp(DOUBLE);
    }
    lines[line_no] = [items](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        asm_input(items, ".L" + std::to_string(line_no));
    };
    parse_line();
}

void make_on_goto2(struct exp_t *exp) {
    eval_val(var, false);
    std::vector<long> items{};
    if (exp->type == exp_t::V) {
        items.emplace_back(to_long(exp));
    } else {
        while (exp) {
            ASSERT(exp->type == exp_t::OP);
            auto *o = (struct op_t *) exp->data;
            if (o->op != ',' && o->op != ';') {
                if (exp) eval_val(exp, false);
                items.emplace_back(to_long(exp));
                break;
            }
            if (o->left) eval_val(o->left, false);
            items.emplace_back(to_long(o->left));
            struct op_t *op = nullptr;
            if (o->right) op = (struct op_t *) o->right->data;
            if (op && (o->right->type == exp_t::V)) {
                if (o->right) eval_val(o->right, false);
                items.emplace_back(to_long(o->right));
                break;
            } else {
                if (!op) {
                    break;
                }
                exp = o->right;
            }
        }
    }
    ASSERT(!items.empty());
    if (!std::all_of(items.cbegin(), items.cend(), [](long l) {
        return l > 0;
    })) {
        throw std::runtime_error("non-existing line number");
    }
    lines[line_no] = [items, v = var](long l) {
        asm_set_label(".L" + std::to_string(line_no));
        if (!std::all_of(items.cbegin(), items.cend(), [](long l) {
            return line_numbers.contains(l);
        })) {
            throw std::runtime_error("non-existing line number");
        }
        asm_on_goto(v, items);
    };
    parse_line();
}

void make_on_goto1(struct exp_t *exp) {
    var = exp;
    if (smolmath_parse(to, PREC_STR, '-', VAR_TYPE_STR, make_on_goto2)) {
        throw std::runtime_error("syntax error");
    }
}

void make_def_single(std::string_view name, exp_t *args, exp_t *line) {
    std::vector<std::string> arg_names{};
    if (args) {
        if (args->type == exp_t::V) {
            auto n = is_name(args);
            arg_names.emplace_back(*n);
        } else {
            while (args) {
                ASSERT(args->type == exp_t::OP);
                auto *o = (struct op_t *) args->data;
                if (o->op != ',' && o->op != ';') {
                    auto n = is_name(args);
                    arg_names.emplace_back(*n);
                    break;
                }
                auto n = is_name(o->left);
                arg_names.emplace_back(*n);
                struct op_t *op = nullptr;
                if (o->right) op = (struct op_t *) o->right->data;
                if (op && (o->right->type == exp_t::V)) {
                    n = is_name(o->right);
                    arg_names.emplace_back(*n);
                    break;
                } else {
                    if (!op) {
                        break;
                    }
                    args = o->right;
                }
            }
        }
    }
    for (auto &n: arg_names) {
        if (ispunct(n.back()) && !features.fulldef) {
            throw std::runtime_error("numeric variable name expected");
        }
    }
    if (arg_names.size() > 1 && !features.fulldef) {
        throw std::runtime_error("syntax error");
    }
    auto end = std::string(".T") + std::to_string(tmp_labels++);
    lines[line_no] = [arg_names, line, end, name = std::string(name)](long) {
        asm_jump_label(end);
        asm_set_label(name);

        outer_stack = pushStack();

        for (auto &f: arg_names) {
            ASSERT(!local_variables.contains(f));
            switch (eval_ret_from_suffix(f.back())) {
                case NUMBERC:
                    local_variables[f] = add_tmp(CHAR);
                    break;
                case NUMBERS:
                    local_variables[f] = add_tmp(SHORT);
                    break;
                case NUMBERI:
                    local_variables[f] = add_tmp(INT);
                    break;
                case NUMBERL:
                    local_variables[f] = add_tmp(LONG);
                    break;
                case NUMBERF:
                    local_variables[f] = add_tmp(FLOAT);
                    break;
                case NUMBERD:
                    local_variables[f] = add_tmp(DOUBLE);
                    break;
                case STRING:
                    local_variables[f] = add_tmp(PTR);
                    break;
                case NUMBERP:
                    local_variables[f] = add_tmp(PTR);
                    break;
            }
        }

        auto tmp_s = get_tmp_count();

        pval = false;
        current_def = name;
        eval_val(line, false);
        pval = true;

        proc_sub_start();
        proc_sub_store_args(arg_names);
        reset_tmp_count(tmp_s);
        asm_promote(eval_val(line, false));
        proc_end();

        popStack(*outer_stack);
        outer_stack = std::nullopt;
        local_variables.clear();

        asm_set_label(".L" + std::to_string(line_no));
        asm_set_label(end);
        current_def = std::nullopt;
    };
    assert(arg_names.size() < 9);
    std::array<char, 9> sig{0};
    for (auto i = 0; i < arg_names.size(); ++i) {
        switch (eval_ret_from_suffix(arg_names[i].back())) {
            case NUMBERC:
                sig[i] = 'c';
                break;
            case NUMBERS:
                sig[i] = 's';
                break;
            case NUMBERI:
                sig[i] = 'i';
                break;
            case NUMBERL:
                sig[i] = 'l';
                break;
            case NUMBERF:
                sig[i] = 'f';
                break;
            case NUMBERD:
                sig[i] = 'd';
                break;
            case STRING:
                sig[i] = 'S';
                break;
            case NUMBERP:
                sig[i] = 'p';
                break;
        }
    }
    defns[std::string(name)] = sig;
}

void make_def(struct exp_t *exp) {
    ASSERT(exp);
    ASSERT(exp->type == exp_t::OP);
    auto *o = reinterpret_cast<op_t *>(exp->data);
    struct exp_t *sig = o->left;
    ASSERT(sig);
    ASSERT(!outer_stack);
    if (sig->type == exp_t::V) {
        auto name = is_name(sig);
        ASSERT(name);
        if (defns.contains(std::string(*name))) {
            throw std::runtime_error("function redeclared");
        }
        if (o->op == '=') { // single-line
            make_def_single(*name, nullptr, o->right);
        } else {
            ASSERT(o->op == ':');
            throw std::runtime_error("UNIMPLEMENTED");
        }
    } else {
        auto *sigop = reinterpret_cast<op_t *>(sig->data);
        ASSERT(sigop->op == ':');
        auto name = is_name(sigop->left);
        ASSERT(name);
        if (defns.contains(std::string(*name))) {
            throw std::runtime_error("function redeclared");
        }
        ASSERT(!outer_stack);
        if (o->op == '=') { // single-line
            make_def_single(*name, sigop->right, o->right);
        } else {
            ASSERT(o->op == ':');
            throw std::runtime_error("UNIMPLEMENTED");
        }
    }
    parse_line();
}

std::map<std::string_view, int *> feature_strings = {
        {"EXTERN",  &features.external},
        {"FULLDEF", &features.fulldef},
        {"INLINE",  &features.inline_asm},
        {"NOEND",   &features.noend},
        {"PTR",     &features.ptr},
        {"TYPE",    &features.type}
};

void process_flag(std::string_view f) {
    int v;
    switch (f.front()) {
        case '+': {
            f = f.substr(1);
            v = 1;
            r:
            if (!feature_strings.contains(f)) {
                if (f.starts_with("NO")) {
                    v = 0;
                    f = f.substr(2);
                    goto r;
                }
                goto unk;
            }
            *feature_strings[f] = v;
            break;
        }
        case '-': {
            f = f.substr(1);
            v = 0;
            r1:
            if (!feature_strings.contains(f)) {
                if (f.starts_with("NO")) {
                    v = 1;
                    f = f.substr(2);
                    goto r1;
                }
                goto unk;
            }
            *feature_strings[f] = v;
            break;
        }
        default:
        unk:
            std::cerr << "unknown feature: " << f << std::endl;
            exit(1);
    }
}

void parse_line() {
    reset_tmp_count();
    if (fd.eof()) {
        final:
        if (!end_found && !features.noend) {
            error = true;
            std::cerr << std::to_string(line_no) << ": error: program must have an END statement" << std::endl;
            return;
        }
        process();
        return;
    }
    std::string l;
    std::getline(fd, l);
    char *line = l.data();
    if (fd.eof() && l.empty()) {
        goto final;
    }
    if (!isdigit(*line) && features.inline_asm) {
        inline_asm0:
        inline_asm.emplace(line_no, std::string(line));
        parse_line();
        return;
    }
    if (isspace(*line)) {
        if (features.inline_asm) goto inline_asm0;
        throw std::runtime_error("invalid line number");
    }
    auto line_number = std::string_view(word(&line));
    if (line_number.empty()) {
        if (features.inline_asm) goto inline_asm0;
        throw std::runtime_error("INVALID LINE " + std::string(line));
    } else if (!std::all_of(line_number.cbegin(), line_number.cend(), isdigit)) {
        if (features.inline_asm) goto inline_asm0;
        throw std::runtime_error("invalid line number");
    }
    line_no = 0;
    for (auto &c: line_number) {
        assert(isdigit(c));
        line_no = 10 * line_no + (c - '0');
    }
    if (line_number.size() >= 5) {
        throw std::runtime_error("invalid line number");
    }
    if (line_no <= 0) {
        if (features.inline_asm) goto inline_asm0;
        throw std::runtime_error("invalid line number");
    }
    line_numbers.insert(line_no);
    if (line_no < max_line_no) {
        throw std::runtime_error("invalid line order");
    } else if (line_no == max_line_no) {
        throw std::runtime_error("duplicated line number");
    }
    max_line_no = line_no;
    if (end_found) {
        std::cerr << std::to_string(line_no) << ": error: line after an END statement" << std::endl;
        error = true;
    }
    auto w = std::string_view(word(&line));
    for (auto c: w) {
        if (islower(c)) {
            throw std::runtime_error("invalid characters found");
        }
    }
    if (*line != 0 && !isspace(word_oc)) {
        throw std::runtime_error("no space after keyword " + std::string(w));
    }
    if (w == "PRINT") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_print)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "LET") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_let)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "STOP") {
        trim_left(&line);
        ASSERT(!*line);
        lines[line_no] = [](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            proc_main_end(0);
        };
        parse_line();
    } else if (w == "END") {
        end_found = true;
        trim_left(&line);
        ASSERT(!*line);
        lines[line_no] = [](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            proc_main_end(0);
        };
        parse_line();
    } else if (w == "REM") {
        lines[line_no] = [](long) {
            asm_set_label(".L" + std::to_string(line_no));
        };
        parse_line();
    } else if (w == "INPUT") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_input)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "DIM") {
        while (true) {
            auto s = std::string(word(&line));
            ASSERT(is_var_name(s));
            if (var_dims.contains(s)) {
                throw std::runtime_error("redimensioned variable " + s);
            }
            if (word_oc == '(') {
                auto pe = strchr(line, ')');
                ASSERT(pe);
                long y;
                long x;
                trim_left(&line);
                y = slurp_number(&line);
                trim_left(&line);
                if (*line == ',') {
                    ++line;
                    x = slurp_number(&line);
                    trim_left(&line);
                    if (y <= 0 || x <= 0) {
                        throw std::runtime_error("invalid DIM subscript");
                    }
                    if (isdigit(s[1])) {
                        throw std::runtime_error("numeric variable used as array " + s);
                    }
                    var_dims[s] = std::make_pair(y, x);
                } else {
                    if (y <= 0) {
                        throw std::runtime_error("invalid DIM subscript");
                    }
                    if (isdigit(s[1])) {
                        throw std::runtime_error("numeric variable used as array " + s);
                    }
                    var_dims[s] = std::make_pair(y, 0);
                }
                ASSERT(*line == ')');
                ++line;
            }
            trim_left(&line);
            if (*line == 0) break;
            else if (*line == ',') {
                ++line;
                continue;
            } else {
                throw std::runtime_error("syntax error");
            }
        }

        lines[line_no] = [](long) {
            asm_set_label(".L" + std::to_string(line_no));
        };
        parse_line();
    } else if (w == "GOTO") {
        s_goto:
        auto s = std::string_view(word(&line));
        std::from_chars(s.begin(), s.end(), dest);
        lines[line_no] = [d = dest](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            if (!line_numbers.contains(d)) {
                throw std::runtime_error("non-existing line number (" + std::to_string(d) + ")");
            }
            if (auto o = line_in_for(d)) {
                if (!(o->first <= l && o->second >= l)) {
                    throw std::runtime_error("jump into FOR block");
                }
            }
            asm_jump_label(".L" + std::to_string(d));
        };
        parse_line();
    } else if (w == "GOSUB") {
        s_gosub:
        auto s = std::string_view(word(&line));
        auto ec = std::from_chars(s.begin(), s.end(), dest);
        if (ec.ec == std::errc::invalid_argument || ec.ptr != s.end()) {
            throw std::runtime_error("syntax error");
        }
        lines[line_no] = [d = dest](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            if (!line_numbers.contains(d)) {
                throw std::runtime_error("non-existing line number (" + std::to_string(d) + ")");
            }
            if (auto o = line_in_for(d)) {
                if (!(o->first <= l && o->second >= l)) {
                    throw std::runtime_error("jump into FOR block");
                }
            }
            asm_gosub(d);
        };
        parse_line();
    } else if (w == "RETURN") {
        lines[line_no] = [](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            asm_return();
        };
        parse_line();
    } else if (w == "GO") {
        auto s = std::string_view(word(&line));
        if (s == "TO") goto s_goto;
        else if (s == "SUB") goto s_gosub;
        goto err;
    } else if (w == "IF") {
        auto *end = line + strlen(line) - 1;
        auto *e = end;
        while (e != line && isspace(*e)) --e;
        while (e != line && isdigit(*e)) --e;
        std::from_chars(e + 1, end + 1, dest);
        while (e != line && isspace(*e)) --e;
        *(e + 1) = 0;
        while (e != line && !isspace(*e)) --e;
        if (strcmp(e, " THEN") != 0) {
            throw std::runtime_error("INVALID IF STATEMENT, got: " + std::string(line));
        }
        *e = 0;
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_if)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "ON") {
        auto end = strstr(line, "GO");
        *end = 0;
        ASSERT(end);
        end += 2;
        while (*end && isspace(*end)) ++end;
        if (strncmp(end, "TO", 2) == 0) {
            end += 2;
            to = end;
            if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_on_goto1)) {
                throw std::runtime_error("syntax error");
            }
        } else goto err;
    } else if (w == "READ") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_read)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "DEF") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_def)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "RESTORE") {
        trim_left(&line);
        ASSERT(*line == 0);
        lines[line_no] = [](long) {
            asm_set_label(".L" + std::to_string(line_no));
            asm_call("RESTORE");
        };
        parse_line();
    } else if (w == "RANDOMIZE") {
        trim_left(&line);
        ASSERT(*line == 0);
        lines[line_no] = [](long) {
            asm_set_label(".L" + std::to_string(line_no));
            asm_call("RANDOMIZE");
        };
        parse_line();
    } else if (w == "DATA") {
        make_data(line);
    } else if (w == "OPTION") {
        if (option_declared) {
            throw std::runtime_error("OPTION redeclared");
        }
        option_declared = 1;
        auto ww = std::string_view(word(&line));
        if (ww == "BASE") {
            trim_left(&line);
            auto i = slurp_number(&line);
            trim_left(&line);
            ASSERT(*line == 0);
            ASSERT(i == 0 || i == 1);
            if (!std::all_of(var_dims.cbegin(), var_dims.cend(), [](auto p) -> bool {
                auto [n, d] = p;
                auto [y, x] = d;
                return !(y || x);
            })) {
                throw std::runtime_error("OPTION used after arrays used or DIM");
            }
            option_base = i;
        } else if (ww == "FLAGS") {
            while (true) {
                trim_left(&line);
                char *wstr = line;
                while (*line && !isspace(*line)) ++line;
                auto wrd = std::string_view(wstr, line);
                if (wrd.empty()) break;
                process_flag(wrd);
            }
        }
        lines[line_no] = [](long) {
            asm_set_label(".L" + std::to_string(line_no));
        };
        parse_line();
    } else if (w == "FOR") {
        to = strstr(line, "TO");
        *to = 0;
        to += 2;
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_for1)) {
            throw std::runtime_error("syntax error");
        }
    } else if (w == "NEXT") {
        auto ww = word(&line);
        trim_left(&line);
        ASSERT(!*line);
        if (for_stack.empty()) {
            throw std::runtime_error("NEXT without FOR");
        }
        auto e = std::get<0>(for_stack.front());
        ASSERT(e && e->type == exp_t::V);
        auto *v = reinterpret_cast<val_t *>(e->data);
        ASSERT(v->type == val_t::N);
        if (strcmp(ww, v->ns) != 0) {
            throw std::runtime_error("NEXT without FOR");
        }
        eval_val(e, false);
        auto el = for_stack.front();
        lines[line_no] = [el](long l) {
            asm_set_label(".L" + std::to_string(line_no));
            asm_for_step(std::get<0>(el), std::get<3>(el));
            asm_jump_label(std::get<1>(el));
            asm_set_label(std::get<2>(el));
        };
        for_blocks.emplace_back(std::get<4>(el), line_no);
        for_stack.pop_front();
        parse_line();
    } else if (features.external && w == "CALL") {
        if (smolmath_parse(line, PREC_STR, '-', VAR_TYPE_STR, make_call)) {
            throw std::runtime_error("syntax error");
        }
    } else {
        err:
        throw std::runtime_error("syntax error");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) return 1;
    int i;
    for (i = 1; i < argc - 2; ++i) {
        std::string_view f(argv[i]);
        process_flag(f);
    }
    fd.open(argv[argc - 2]);
    od.open(argv[argc - 1]);
    try {
        parse_line();
    } catch (const std::runtime_error &e) {
        std::cerr << line_no << ": error: " << e.what() << std::endl;
        error = true;
    }
    if (error) return 1;
    return 0;
}
