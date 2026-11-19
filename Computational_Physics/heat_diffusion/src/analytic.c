#include <math.h>

#include "config.h"
#include "analytic.h"

/* 级数项数硬上限：t=0 时按 1/k 收敛，靠这个上限兜住 */
#define ANALYTIC_KMAX 200000L

double analytic_wavenumber(long k, int periodic)
{
    double q;

    q = HD_PI * (double)k / ROD_LENGTH;
    return periodic ? 2.0 * q : q;
}

double analytic_decay_rate(long k, int periodic)
{
    double q;

    q = analytic_wavenumber(k, periodic);
    return IRON_ALPHA * q * q;
}

double analytic_b_dirichlet(long k)
{
    double q;

    q = analytic_wavenumber(k, 0); /* k pi / l */
    /* (2/l) * HOT_TEMP * int_lo^hi sin(q x) dx
       = (2 HOT_TEMP/(l q)) [cos(q lo) - cos(q hi)] */
    return (2.0 * HOT_TEMP / (ROD_LENGTH * q))
           * (cos(q * HOT_LO) - cos(q * HOT_HI));
}

double analytic_a_periodic(long k)
{
    double q;

    q = analytic_wavenumber(k, 1); /* 2 k pi / l，不是 k pi / l */
    /* (2/l) * HOT_TEMP * int_lo^hi cos(q x) dx
       = (2 HOT_TEMP/(l q)) [sin(q hi) - sin(q lo)]
       系数与波数必须用同一个 q，写错任何一个都会让偶数 k 恰好抵消、
       读数看起来只是"错位一格"。 */
    return (2.0 * HOT_TEMP / (ROD_LENGTH * q))
           * (sin(q * HOT_HI) - sin(q * HOT_LO));
}

double analytic_mean_periodic(void)
{
    return ((HOT_TEMP - COLD_TEMP) * HOT_WIDTH + COLD_TEMP * ROD_LENGTH)
           / ROD_LENGTH;
}

long analytic_nmax(double t, double tol, int periodic)
{
    double rate1;
    double amp;
    double term;
    double bound;
    long k;
    long last;

    if (t <= 0.0) {
        return ANALYTIC_KMAX; /* 没有指数压制，只能给上限 */
    }
    rate1 = analytic_decay_rate(1, periodic);
    bound = 2.0 * HOT_TEMP / HD_PI; /* |系数| <= 2*HOT_TEMP/(k*pi) */

    last = 1;
    for (k = 1; k <= ANALYTIC_KMAX; k++) {
        amp = bound / (double)k;
        term = amp * exp(-rate1 * (double)k * (double)k * t);
        if (term >= tol) {
            last = k;
        } else if (rate1 * (double)k * (double)k * t > 100.0) {
            break; /* 指数已经彻底压死，后面不会再翻上来 */
        }
    }
    if (last + 2 < ANALYTIC_KMAX) {
        last += 2; /* 余量 */
    }
    return last;
}

double analytic_u(double x, double t, long nmax, int periodic)
{
    double sum;
    double q;
    long k;

    if (nmax <= 0) {
        nmax = analytic_nmax(t, 1e-12, periodic);
    }
    if (nmax > ANALYTIC_KMAX) {
        nmax = ANALYTIC_KMAX;
    }

    sum = periodic ? analytic_mean_periodic() : 0.0;
    for (k = 1; k <= nmax; k++) {
        q = analytic_wavenumber(k, periodic);
        if (periodic) {
            sum += analytic_a_periodic(k) * cos(q * x)
                   * exp(-q * q * IRON_ALPHA * t);
        } else {
            sum += analytic_b_dirichlet(k) * sin(q * x)
                   * exp(-q * q * IRON_ALPHA * t);
        }
    }
    return sum;
}

double analytic_project_dirichlet(long k, long npts)
{
    double h;
    double s;
    double q;
    long j;

    q = analytic_wavenumber(k, 0);
    h = (HOT_HI - HOT_LO) / (double)npts;
    s = sin(q * HOT_LO) + sin(q * HOT_HI);
    for (j = 1; j < npts; j++) {
        s += ((j % 2) == 0 ? 2.0 : 4.0) * sin(q * (HOT_LO + (double)j * h));
    }
    s *= h / 3.0;
    return (2.0 / ROD_LENGTH) * HOT_TEMP * s;
}

double analytic_project_periodic(long k, long npts)
{
    double h;
    double s;
    double q;
    long j;

    q = analytic_wavenumber(k, 1);
    h = (HOT_HI - HOT_LO) / (double)npts;
    s = cos(q * HOT_LO) + cos(q * HOT_HI);
    for (j = 1; j < npts; j++) {
        s += ((j % 2) == 0 ? 2.0 : 4.0) * cos(q * (HOT_LO + (double)j * h));
    }
    s *= h / 3.0;
    return (2.0 / ROD_LENGTH) * HOT_TEMP * s;
}