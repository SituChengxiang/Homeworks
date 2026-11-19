#include <stdlib.h>
#include <math.h>

#include "config.h"
#include "material.h"
#include "converge.h"

double converge_target(BoundaryKind bc, const Grid *g)
{
    if (bc == BC_DIRICHLET) {
        return COLD_TEMP; /* 两端钉 0 度，最终处处 0 */
    }
    return grid_mean_periodic(g); /* 守恒的平均值，就是终态 */
}

double converge_max_dev(const double *u, long nn, double target)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(u[i] - target);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

double converge_max_jump(const double *u_prev, const double *u_now, long nn)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(u_now[i] - u_prev[i]);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

void converge_init(Converge *c, double target, double eps)
{
    c->target = target;
    c->eps = eps;
    c->max_dev = 0.0;
    c->max_jump = 0.0;
    c->steps = 0;
    c->converged = 0;
}

void converge_observe(Converge *c, const double *u_prev, const double *u_now,
                      long nn)
{
    c->max_dev = converge_max_dev(u_now, nn, c->target);
    c->max_jump = converge_max_jump(u_prev, u_now, nn);
    c->steps++;
    if (c->max_dev <= c->eps) {
        c->converged = 1;
    }
}

double converge_predict_time(double alpha, double length, double amp,
                             int periodic, double eps)
{
    double tau;

    if (amp <= eps || alpha <= 0.0) {
        return 0.0;
    }
    tau = periodic ? material_tau_periodic(alpha, length)
                   : material_tau_dirichlet(alpha, length);
    return tau * log(amp / eps);
}

/*
 * hist[i] 是"第 i+1 步之后"的 max_jump（0 基数组，1 基步数）。
 * 返回最小的步数 S，使得从 S 步起每一步都满足 hist <= threshold；
 * 若整个历史都满足则 S = 1；若连最后一步都不满足则返回 -1。
 *
 * 必须这样从后往前扫，不能取"首次穿越"：初期陡峭前沿还在快速下降，
 * max_jump 会先冲高再单调回落，中途可能短暂跌破阈值又弹回去。
 */
long converge_first_permanent(const double *hist, long n, double threshold)
{
    long i;

    if (n <= 0) {
        return -1;
    }
    i = n - 1;
    while (i >= 0 && hist[i] <= threshold) {
        i--;
    }
    if (i < 0) {
        return 1; /* 从头就满足 */
    }
    if (i + 1 >= n) {
        return -1; /* 最后一步仍超阈值 */
    }
    return i + 2; /* 下标 i+1 对应步数 i+2 */
}