#include <stdlib.h>
#include <math.h>

#include "config.h"
#include "grid.h"

static double overlap_of(double a_lo, double a_hi, double b_lo, double b_hi)
{
    double lo;
    double hi;

    lo = (a_lo > b_lo) ? a_lo : b_lo;
    hi = (a_hi < b_hi) ? a_hi : b_hi;
    return (hi > lo) ? hi - lo : 0.0;
}

int grid_init(Grid *g, double length, double dx)
{
    long n;
    long i;

    g->n = 0;
    g->nn = 0;
    g->dx = 0.0;
    g->length = 0.0;
    g->x = NULL;
    g->u = NULL;

    if (length <= 0.0 || dx <= 0.0 || dx > length) {
        return 1;
    }

    n = (long)(length / dx + 0.5);
    if (n < 2) {
        return 2;
    }

    g->x = (double *)malloc((size_t)(n + 1) * sizeof(double));
    g->u = (double *)malloc((size_t)(n + 1) * sizeof(double));
    if (g->x == NULL || g->u == NULL) {
        grid_free(g);
        return 3;
    }

    g->n = n;
    g->nn = n + 1;
    g->length = length;
    g->dx = length / (double)n; /* 用实际步长，避免 l/dx 不整除时铺不满 */

    for (i = 0; i <= n; i++) {
        g->x[i] = (double)i * g->dx;
        g->u[i] = 0.0;
    }
    g->x[n] = length; /* 端点取精确值，不让累加误差漂出去 */
    return 0;
}

void grid_free(Grid *g)
{
    if (g->x != NULL) {
        free(g->x);
        g->x = NULL;
    }
    if (g->u != NULL) {
        free(g->u);
        g->u = NULL;
    }
    g->n = 0;
    g->nn = 0;
}

double grid_cell_width(const Grid *g, long i, int periodic)
{
    if (!periodic && (i == 0 || i == g->n)) {
        return g->dx / 2.0;
    }
    return g->dx;
}

double grid_cell_overlap(double xc, double dx, double hot_lo, double hot_hi,
                         double length, int periodic)
{
    double c_lo;
    double c_hi;
    double sum;
    int shift;

    c_lo = xc - dx / 2.0;
    c_hi = xc + dx / 2.0;

    if (!periodic) {
        if (c_lo < 0.0) {
            c_lo = 0.0;
        }
        if (c_hi > length) {
            c_hi = length;
        }
        return overlap_of(c_lo, c_hi, hot_lo, hot_hi);
    }

    /* 周期：高温段在圆上等价于它自己加上 +-length 的平移 */
    sum = 0.0;
    for (shift = -1; shift <= 1; shift++) {
        sum += overlap_of(c_lo, c_hi,
                          hot_lo + (double)shift * length,
                          hot_hi + (double)shift * length);
    }
    return sum;
}

void grid_initial_fv(Grid *g, double hot_lo, double hot_hi,
                     double hot_temp, double cold_temp, int periodic)
{
    long i;
    double w;
    double ov;

    for (i = 0; i < g->nn; i++) {
        w = grid_cell_width(g, i, periodic);
        ov = grid_cell_overlap(g->x[i], g->dx, hot_lo, hot_hi,
                               g->length, periodic);
        if (ov > w) { /* 浮点保护：重叠不可能超过控制体本身 */
            ov = w;
        }
        g->u[i] = cold_temp + (hot_temp - cold_temp) * (ov / w);
    }
    if (periodic) {
        g->u[g->n] = g->u[0];
    }
}

void grid_initial_naive(Grid *g, double hot_lo, double hot_hi,
                        double hot_temp, double cold_temp)
{
    long i;

    for (i = 0; i < g->nn; i++) {
        if (g->x[i] >= hot_lo - GEOM_TOL && g->x[i] <= hot_hi + GEOM_TOL) {
            g->u[i] = hot_temp;
        } else {
            g->u[i] = cold_temp;
        }
    }
}

double grid_heat_dirichlet(const Grid *g)
{
    long i;
    double sum;

    sum = 0.0;
    for (i = 0; i < g->nn; i++) {
        sum += g->u[i] * grid_cell_width(g, i, 0);
    }
    return sum;
}

double grid_heat_periodic(const Grid *g)
{
    long i;
    double sum;

    sum = 0.0;
    for (i = 0; i < g->n; i++) { /* 不含 i=n，它与 i=0 重复 */
        sum += g->u[i] * g->dx;
    }
    return sum;
}

double grid_mean_periodic(const Grid *g)
{
    return grid_heat_periodic(g) / g->length;
}

double grid_symmetry_defect(const Grid *g)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < g->nn; i++) {
        d = fabs(g->u[i] - g->u[g->n - i]);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

long grid_count_equal(const Grid *g, double value, double tol)
{
    long i;
    long count;

    count = 0;
    for (i = 0; i < g->nn; i++) {
        if (fabs(g->u[i] - value) <= tol) {
            count++;
        }
    }
    return count;
}

void grid_apply_bc(Grid *g, BoundaryKind kind, double value)
{
    if (kind == BC_DIRICHLET) {
        g->u[0] = value;
        g->u[g->n] = value;
    } else {
        g->u[g->n] = g->u[0];
    }
}