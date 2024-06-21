cmake_minimum_required(VERSION 3.26)
project(smolbasic55 CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS, "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,stack-size=10000000")

add_executable(smolbasic55-riscv64 main.cpp smolmath.c smolmath.h
        features.c
        features.h
        asm.h
        asm_riscv.cpp
        eval.cpp
        eval.h
        util.cpp
        util.h)

add_executable(smolbasic55-amd64 main.cpp smolmath.c smolmath.h
        features.c
        features.h
        asm.h
        asm_amd64.cpp
        eval.cpp
        eval.h
        util.cpp
        util.h)

add_executable(smoltest smoltest.cpp)
