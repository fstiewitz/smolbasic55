// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<stdio.h>
#include<stdlib.h>

long ARRAY__chk_bound1(long x, long m, long ob) {
    if(x < ob || x > m) {
        fprintf(stderr, "invalid array access: %li to array of dim (%li)\n", x, m);
        exit(1);
    }
    return (x - ob) * 8;
}

long ARRAY__chk_bound2(long y, long x, long my, long mx, long ob) {
    if(x < ob || x > mx || y < ob || y > my) {
        fprintf(stderr, "invalid array access: (%li, %li) to array of dim (%li, %li)\n", y, x, my, mx);
        exit(1);
    }
    return ((y - ob)  * (mx + (1 - ob)) + (x - ob)) * 8;
}
