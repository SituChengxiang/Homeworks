cmake_minimum_required(VERSION 3.10)
project(alpha_scattering C)

# ---------------------------------------------------------------------------
# 强制 C89
#
# 这里不用 set(CMAKE_C_STANDARD 89)，而是直接写编译选项。
# 原因：CMake 4.x 的"编译器特性表"不认识 GCC 16 这个新版本号，
# 会误报 "current compiler GNU does not support C89" 而直接 configure 失败，
# 尽管 gcc -std=c89 本身完全可用。显式给 flag 可以绕开这张表，
# 在任何 CMake 版本、任何 CLion 版本下都能配出来。
#
# CMAKE_C_EXTENSIONS OFF 的等价效果由 -std=c89（而非 -std=gnu89）保证。
# ---------------------------------------------------------------------------
add_compile_options(-std=c89 -pedantic-errors -Wall -Wextra -O2)

# 万一将来 CMake 修好了这张表，也可以顺手把语言等级告诉 IDE（可选，不强制）
include(CheckCCompilerFlag)
check_c_compiler_flag("-std=c89" HAVE_C89_FLAG)
if(NOT HAVE_C89_FLAG)
    message(FATAL_ERROR "当前编译器不支持 -std=c89")
endif()

add_executable(alpha_scattering
        src/main.c
        src/cases.c
        src/coulomb.c
        src/dynamics.c
        src/stepper.c
        src/rk4.c
        src/trajectory.c
        src/rutherford.c
        src/export.c)

target_include_directories(alpha_scattering PRIVATE include)

# 数学库：C89 提供 math.h，但 libm 需要显式链接
target_link_libraries(alpha_scattering m)
