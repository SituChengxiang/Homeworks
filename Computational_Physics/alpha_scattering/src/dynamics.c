#include "dynamics.h"
#include "coulomb.h"

void alpha_acceleration_from_force(double fx, double fy,
				   double *ax, double *ay)
{
	/* 牛顿第二定律。质量是常量，所以直接除；
	 * 若将来做到相对论速度，这里的 p/m 关系要换掉。
	 */
	*ax = fx / MASS_ALPHA;
	*ay = fy / MASS_ALPHA;
}

void alpha_acceleration(double x, double y, double *ax, double *ay)
{
	double fx, fy;

	coulomb_force(x, y, &fx, &fy);
	alpha_acceleration_from_force(fx, fy, ax, ay);
}

double alpha_kinetic_energy(double vx, double vy)
{
	return 0.5 * MASS_ALPHA * (vx * vx + vy * vy);
}
