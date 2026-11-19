#include <math.h>
#include "gauss_fit.h"

#define GF_PI 3.14159265358979323846
#define GF_MAX_POINTS 256
#define GF_MAX_ITER 200

double gauss_model(double x, double m, double sigma, double amp)
{
    double z;

    if (sigma <= 0.0) {
        return 0.0;
    }
    z = (x - m) / sigma;
    return amp / (sigma * sqrt(2.0 * GF_PI)) * exp(-0.5 * z * z);
}

static double sse_of(const double *x, const double *y, int n,
                     const double *p)
{
    int i;
    double s = 0.0;

    for (i = 0; i < n; i++) {
        double r = y[i] - gauss_model(x[i], p[0], p[1], p[2]);
        s += r * r;
    }
    return s;
}

/*
 * 解 n x n 线性方程组 A x = b，列主元高斯消去。
 * A 和 b 会被就地修改。返回 0 表示矩阵奇异。
 */
static int solve_linear(double a[3][3], double b[3], int n, double x[3])
{
    int i;
    int j;
    int k;
    int piv;

    for (k = 0; k < n; k++) {
        piv = k;
        for (i = k + 1; i < n; i++) {
            if (fabs(a[i][k]) > fabs(a[piv][k])) {
                piv = i;
            }
        }
        if (fabs(a[piv][k]) < 1e-14) {
            return 0;
        }
        if (piv != k) {
            for (j = k; j < n; j++) {
                double tmp = a[k][j];
                a[k][j] = a[piv][j];
                a[piv][j] = tmp;
            }
            {
                double tmp = b[k];
                b[k] = b[piv];
                b[piv] = tmp;
            }
        }
        for (i = k + 1; i < n; i++) {
            double f = a[i][k] / a[k][k];
            for (j = k; j < n; j++) {
                a[i][j] -= f * a[k][j];
            }
            b[i] -= f * b[k];
        }
    }

    for (i = n - 1; i >= 0; i--) {
        double s = b[i];
        for (j = i + 1; j < n; j++) {
            s -= a[i][j] * x[j];
        }
        x[i] = s / a[i][i];
    }
    return 1;
}

int gauss_fit(const double *x, const double *y, int n,
              double amp_init, int fix_amp, GaussFit *out)
{
    double p[3];
    double pnew[3];
    double r[GF_MAX_POINTS];
    double jac[GF_MAX_POINTS][3];
    double a[3][3];
    double b[3];
    double delta[3];
    double lambda = 1e-3;
    double sse;
    double sse_new;
    double sw = 0.0;
    double sm = 0.0;
    double sv = 0.0;
    double max_res = 0.0;
    int np;
    int i;
    int j;
    int k;
    int iter;
    int accepted;
    int done = 0;

    if (n <= 0 || n > GF_MAX_POINTS || amp_init <= 0.0) {
        return 0;
    }
    np = fix_amp ? 2 : 3;

    /* 初值：分箱计数加权的均值和标准差 */
    for (i = 0; i < n; i++) {
        sw += y[i];
        sm += x[i] * y[i];
    }
    if (sw <= 0.0) {
        return 0;
    }
    p[0] = sm / sw;
    for (i = 0; i < n; i++) {
        double d = x[i] - p[0];
        sv += y[i] * d * d;
    }
    p[1] = sqrt(sv / sw);
    if (p[1] < 1e-6) {
        p[1] = 1e-6;
    }
    p[2] = amp_init;

    sse = sse_of(x, y, n, p);

    for (iter = 0; iter < GF_MAX_ITER; iter++) {
        /* 组装正规方程 J^T J 和 J^T r */
        for (k = 0; k < 3; k++) {
            for (j = 0; j < 3; j++) {
                a[k][j] = 0.0;
            }
            b[k] = 0.0;
        }

        for (i = 0; i < n; i++) {
            double z = (x[i] - p[0]) / p[1];
            double e = exp(-0.5 * z * z);
            double base = p[2] / (p[1] * sqrt(2.0 * GF_PI)) * e;

            r[i] = y[i] - base;
            jac[i][0] = base * z / p[1];
            jac[i][1] = base * (z * z - 1.0) / p[1];
            jac[i][2] = base / p[2];
        }

        for (i = 0; i < n; i++) {
            for (k = 0; k < np; k++) {
                b[k] += jac[i][k] * r[i];
                for (j = 0; j < np; j++) {
                    a[k][j] += jac[i][k] * jac[i][j];
                }
            }
        }

        accepted = 0;
        while (!accepted) {
            double mat[3][3];
            double rhs[3];

            for (k = 0; k < np; k++) {
                for (j = 0; j < np; j++) {
                    mat[k][j] = a[k][j];
                }
                mat[k][k] *= (1.0 + lambda);
                rhs[k] = b[k];
            }
            if (!solve_linear(mat, rhs, np, delta)) {
                return 0;
            }

            for (k = 0; k < 3; k++) {
                pnew[k] = p[k];
            }
            for (k = 0; k < np; k++) {
                pnew[k] = p[k] + delta[k];
            }

            if (pnew[1] <= 1e-8 || (!fix_amp && pnew[2] <= 0.0)) {
                lambda *= 10.0;
                if (lambda > 1e12) {
                    accepted = 1;
                }
                continue;
            }

            sse_new = sse_of(x, y, n, pnew);
            if (sse_new < sse) {
                double step = 0.0;
                for (k = 0; k < np; k++) {
                    step += delta[k] * delta[k];
                }
                for (k = 0; k < 3; k++) {
                    p[k] = pnew[k];
                }
                sse = sse_new;
                lambda *= 0.3;
                if (lambda < 1e-12) {
                    lambda = 1e-12;
                }
                accepted = 1;
                if (sqrt(step) < 1e-13) {
                    done = 1;
                }
            } else {
                lambda *= 10.0;
                if (lambda > 1e12) {
                    accepted = 1;
                }
            }
        }
        if (done || lambda > 1e12) {
            break;
        }
    }

    for (i = 0; i < n; i++) {
        double res = fabs(y[i] - gauss_model(x[i], p[0], p[1], p[2]));
        if (res > max_res) {
            max_res = res;
        }
    }

    out->m = p[0];
    out->sigma = p[1];
    out->amp = p[2];
    out->sse = sse;
    out->max_abs_res = max_res;
    out->iterations = iter;
    return 1;
}
