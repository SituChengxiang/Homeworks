#include <math.h>
#include "coulomb.h"

/* ---------------------------------------------------------------------------
 * 内部辅助：位置 -> 对核的位矢分量与位径平方
 *
 * 力是 1/r^2、势能是 1/r，都由同一个 r_vec 出发，
 * 集中在一处计算可以避免各函数自己算 r 时口径不一致。
 * ------------------------------------------------------------------------- */
static void relative_vector(double x, double y,
			    double *dx, double *dy, double *r2)
{
	*dx = x - X_NUCLEUS;
	*dy = y - Y_NUCLEUS;
	*r2 = (*dx) * (*dx) + (*dy) * (*dy);
}

double coulomb_radius(double x, double y)
{
	double dx, dy, r2;
	relative_vector(x, y, &dx, &dy, &r2);
	return sqrt(r2);
}

double coulomb_potential(double x, double y)
{
	double dx, dy, r2;

	relative_vector(x, y, &dx, &dy, &r2);
	if (r2 < R_SINGULARITY_GUARD * R_SINGULARITY_GUARD) {
		return COUPLING / R_SINGULARITY_GUARD;   /* 上限值，防 inf */
	}
	return COUPLING / sqrt(r2);
}

void coulomb_force(double x, double y, double *fx, double *fy)
{
	double dx, dy, r2, r, r3;

	relative_vector(x, y, &dx, &dy, &r2);

	/* r -> 0 时 1/r^2 发散。这不是物理，是数值爆炸的前兆，
	 * 与其返回一个巨大的力污染后面所有步，不如返回 0，
	 * 让调用方通过 coulomb_radius() 发现粒子过分靠近核并停算。
	 */
	if (r2 < R_SINGULARITY_GUARD * R_SINGULARITY_GUARD) {
		*fx = 0.0;
		*fy = 0.0;
		return;
	}

	r  = sqrt(r2);
	r3 = r2 * r;                     /* = r^3，全程只开一次方 */

	/* F = k q1 q2 * r_vec / r^3，即 k q1 q2 / r^2 沿 r_hat 方向。
	 * 写 /r^3 而不写 /r^2 再乘单位矢，是为了少一次除法，
	 * 也避免 r_hat 归一化时再开一次方。
	 */
	*fx = COUPLING * dx / r3;
	*fy = COUPLING * dy / r3;
}
