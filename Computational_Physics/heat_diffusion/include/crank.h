#ifndef CRANK_H
#define CRANK_H

#include "grid.h"

/*
 * Crank-Nicolson（隐式，二阶时间）。把方程写成
 *   (I - (r/2) L) u^{n+1} = (I + (r/2) L) u^n,   r = alpha dt / dx^2
 * 左右两边在 t_{n+1/2} 处都是二阶精度，所以整个格式是 O(dt^2) + O(dx^2)。
 *
 * 左端矩阵是三对角的：
 *   对角 1+r，上下对角 -r/2，周期边界再加两个角元 -r/2。
 * 它严格对角占优（|1+r| > r），Thomas 追赶法不需要选主元就稳定。
 *
 * 周期边界的矩阵是"循环三对角"，用 Sherman-Morrison 降回三对角：
 *   取 u = [alpha, 0,...,0, beta]^T, v = [1, 0,...,0, 1]^T，
 *   A' = A - u v^T 就把两个角元抹掉了，代价是对角首尾各减去 alpha / beta
 *   （我们这里 alpha = beta = -r/2，于是首尾对角变成 1 + 1.5r，更占优）。
 *   解两次 A'（一次右端是 rhs，一次右端是 u），再按
 *     x = A'^{-1}b - (v . A'^{-1}b)/(1 + v . A'^{-1}u) * A'^{-1}u
 *   合成。这是 Numerical Recipes 的 cyclic 做法，O(m) 复杂度。
 *
 * 稳定性：第 k 模的放大因子
 *   G = (1 - 2 r s^2) / (1 + 2 r s^2),  s = sin(q dx / 2)
 * 对任何 r 都满足 |G| <= 1，所以无条件稳定。但注意 r -> inf 时最高频模
 * 的 G -> -1：它不衰减，只是每步翻一次符号。也就是说 CN 无条件稳定，
 * 却无条件"不单调"——大步长会留下不衰减的锯齿。这是 CN 的真实代价，
 * 第 4 步会把它量出来。
 */
typedef struct {
    Grid *g;             /* 借用 */
    BoundaryKind bc;
    double bc_value;
    double alpha;
    double dt;
    double r;            /* 名义 Fourier 数 */
    double r_last;
    double time;
    long steps;
    long m;              /* 未知数个数：Dirichlet = n-1，周期 = n */
    double *diag;        /* 长度 m */
    double *sub;         /* 长度 m，sub[0] 不用 */
    double *sup;         /* 长度 m，sup[m-1] 不用 */
    double *rhs;         /* 长度 m */
    double *x;           /* 长度 m */
    double *cp;          /* Thomas 工作区 */
    double *dp;          /* Thomas 工作区 */
    double *dg;          /* Sherman-Morrison 修正后的对角 */
    double *uvec;        /* SM 的 u */
    double *w;           /* A'^{-1} rhs */
    double *y;           /* A'^{-1} u */
} Crank;

int crank_init(Crank *s, Grid *g, BoundaryKind bc, double bc_value,
               double alpha, double dt);
void crank_free(Crank *s);

void crank_step(Crank *s);
int crank_advance(Crank *s, double target_time);
int crank_all_finite(const Crank *s);

/* CN 的离散模态放大因子 G = (1-2r s^2)/(1+2r s^2) */
double crank_growth_factor(double r, double q, double dx);

#endif