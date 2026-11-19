cmake_minimum_required(VERSION 3.10)
project(heat_diffusion C)

# 与本仓库另外两份计算物理作业保持同一套约定：显式 -std=c89，
# 绕开 CMake 4.x 特性表不认识新 GCC 版本导致的误报。
add_compile_options(-std=c89 -pedantic-errors -Wall -Wextra -Werror -O2)

include(CheckCCompilerFlag)
check_c_compiler_flag("-std=c89" HAVE_C89_FLAG)
if(NOT HAVE_C89_FLAG)
    message(FATAL_ERROR "当前编译器不支持 -std=c89")
endif()

add_executable(heat_diffusion
        src/main.c
        src/material.c
        src/grid.c
        src/solver.c
        src/analytic.c
        src/crank.c
        src/converge.c
        src/export.c
        src/verify.c
        src/util.c)

target_include_directories(heat_diffusion PRIVATE include)
target_link_libraries(heat_diffusion m)