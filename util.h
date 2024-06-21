// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#ifndef SMOLBASIC55_UTIL_H
#define SMOLBASIC55_UTIL_H

#include <string_view>
#include <optional>

#include "smolmath.h"

std::string tr(std::string_view in);
bool is_var_name(std::string_view n);
std::optional<std::string_view> is_name(struct exp_t *e);
bool is_comma(struct exp_t *v);
std::optional<std::string_view> is_simple_var(struct exp_t *e);

#endif //SMOLBASIC55_UTIL_H
