#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "solver.h"
#include "util.h"

/* 时刻比较的相对容差：吸收 3000 次 time += dt 的累加误差 */
#define TIME_EPS_REL 1e-12

int ftcs_init(Ftcs *s, Grid *g, BoundaryKind bc, double bc_value,
              double alpha, double dt)
{
    s->g = g;
    s->bc = bc;
    s->bc_value = bc_value;
    s->alpha = alpha;
    s->dt = dt;
    s->r = 0.0;
    s->r_last = 0.0;
    s->time = 0.0;
    s->steps = 0;
    s->work = NULL;

    if (g == NULL || g->u == NULL || dt <= 0.0 || alpha < 0.0) {
        return 1;
    }
    s->work = (double *)malloc((size_t)g->nn * sizeof(double));
    if (s->work == NULL) {
        return 2;
    }
    s->r = alpha * dt / (g->dx * g->dx);
    s->r_last = s->r;
    grid_apply_bc(g, bc, bc_value);
    return 0;
}

void ftcs_free(Ftcs *s)
{
    if (s->work != NULL) {
        free(s->work);
        s->work = NULL;
    }
}

/* 用指定步长走一步；r 随步长重算，末步缩短时依然满足稳定性 */
static void ftcs_step_dt(Ftcs *s, double dt)
{
    Grid *g;
    double *u;
    double *w;
    double r;
    long i;
    long im;
    long ip;
    long n;

    g = s->g;
    u = g->u;
    w = s->work;
    n = g->n;
    r = s->alpha * dt / (g->dx * g->dx);
    s->r_last = r;

    if (s->bc == BC_DIRICHLET) {
        w[0] = s->bc_value;
        w[n] = s->bc_value;
        for (i = 1; i < n; i++) {
            w[i] = u[i] + r * (u[i + 1] - 2.0 * u[i] + u[i - 1]);
        }
    } else {
        /* 周期：只有 i=0..n-1 是独立未知量，i=n 与 i=0 是同一点 */
        for (i = 0; i < n; i++) {
            im = (i == 0) ? n - 1 : i - 1;
            ip = (i == n - 1) ? 0 : i + 1;
            w[i] = u[i] + r * (u[ip] - 2.0 * u[i] + u[im]);
        }
        w[n] = w[0];
    }

    memcpy(u, w, (size_t)g->nn * sizeof(double));
    s->time += dt;
    s->steps++;
}

void ftcs_step(Ftcs *s)
{
    ftcs_step_dt(s, s->dt);
}

int ftcs_all_finite(const Ftcs *s)
{
    return util_all_finite(s->g->u, s->g->nn);
}

int ftcs_advance(Ftcs *s, double target_time)
{
    double remain;
    double eps;
    double scale;

    if (target_time < s->time) {
        return 2; /* 不许倒着走 */
    }
    scale = (target_time > 1.0) ? target_time : 1.0;
    eps = TIME_EPS_REL * scale;

    while (target_time - s->time > eps) {
        remain = target_time - s->time;
        if (remain < s->dt) {
            ftcs_step_dt(s, remain);
        } else {
            ftcs_step_dt(s, s->dt);
        }
        if (!ftcs_all_finite(s)) {
            return 1;
        }
        if (s->steps >= CONV_MAX_STEPS) {
            return 3;
        }
    }
    s->time = target_time; /* 吸掉累加误差，让快照时刻精确 */
    return 0;
}

double ftcs_growth_factor(double r, double q, double dx)
{
    double s;

    s = sin(q * dx / 2.0);
    return 1.0 - 4.0 * r * s * s;
}