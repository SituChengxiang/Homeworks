#ifndef CONVERGE_H
#define CONVERGE_H

#include "grid.h"

/*
 * 收敛判据。题目要求"自设 Dt 以及收敛条件"，这里给两条，并且实测它们的差别
 * ——因为第二条是很多人会误用的那一条。
 *
 * 主判据（采用）：max_i |u_i - u_inf| <= eps
 *   u_inf 由边界条件决定，不是猜的：
 *     Dirichlet 两端钉 0 度，热量从两端漏光 -> u_inf = 0；
 *     周期边界热量无处可去，平均值守恒 -> u_inf = 初值平均（10 度）。
 *   它直接回答"离物理终态还差多少"，与 dt 无关。
 *
 * 副判据（对照）：max_i |u_i^{n+1} - u_i^n| <= eps_step
 *   这是"逐 step 变化量"判据，回答的是"还动不动"，不是"到没到"。
 *   单指数衰减 u - u_inf ~ A exp(-t/tau) 时，单步变化量约等于
 *      (u - u_inf) * dt / tau
 *   于是副判据在残差 eps_step * tau / dt 处就停手，比主判据松 tau/dt 倍。
 *   本例 tau_Dirichlet / dt = 44.05 / 0.01 = 4405，差三个数量级。
 *   衰减越慢（tau 越大）这个漏洞越严重，而铁棍恰好就是慢的那一类。
 */
typedef struct {
    double target;      /* u_inf */
    double eps;         /* 主判据阈值，摄氏度 */
    double max_dev;     /* 当前 max|u - u_inf| */
    double max_jump;    /* 最近一步的 max|u^{n+1} - u^n| */
    long steps;         /* 已观察的步数 */
    int converged;      /* 主判据是否已满足 */
} Converge;

/* 终态温度：Dirichlet 取边界值 0，周期取网格平均（守恒量） */
double converge_target(BoundaryKind bc, const Grid *g);

/* 场对终态的最大偏离 / 相邻两步的最大变化 */
double converge_max_dev(const double *u, long nn, double target);
double converge_max_jump(const double *u_prev, const double *u_now, long nn);

void converge_init(Converge *c, double target, double eps);

/* 每走一步调用一次；steps 自增，并判断主判据 */
void converge_observe(Converge *c, const double *u_prev, const double *u_now,
                      long nn);

/*
 * 主判据达标时刻的单指数预测：t = tau * ln(amp / eps)。
 * amp 是最低阶模的初始幅度（Dirichlet 用 |b_1|，周期用 |A_1|），
 * tau 用对应的最低阶模衰减时间。多模解在后期被最低阶模主导，
 * 所以这个预测应当和实测接近——差多少本身就是个检查。
 */
double converge_predict_time(double alpha, double length, double amp,
                             int periodic, double eps);

/*
 * 从逐 step 变化量的历史里，找"从这一步起永久低于阈值"的第一个步数。
 * 直接取首次穿越是不行的：初期前沿还在陡降，max_jump 会先冲高再回落，
 * 中途可能短暂跌破阈值又弹回去。找不到返回 -1。
 */
long converge_first_permanent(const double *hist, long n, double threshold);

#endif