// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#ifndef SMOLBASIC55_FEATURES_H
#define SMOLBASIC55_FEATURES_H

struct features_t {
    int external;
    int fulldef;
    int inline_asm;
    int noend;
    int ptr;
    int type;
};

extern struct features_t features;

#endif //SMOLBASIC55_FEATURES_H
