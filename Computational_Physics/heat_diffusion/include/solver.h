#ifndef SOLVER_H
#define SOLVER_H

#include "grid.h"

/*
 * 显式 FTCS（forward time, centered space）推进器。
 *
 *   u_i^{n+1} = u_i^n + r (u_{i+1}^n - 2 u_i^n + u_{i-1}^n),
 *   r = alpha dt / dx^2
 *
 * 改写成凸组合就看清楚稳定性从哪来：
 *   u_i^{n+1} = r u_{i-1}^n + (1-2r) u_i^n + r u_{i+1}^n
 * 三个权重非负且和为 1 当且仅当 r <= 1/2，于是新值夹在旧值的
 * 最小最大之间——离散极值原理成立，格式稳定。r > 1/2 时中间权重
 * 变负，最高频模态的放大因子 1-4r 越过 -1，振荡发散。
 *
 * 推进器借用 Grid，不拥有它；自己只持有一块等长的 scratch，
 * 因为显式格式绝不能原地更新（右边的 u_{i-1} 会被本步的新值污染）。
 */
typedef struct {
    Grid *g;             /* 借用的网格与温度场 */
    BoundaryKind bc;
    double bc_value;     /* Dirichlet 两端钉住的温度 */
    double alpha;
    double dt;           /* 名义时间步长 */
    double r;            /* 名义 Fourier 数 alpha*dt/dx^2 */
    double r_last;       /* 最近一步实际用的 r（末步可能缩短） */
    double time;         /* 当前物理时刻 */
    long steps;          /* 已走步数 */
    double *work;        /* scratch，长度 g->nn */
} Ftcs;

/* 成功返回 0。dt<=0 或内存不足返回非 0。不做稳定性拦截——
   r>1/2 也要能跑，负向对照靠它。 */
int ftcs_init(Ftcs *s, Grid *g, BoundaryKind bc, double bc_value,
              double alpha, double dt);
void ftcs_free(Ftcs *s);

/* 走一个名义步长 dt */
void ftcs_step(Ftcs *s);

/*
 * 走到 target_time。剩余时间不足一个 dt 时，末步按剩余时间缩短
 * （r 变小，仍然稳定），这样任意 dt 都能精确落在 10/20/30 s 上。
 * 返回 0 正常；返回 1 表示中途出现非有限值（发散）。
 */
int ftcs_advance(Ftcs *s, double target_time);

/* 温度场里是否有 NaN / inf */
int ftcs_all_finite(const Ftcs *s);

/*
 * 波数为 q 的离散模态的放大因子。
 * 把 u_i = sin(q x_i) 或 cos(q x_i) 代入差分算子：
 *   u_{i+1} - 2u_i + u_{i-1} = -4 sin^2(q dx/2) u_i
 * 所以 G = 1 - 4 r sin^2(q dx / 2)。
 * Dirichlet 第 k 模 q = k pi / l；周期第 k 模 q = 2 k pi / l。
 * 对应的连续解每步衰减 exp(-alpha q^2 dt)，两者之差就是离散误差。
 */
double ftcs_growth_factor(double r, double q, double dx);

#endif