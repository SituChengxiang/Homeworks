#include "rk4.h"

/* 单级加权累加辅助宏，避免在每个 k_i 处重复写四行 */
static void combine(const double y_in[RK4_DIM], const double k[RK4_DIM],
		    double coeff, double out[RK4_DIM])
{
	int i;

	for (i = 0; i < RK4_DIM; i++) {
		out[i] = y_in[i] + coeff * k[i];
	}
}

void rk4_step(const double y_in[RK4_DIM], double h,
	      double y_out[RK4_DIM], Rk4RhsFunc rhs)
{
	double k1[RK4_DIM], k2[RK4_DIM], k3[RK4_DIM], k4[RK4_DIM];
	double stage[RK4_DIM];
	int i;

	rhs(y_in, k1);

	combine(y_in, k1, 0.5 * h, stage);
	rhs(stage, k2);

	combine(y_in, k2, 0.5 * h, stage);
	rhs(stage, k3);

	combine(y_in, k3, h, stage);
	rhs(stage, k4);

	for (i = 0; i < RK4_DIM; i++) {
		y_out[i] = y_in[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i]
						  + 2.0 * k3[i] + k4[i]);
	}
}
