cmake_minimum_required(VERSION 3.10)
project(random_mean_stats C)

# 与 alpha_scattering 保持同一套约定：显式 -std=c89，绕开 CMake 4.x
# 特性表不认识新 GCC 版本导致的误报。
add_compile_options(-std=c89 -pedantic-errors -Wall -Wextra -O2)

include(CheckCCompilerFlag)
check_c_compiler_flag("-std=c89" HAVE_C89_FLAG)
if(NOT HAVE_C89_FLAG)
    message(FATAL_ERROR "当前编译器不支持 -std=c89")
endif()

add_executable(random_mean_stats
        src/main.c
        src/rng.c
        src/sampling.c
        src/stats.c
        src/histogram.c
        src/irwin_hall.c
        src/lagrange.c
        src/gauss_fit.c
        src/verify.c)

target_include_directories(random_mean_stats PRIVATE include)
target_link_libraries(random_mean_stats m)
