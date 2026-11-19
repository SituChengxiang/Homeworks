#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "crank.h"
#include "util.h"

#define TIME_EPS_REL 1e-12

/*
 * 三对角方程组 a*sub, d*diag, c*sup 的 Thomas 解法。
 * sub[0] 与 sup[m-1] 不参与运算。矩阵严格对角占优，无需选主元。
 * cp / dp 是长度为 m 的工作区。
 */
static void thomas(const double *sub, const double *diag, const double *sup,
                   const double *d, long m, double *x,
                   double *cp, double *dp)
{
    double den;
    long i;

    if (m <= 0) {
        return;
    }
    cp[0] = sup[0] / diag[0];
    dp[0] = d[0] / diag[0];
    for (i = 1; i < m; i++) {
        den = diag[i] - sub[i] * cp[i - 1];
        cp[i] = (i < m - 1) ? sup[i] / den : 0.0;
        dp[i] = (d[i] - sub[i] * dp[i - 1]) / den;
    }
    x[m - 1] = dp[m - 1];
    for (i = m - 2; i >= 0; i--) {
        x[i] = dp[i] - cp[i] * x[i + 1];
    }
}

/*
 * 循环三对角求解：Sherman-Morrison 把两个角元吸收掉。
 * 角元取 s->sup[m-1] 与 s->sub[0] 存放（这两个位置在纯三对角里本来不用，
 * 正好借来存角元，避免再开数组）。
 */
static void cyclic_solve(Crank *s, const double *d, double *x)
{
    long m;
    long i;
    double al;
    double be;
    double vb;
    double uy;
    double k;

    m = s->m;
    al = s->sup[m - 1]; /* A[0][m-1] */
    be = s->sub[0];     /* A[m-1][0] */

    memcpy(s->dg, s->diag, (size_t)m * sizeof(double));
    s->dg[0] -= al;
    s->dg[m - 1] -= be;

    /* u = [al, 0, ..., 0, be] */
    memset(s->uvec, 0, (size_t)m * sizeof(double));
    s->uvec[0] = al;
    s->uvec[m - 1] = be;

    /* 纯三对角部分：sub[0] / sup[m-1] 必须置零才不会被 thomas 误用 */
    s->sub[0] = 0.0;
    s->sup[m - 1] = 0.0;

    thomas(s->sub, s->dg, s->sup, d, m, s->w, s->cp, s->dp);
    thomas(s->sub, s->dg, s->sup, s->uvec, m, s->y, s->cp, s->dp);

    vb = s->w[0] + s->w[m - 1];
    uy = s->y[0] + s->y[m - 1];
    k = vb / (1.0 + uy);
    for (i = 0; i < m; i++) {
        x[i] = s->w[i] - k * s->y[i];
    }

    /* 把角元放回去，下一步还要用 */
    s->sub[0] = be;
    s->sup[m - 1] = al;
}

int crank_init(Crank *s, Grid *g, BoundaryKind bc, double bc_value,
               double alpha, double dt)
{
    long m;
    size_t sz;

    memset(s, 0, sizeof(Crank));
    s->g = g;
    s->bc = bc;
    s->bc_value = bc_value;
    s->alpha = alpha;
    s->dt = dt;

    if (g == NULL || g->u == NULL || dt <= 0.0 || alpha < 0.0) {
        return 1;
    }
    if (bc == BC_DIRICHLET) {
        m = g->n - 1; /* 未知量 i = 1..n-1 */
    } else {
        m = g->n;     /* 未知量 i = 0..n-1，i=n 与 i=0 同一点 */
    }
    if (m < 1) {
        return 2;
    }
    s->m = m;

    sz = (size_t)m * sizeof(double);
    s->diag = (double *)malloc(sz);
    s->sub = (double *)malloc(sz);
    s->sup = (double *)malloc(sz);
    s->rhs = (double *)malloc(sz);
    s->x = (double *)malloc(sz);
    s->cp = (double *)malloc(sz);
    s->dp = (double *)malloc(sz);
    s->dg = (double *)malloc(sz);
    s->uvec = (double *)malloc(sz);
    s->w = (double *)malloc(sz);
    s->y = (double *)malloc(sz);
    if (s->diag == NULL || s->sub == NULL || s->sup == NULL
        || s->rhs == NULL || s->x == NULL || s->cp == NULL || s->dp == NULL
        || s->dg == NULL || s->uvec == NULL || s->w == NULL || s->y == NULL) {
        crank_free(s);
        return 3;
    }

    s->r = alpha * dt / (g->dx * g->dx);
    s->r_last = s->r;
    grid_apply_bc(g, bc, bc_value);
    return 0;
}

