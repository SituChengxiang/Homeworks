#include <math.h>
#include <stdlib.h>
#include "trajectory.h"
#include "units.h"
#include "coulomb.h"
#include "dynamics.h"

/* ---------------------------------------------------------------------------
 * 容器管理
 * ------------------------------------------------------------------------- */

int traj_alloc(Trajectory *traj, int capacity)
{
	traj->pt = (TrajPoint *)malloc((size_t)capacity * sizeof(TrajPoint));
	if (traj->pt == NULL) {
		traj->n = 0;
		traj->cap = 0;
		return 0;
	}
	traj->cap = capacity;
	traj->n = 0;
	return 1;
}

void traj_free(Trajectory *traj)
{
	free((void *)traj->pt);
	traj->pt = NULL;
	traj->n = 0;
	traj->cap = 0;
}

static int traj_push(Trajectory *traj, double t,
		     double x, double y, double vx, double vy)
{
	if (traj == NULL || traj->pt == NULL) return 1;      /* 不存数据，直接放过 */
	if (traj->n >= traj->cap)            return 0;      /* 容量不足，由上层置溢出标志 */

	traj->pt[traj->n].t  = t;
	traj->pt[traj->n].x  = x;
	traj->pt[traj->n].y  = y;
	traj->pt[traj->n].vx = vx;
	traj->pt[traj->n].vy = vy;
	traj->n++;
	return 1;
}

/* ---------------------------------------------------------------------------
 * 物理量：一阶系统 dy/dt = f(y)，y = {x, y, vx, vy}
 * ------------------------------------------------------------------------- */

void alpha_rhs(const double y[RK4_DIM], double dy[RK4_DIM])
{
	double ax, ay;

	/* 位置方程 */
	dy[0] = y[2];
	dy[1] = y[3];

	/* 速度方程：dv/dt = a(x, y) */
	alpha_acceleration(y[0], y[1], &ax, &ay);
	dy[2] = ax;
	dy[3] = ay;
}

double alpha_energy(double x, double y, double vx, double vy)
{
	return alpha_kinetic_energy(vx, vy) + coulomb_potential(x, y);
}

double alpha_angular_momentum(double x, double y, double vx, double vy)
{
	return MASS_ALPHA * (x * vy - y * vx);
}

/* ---------------------------------------------------------------------------
 * 积分驱动
 * ------------------------------------------------------------------------- */

void alpha_run(double x0, double y0, double vx0, double vy0,
	       const RunControl *ctl, Trajectory *traj, TrajReport *rep)
{
	double y[RK4_DIM];
	double yn[RK4_DIM];
	Particle p;
	double t, E0, L0, v0;
	long nsteps, step;

	/* ---- 初态与基准量 ---- */
	y[0] = x0;  y[1] = y0;  y[2] = vx0; y[3] = vy0;
	p.x  = x0;  p.y  = y0;  p.vx = vx0; p.vy = vy0;

	E0 = alpha_energy(x0, y0, vx0, vy0);
	L0 = alpha_angular_momentum(x0, y0, vx0, vy0);
	v0 = sqrt(vx0 * vx0 + vy0 * vy0);

	if (rep != NULL) {
		rep->e0 = E0;    rep->e_end = E0;
		rep->de_max = 0.0;
		rep->l0 = L0;    rep->dl_max = 0.0;
		rep->r_min = coulomb_radius(x0, y0);
		rep->v0 = v0;
		rep->t_end = 0.0;
		rep->x_end = x0; rep->y_end = y0;
		rep->vx_end = vx0; rep->vy_end = vy0;
		rep->theta_deg = 0.0;
		rep->steps = 0;  rep->stored = 0;
		rep->overflow = 0;
		rep->stopped_by = 0;
	}

	traj_push(traj, 0.0, x0, y0, vx0, vy0);

	/* ---- 步数上限 ----
	 * 按固定步数推进而不是 while (t < t_max)，
	 * 否则 t 会因浮点累加漂移，不同步长下"同一时刻"就不成立了。
	 */
	if (ctl->t_max > 0.0) nsteps = (long)(ctl->t_max / ctl->dt + 0.5);
	else                  nsteps = 1000000000L;

	for (step = 0; step < nsteps; step++) {

		if (ctl->integrator == INTEGRATOR_RK4) {
			rk4_step(y, ctl->dt, yn, alpha_rhs);
			y[0] = yn[0]; y[1] = yn[1]; y[2] = yn[2]; y[3] = yn[3];
		} else {
			p.x = y[0]; p.y = y[1]; p.vx = y[2]; p.vy = y[3];
			stepper_step(&p, ctl->dt, alpha_acceleration);
			y[0] = p.x; y[1] = p.y; y[2] = p.vx; y[3] = p.vy;
		}

		t = (double)(step + 1) * ctl->dt;

		if (rep != NULL) {
			double r  = coulomb_radius(y[0], y[1]);
			double E  = alpha_energy(y[0], y[1], y[2], y[3]);
			double L  = alpha_angular_momentum(y[0], y[1], y[2], y[3]);
			double dE = fabs(E - E0) / fabs(E0);
			double dL = fabs(L - L0) / (fabs(L0) > 0.0 ? fabs(L0) : 1.0);

			if (r < rep->r_min) rep->r_min = r;
			if (dE > rep->de_max) rep->de_max = dE;
			if (dL > rep->dl_max) rep->dl_max = dL;

			rep->steps = step + 1;
			rep->t_end = t;
			rep->x_end = y[0]; rep->y_end = y[1];
			rep->vx_end = y[2]; rep->vy_end = y[3];
			rep->e_end = E;
		}

		/* r 已经小到触发保护，说明步长粗到跟不上场强，立即停算 */
		if (coulomb_radius(y[0], y[1]) < R_SINGULARITY_GUARD) {
			if (rep != NULL) rep->stopped_by = 3;
			break;
		}

		if (ctl->record_every > 0 && (step + 1) % ctl->record_every == 0) {
			if (!traj_push(traj, t, y[0], y[1], y[2], y[3])) {
				if (rep != NULL) {
					rep->overflow = 1;
					rep->stopped_by = 2;
				}
				break;
			}
		}

		/* 越出绘图框就停：轨迹不需要跑到无穷远 */
		if (ctl->stop_box > 0.0 &&
		    (fabs(y[0]) > ctl->stop_box || fabs(y[1]) > ctl->stop_box)) {
			if (rep != NULL) rep->stopped_by = 1;
			break;
		}
	}

	if (rep != NULL && traj != NULL) rep->stored = traj->n;

	/* ---- 框内偏转角：末态速度与初态速度的夹角 ----
	 * 注意这不是卢瑟福的渐近偏转角；
	 * 粒子在框内就停，还没回到无穷远，两者只有在出射足够远时才趋于一致。
	 */
	if (rep != NULL && v0 > 0.0) {
		double ve = sqrt(rep->vx_end * rep->vx_end + rep->vy_end * rep->vy_end);
		double c  = 0.0;
		if (ve > 0.0) {
			c = (vx0 * rep->vx_end + vy0 * rep->vy_end) / (v0 * ve);
			if (c >  1.0) c =  1.0;
			if (c < -1.0) c = -1.0;
			rep->theta_deg = acos(c) * 180.0 / PI_CONST;
		}
	}
}
