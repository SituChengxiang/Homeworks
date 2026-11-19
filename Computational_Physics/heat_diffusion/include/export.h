#ifndef EXPORT_H
#define EXPORT_H

#include <stdio.h>

#include "grid.h"

/*
 * 数据导出。所有数字都用 %.17g 写，读回来必须和内存里逐位相同，
 * 这样第 6 步的"代入式核对"才是在核对真实交付的数，而不是另一份近似值。
 */

/*
 * 剖面文件：一行一个节点，列依次是
 *   x  u(0)  u(10s)  u(20s)  u(30s)  u(收敛后)
 * 这正是题目 (d)(e) 要画的四条曲线加初值。meta 是写进注释头的说明行。
 */
int export_profiles(const char *outdir, const char *name, const Grid *g,
                    const double *u0, const double *u10, const double *u20,
                    const double *u30, const double *ucv,
                    double target, double eps, double t_conv, int periodic,
                    const char *meta);

/*
 * 相隔正好一个 dt 的两条剖面。注释头带上 length/dx/n/nn/dt/alpha/r/periodic/t0，
 * 让核对程序只读这一个文件就能把差分方程
 *   u^{n+1}_i - u^n_i = r (u^n_{i+1} - 2 u^n_i + u^n_{i-1})
 * 的残差代出来，不用去猜参数。
 */
int export_stepdt(const char *outdir, const char *name, const Grid *g,
                  const double *ua, const double *ub, double t0,
                  double dt, double alpha, int periodic, const char *meta);

/* 演化文件的流式写入器：一行一个时刻，先写 t 再写整条温度剖面 */
typedef struct {
    FILE *fp;
    long nn;
    long nslice;
    char path[512];
} EvoWriter;

int evo_open(EvoWriter *w, const char *outdir, const char *name,
             const Grid *g, const char *meta);
int evo_write(EvoWriter *w, double t, const double *u);
int evo_close(EvoWriter *w);

/*
 * 判据扫描文件：列依次是
 *   eps_step  停手步数  停手时刻  停手时的残差 max|u-u_inf|  残差/eps
 * 用来把"副判据比主判据松多少"这件事交给读者复核。
 */
int export_criterion(const char *outdir, const char *name, const char *meta,
                     const double *eps_step, const long *stop_step,
                     const double *stop_time, const double *dev_at_stop,
                     long nrows, double eps_main, double tau, double dt);

/*
 * 通用多列导出：第一列固定是 x，后面跟 ncols 个剖面。
 * 附图（稳定性对照、CN 过冲、误差分解）用它写，避免每种图开一个函数。
 */
int export_columns(const char *outdir, const char *name, const char *meta,
                   const double *x, const double *const *cols, long ncols,
                   const char *const *col_heads, long nn);

/* 汇总：每个 (dx, 边界) 组合的关键数字，报告表格直接引它 */
int export_summary(const char *outdir, const char *name,
                   const double *rows, long ncols, long nrows,
                   const char *const *heads, const char *meta);

#endif