#ifndef MATERIAL_H
#define MATERIAL_H

/*
 * 材料层：只负责把"铁"翻译成方程里的系数，以及由系数派生出
 * 离散参数必须遵守的界限。不涉及网格，也不涉及时间推进。
 */

/* 采用的热扩散率 alpha，m^2/s */
double material_alpha(void);

/* 由 k/(rho*cp) 独立算出的 alpha，用来交叉核对表值 */
double material_alpha_from_krc(void);

/* (表值 - k/rho/cp) / (k/rho/cp)，相对偏差 */
double material_alpha_rel_dev(void);

/* Fourier 数 r = alpha*dt/dx^2，显式格式的稳定性就看它 */
double material_fourier_number(double alpha, double dx, double dt);

/* FTCS 稳定的最大时间步长 dx^2/(2*alpha)，s */
double material_dt_max_ftcs(double alpha, double dx);

/*
 * 扩散时间尺度：最低阶模态衰减到 1/e 所需的时间。
 * Dirichlet（两端 0 度）最低模是 sin(pi x/l)，tau = l^2/(pi^2 alpha)；
 * 周期边界最低非零模是 cos(2 pi x/l)，tau = l^2/(4 pi^2 alpha)。
 * 周期边界的 tau 是 Dirichlet 的 1/4，所以它收敛快得多。
 */
double material_tau_dirichlet(double alpha, double length);
double material_tau_periodic(double alpha, double length);

#endif