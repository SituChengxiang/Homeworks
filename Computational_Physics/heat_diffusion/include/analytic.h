#ifndef ANALYTIC_H
#define ANALYTIC_H

/*
 * 分离变量法给出的解析解，作为数值解的标尺。
 *
 * 初始条件是方波 u0(x) = 100 于 [l/2-w/2, l/2+w/2]，其余 0。
 * 它在区间端点有跳跃，所以 Fourier 系数按 1/k 衰减——这既是
 * 第 0 节 Gibbs 现象的来源，也意味着 t=0 时级数收敛得很慢。
 * 只要 t>0，exp(-alpha q_k^2 t) 就把高频压死，项数需求骤降。
 *
 * (1) Dirichlet：两端钉 0 度，本征函数 sin(k pi x/l)
 *       u(x,t) = sum_k b_k sin(k pi x/l) exp(-alpha (k pi/l)^2 t)
 *       b_k = (2/l) int_0^l u0 sin(k pi x/l) dx
 *           = (400/(k pi)) sin(k pi/2) sin(k pi w/(2l))
 *     偶数项恒为 0（初值关于中点对称，sin 关于中点是反对称的）。
 *     终态 u_inf = 0，热量从两端漏光。
 *
 * (2) 周期：本征函数 {1, cos(2k pi x/l), sin(2k pi x/l)}
 *       u(x,t) = 10 + sum_k A_k cos(2k pi x/l) exp(-alpha (2k pi/l)^2 t)
 *       A_k = (200/(k pi)) (-1)^k sin(k pi w/l)
 *     正弦系数全为 0（初值关于 x=l/2 对称，sin 项关于它是反对称的）。
 *     零模 = 平均值 = 100*w/l = 10 度，它不随时间变——这就是终态。
 *
 * 两套系数都不许写死数字，全部由 config.h 的 l / w / 100 / 0 推出来。
 */

/* 第 k 模的波数：Dirichlet 取 k*pi/l，周期取 2*k*pi/l */
double analytic_wavenumber(long k, int periodic);

/* 衰减率 alpha * q_k^2，单位 1/s；它的倒数就是该模的衰减时间 */
double analytic_decay_rate(long k, int periodic);

/* Fourier 系数（解析式） */
double analytic_b_dirichlet(long k);
double analytic_a_periodic(long k);

/* 周期边界的零模，即平均温度 = (HOT_TEMP - COLD_TEMP) * HOT_WIDTH / l + COLD_TEMP */
double analytic_mean_periodic(void);

/*
 * 级数求值。nmax <= 0 表示按 tol 自动定项数。
 * 返回该时刻该位置的温度（摄氏度）。
 */
double analytic_u(double x, double t, long nmax, int periodic);

/*
 * 自动定项数：用 |系数| 的上界乘时间因子，找出仍然大于 tol 的最大 k，
 * 再加余量。t<=0 时指数因子不起作用，级数只按 1/k 收敛，此时
 * 返回一个上限项数并由调用方自行说明收敛性（见 Gibbs 检查）。
 */
long analytic_nmax(double t, double tol, int periodic);

/*
 * 独立核对用的数值投影：对 u0 的支撑区间 [HOT_LO, HOT_HI] 做复合 Simpson
 * 积分算 (2/l) int u0 * 本征函数 dx。被积函数在这个区间里是光滑的正弦/
 * 余弦，Simpson 能达到很高精度，正好用来验上面那两个解析系数式。
 * npts 必须是偶数。
 */
double analytic_project_dirichlet(long k, long npts);
double analytic_project_periodic(long k, long npts);

#endif