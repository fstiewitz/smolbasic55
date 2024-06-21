// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include "util.h"
#include "features.h"
#include <cctype>
#include <stdexcept>
#include <cstring>

bool is_suffix(char c) {
    if(features.type) {
        return strchr("~%|&@!$", c);
    } else return c == '$';
}

bool is_var_name(std::string_view n) {
    switch (n.size()) {
        case 1:
            return isupper(n[0]);
        case 2:
            return isupper(n[0]) && (isdigit(n[1]) || is_suffix(n[1]));
        default:
            return false;
    }
}

std::optional<std::string_view> is_name(struct exp_t *e) {
    if (e->type == exp_t::OP) return std::nullopt;
    auto *v = reinterpret_cast<val_t *>(e->data);
    if (v->type != val_t::N) {
        throw std::runtime_error("cannot dereference raw number");
    }
    return v->ns;
}

bool is_comma(struct exp_t *v) {
    if (v->type == exp_t::V) return false;
    struct op_t *o = reinterpret_cast<op_t *>(v->data);
    return o->op == ',';
}

std::optional<std::string_view> is_simple_var(struct exp_t *e) {
    if (e->type == exp_t::OP) return std::nullopt;
    auto *v = reinterpret_cast<val_t *>(e->data);
    if (v->type != val_t::N) {
        throw std::runtime_error("cannot dereference raw number");
    }
    if (is_var_name(v->ns)) return v->ns;
    return std::nullopt;
}

static std::string tr_suffix(char c) {
    switch(c) {
        case '~': return "$c";
        case '%': return "$s";
        case '|': return "$i";
        case '&': return "$l";
        case '@': return "$p";
        case '!': return "$f";
        case '$': return "$";
        default: return "";
    }
}

std::string tr(std::string_view in) {
    if(is_suffix(in.back())) {
        return std::string(in.substr(0, in.size() - 1)) + tr_suffix(in.back());
    }
    return std::string(in);
}

