#include "lagrange.h"

void lagrange_coeffs(const double *x, const double *y, int n, double *c)
{
    int i;
    int j;
    int k;
    int deg;
    double num[LAGRANGE_MAX_N];
    double den;

    for (k = 0; k < n; k++) {
        c[k] = 0.0;
    }

    for (i = 0; i < n; i++) {
        /* 从常数 1 开始，逐个乘以 (x - x_j) 得到第 i 个基函数的分子 */
        for (k = 0; k < n; k++) {
            num[k] = 0.0;
        }
        num[0] = 1.0;
        deg = 0;
        den = 1.0;

        for (j = 0; j < n; j++) {
            if (j == i) {
                continue;
            }
            /* num <- num * (x - x[j])，从高次往低次写，避免覆盖 */
            for (k = deg + 1; k >= 1; k--) {
                num[k] = num[k - 1] - x[j] * num[k];
            }
            num[0] = -x[j] * num[0];
            deg++;
            den *= (x[i] - x[j]);
        }

        for (k = 0; k < n; k++) {
            c[k] += y[i] * num[k] / den;
        }
    }
}

double poly_eval(const double *c, int n, double x)
{
    int k;
    double s = 0.0;

    for (k = n - 1; k >= 0; k--) {
        s = s * x + c[k];
    }
    return s;
}

double lagrange_eval(const double *x, const double *y, int n, double t)
{
    int i;
    int j;
    double s = 0.0;

    for (i = 0; i < n; i++) {
        double l = 1.0;
        for (j = 0; j < n; j++) {
            if (j != i) {
                l *= (t - x[j]) / (x[i] - x[j]);
            }
        }
        s += y[i] * l;
    }
    return s;
}