void crank_free(Crank *s)
{
    double *p;

    p = s->diag;      s->diag = NULL;      free(p);
    p = s->sub;       s->sub = NULL;       free(p);
    p = s->sup;       s->sup = NULL;       free(p);
    p = s->rhs;       s->rhs = NULL;       free(p);
    p = s->x;         s->x = NULL;         free(p);
    p = s->cp;        s->cp = NULL;        free(p);
    p = s->dp;        s->dp = NULL;        free(p);
    p = s->dg;        s->dg = NULL;        free(p);
    p = s->uvec;      s->uvec = NULL;      free(p);
    p = s->w;         s->w = NULL;         free(p);
    p = s->y;         s->y = NULL;         free(p);
    s->m = 0;
}

/* 按步长 dt 组装矩阵与右端并求解 */
static void crank_step_dt(Crank *s, double dt)
{
    Grid *g;
    double *u;
    double r;
    double half;
    long n;
    long m;
    long i;
    long p;

    g = s->g;
    u = g->u;
    n = g->n;
    m = s->m;
    r = s->alpha * dt / (g->dx * g->dx);
    half = 0.5 * r;
    s->r_last = r;

    for (i = 0; i < m; i++) {
        s->diag[i] = 1.0 + r;
        s->sub[i] = -half;
        s->sup[i] = -half;
    }

    if (s->bc == BC_DIRICHLET) {
        /* 未知量 u[1..n-1]；u[0]、u[n] 是已知边界值，只进右端 */
        for (p = 0; p < m; p++) {
            i = p + 1;
            s->rhs[p] = half * u[i - 1] + (1.0 - r) * u[i] + half * u[i + 1];
        }
        thomas(s->sub, s->diag, s->sup, s->rhs, m, s->x, s->cp, s->dp);
        for (p = 0; p < m; p++) {
            u[p + 1] = s->x[p];
        }
        u[0] = s->bc_value;
        u[n] = s->bc_value;
    } else {
        /* 周期：i = 0..n-1，邻点按模 n 回绕 */
        for (p = 0; p < m; p++) {
            i = p;
            s->rhs[p] = half * u[(i == 0 ? n - 1 : i - 1)]
                        + (1.0 - r) * u[i]
                        + half * u[(i == n - 1 ? 0 : i + 1)];
        }
        /* 角元存进本来不用的位置，cyclic_solve 会取出来 */
        s->sup[m - 1] = -half;
        s->sub[0] = -half;
        cyclic_solve(s, s->rhs, s->x);
        for (p = 0; p < m; p++) {
            u[p] = s->x[p];
        }
        u[n] = u[0];
    }

    s->time += dt;
    s->steps++;
}

void crank_step(Crank *s)
{
    crank_step_dt(s, s->dt);
}

int crank_all_finite(const Crank *s)
{
    return util_all_finite(s->g->u, s->g->nn);
}

int crank_advance(Crank *s, double target_time)
{
    double remain;
    double eps;
    double scale;

    if (target_time < s->time) {
        return 2;
    }
    scale = (target_time > 1.0) ? target_time : 1.0;
    eps = TIME_EPS_REL * scale;

    while (target_time - s->time > eps) {
        remain = target_time - s->time;
        if (remain < s->dt) {
            crank_step_dt(s, remain);
        } else {
            crank_step_dt(s, s->dt);
        }
        if (!crank_all_finite(s)) {
            return 1;
        }
        if (s->steps >= CONV_MAX_STEPS) {
            return 3;
        }
    }
    s->time = target_time;
    return 0;
}

double crank_growth_factor(double r, double q, double dx)
{
    double s;

    s = sin(q * dx / 2.0);
    s *= s;
    return (1.0 - 2.0 * r * s) / (1.0 + 2.0 * r * s);
}