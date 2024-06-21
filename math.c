// Copyright (c) 2024      Fabian Stiewitz <fabian (at) stiewitz.pw>
// Licensed under the EUPL-1.2

#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<fenv.h>
#include<float.h>
#include<time.h>

static double val__check(double a, int recover) {
    fexcept_t ex;
    fegetexceptflag(&ex, (FE_ALL_EXCEPT) & (~FE_INEXACT));
    if(ex) {
        if(ex & FE_DIVBYZERO) {
            fprintf(stderr, "warning: division by zero\n");
        } else {
            fprintf(stderr, "warning: operation raised FP exception(s)");
            if (fetestexcept(FE_DIVBYZERO)) fprintf(stderr, " FE_DIVBYZERO");
            if (fetestexcept(FE_INEXACT)) fprintf(stderr, " FE_INEXACT");
            if (fetestexcept(FE_INVALID)) fprintf(stderr, " FE_INVALID");
            if (fetestexcept(FE_OVERFLOW)) fprintf(stderr, " FE_OVERFLOW");
            if (fetestexcept(FE_UNDERFLOW)) fprintf(stderr, " FE_UNDERFLOW");
            fprintf(stderr, "\n");
        }
        feclearexcept(FE_ALL_EXCEPT);
        if(ex == FE_INVALID && !recover) {
            exit(1);
        }
    }
    return a;
}

double MATH__check_fp(double a) {
    return val__check(a, 1);
}

#define check_fp(X) val__check((X), 0)

double pow__ll(long a, long b) {
    // fprintf(stderr, "%f = %li ^ %li\n", pow(a, b), a, b);
    return check_fp(pow((double)a, (double)b));
}

double pow__ld(long a, double b) {
    // fprintf(stderr, "%f = %li ^ %f\n", pow(a, b), a, b);
    return check_fp(pow((double)a, b));
}

double pow__dl(double a, long b) {
    // fprintf(stderr, "%f = %f ^ %li\n", pow(a, (double)b), a, b);
    return check_fp(pow(a, (double)b));
}

double pow__dd(double a, double b) {
    // fprintf(stderr, "%f = %f ^ %f\n", pow(a, b), a, b);
    return check_fp(pow(a, b));
}

double ABS__d(double a) {
    return fabs(a);
}

double ATN__d(double a) {
    // fprintf(stderr, "%f = ATN(%f)\n", atan(a), a);
    return atan(a);
}

double COS__d(double a) {
    return cos(a);
}

double EXP__d(double a) {
    return check_fp(exp(a));
}

double INT__d(double a) {
    return floor(a);
}

double LOG__d(double a) {
    if(a <= 0) {
        fprintf(stderr, "error: function domain error LOG(%G)\n", a);
        exit(1);
    }
    return log(a);
}

void RANDOMIZE() {
    srand(time(0));
}

double RND() {
    double r =  1.0 * rand() / (1.0 * RAND_MAX + 1);
    return r;
}

double SGN__d(double a) {
    if(a == 0) return 0;
    return copysign(1, a);
}

double SIN__d(double a) {
    return sin(a);
}

double SQR__d(double a) {
    if(a < 0) {
        fprintf(stderr, "error: function domain error SQR(%G)\n", a);
        exit(1);
    }
    return sqrt(a);
}

double TAN__d(double a) {
    return tan(a);
}

