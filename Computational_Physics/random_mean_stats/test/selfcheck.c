#include <stdio.h>
#include <math.h>

#include "irwin_hall.h"
#include "lagrange.h"
#include "gauss_fit.h"

#define PI 3.14159265358979323846
#define NB 10

static int g_fail = 0;

static void check(const char *name, double got, double want, double tol)
{
    double err = fabs(got - want);
    int ok = (err <= tol);

    if (!ok) {
        g_fail = 1;
    }
    printf("  %-40s got=% .12e want=% .12e err=%.3e tol=%.1e %s\n",
           name, got, want, err, tol, ok ? "PASS" : "FAIL");
}

/* 单元级：Irwin-Hall 的 pdf/cdf 与矩必须自洽 */
static void check_irwin_hall(void)
{
    long n = 2000000L;
    long i;
    double h = 1.0 / (double)n;
    double m0 = 0.0, m1 = 0.0, m2 = 0.0;
    double mean, worst = 0.0;

    for (i = 0; i < n; i++) {
        double x = ((double)i + 0.5) * h;
        double f = ih3_pdf_mean(x);
        m0 += f;
        m1 += x * f;
        m2 += x * x * f;
    }
    m0 *= h;
    m1 *= h;
    m2 *= h;
    mean = m1 / m0;

    printf("== 自检 1: Irwin-Hall 精确分布 ==\n");
    check("integral of pdf", m0, 1.0, 1e-9);
    check("mean from pdf", mean, 0.5, 1e-9);
    check("variance from pdf", m2 / m0 - mean * mean, 1.0 / 36.0, 1e-9);
    check("cdf(0.4)", ih3_cdf_mean(0.4), 0.284, 1e-12);
    check("cdf(0.5)", ih3_cdf_mean(0.5), 0.5, 1e-12);
    check("cdf(0.9)", ih3_cdf_mean(0.9), 0.9955, 1e-12);

    for (i = 1; i < 999; i++) {
        double x = (double)i / 1000.0;
        double dh = 1e-5;
        double dF = (ih3_cdf_mean(x + dh) - ih3_cdf_mean(x - dh)) / (2.0 * dh);
        double f = ih3_pdf_mean(x);
        /* 用绝对误差：pdf 在两端趋于 0，相对误差会被无意义地放大 */
        double err = fabs(dF - f);
        if (err > worst) {
            worst = err;
        }
    }
    check("max |d(cdf)/dx - pdf|", worst, 0.0, 1e-8);
    printf("\n");
}

/* 单元级：Lagrange 基函数应满足单位分解与 Kronecker delta */
static void check_lagrange(void)
{
    double x[NB], ones[NB], basis[NB], c[NB];
    double max_unity = 0.0, max_node = 0.0, max_coeff = 0.0, max_kron = 0.0;
    int i, j, k;

    for (i = 0; i < NB; i++) {
        x[i] = 0.05 + 0.1 * (double)i;
        ones[i] = 1.0;
    }
    lagrange_coeffs(x, ones, NB, c);

    for (i = 0; i <= 1000; i++) {
        double t = (double)i / 1000.0;
        double d = fabs(poly_eval(c, NB, t) - 1.0);
        if (d > max_unity) {
            max_unity = d;
        }
    }
    for (j = 0; j < NB; j++) {
        double d = fabs(poly_eval(c, NB, x[j]) - 1.0);
        if (d > max_node) {
            max_node = d;
        }
    }
    for (k = 1; k < NB; k++) {
        double d = fabs(c[k]);
        if (d > max_coeff) {
            max_coeff = d;
        }
    }

    /* 第 i 个基函数：y 取单位向量，节点上应给出 delta_ij */
    for (i = 0; i < NB; i++) {
        for (j = 0; j < NB; j++) {
            double want = (i == j) ? 1.0 : 0.0;
            double d;
            for (k = 0; k < NB; k++) {
                basis[k] = (k == i) ? 1.0 : 0.0;
            }
            d = fabs(lagrange_eval(x, basis, NB, x[j]) - want);
            if (d > max_kron) {
                max_kron = d;
            }
        }
    }

    printf("== 自检 2: Lagrange 基函数（y 恒为 1）==\n");
    check("max |P(x)-1| over 1001 pts", max_unity, 0.0, 1e-8);
    /* 这一条走的是系数展开 + Horner，1e-10 量级正是单项式基条件数的体现 */
    check("max |P(x_i)-1| nodes (coefficient path)", max_node, 0.0, 1e-8);
    check("c0 of the constant poly", c[0], 1.0, 1e-9);
    /* 常数多项式的高次系数理论为 0，残差来自展开过程中的相消 */
    check("max |c_k| for k >= 1", max_coeff, 0.0, 1e-6);
    check("max |L_i(x_j) - delta_ij|", max_kron, 0.0, 1e-12);
    printf("\n");
}

/* 单元级：用已知参数造精确数据，拟合必须原样返回 */
static void check_gauss(void)
{
    double x[NB], y[NB];
    double s = 1.0 / 6.0, A = 100.0, m0 = 0.5;
    GaussFit fit;
    int i;

    for (i = 0; i < NB; i++) {
        x[i] = 0.05 + 0.1 * (double)i;
        y[i] = gauss_model(x[i], m0, s, A);
    }

    printf("== 自检 3: 高斯拟合参数回代 ==\n");
    if (!gauss_fit(x, y, NB, A, 1, &fit)) {
        printf("  gauss_fit (A fixed) FAILED to run\n");
        g_fail = 1;
    } else {
        check("recovered m (A fixed)", fit.m, m0, 1e-6);
        check("recovered sigma (A fixed)", fit.sigma, s, 1e-6);
        check("SSE at the true parameters", fit.sse, 0.0, 1e-10);
    }
    if (!gauss_fit(x, y, NB, A, 0, &fit)) {
        printf("  gauss_fit (A free) FAILED to run\n");
        g_fail = 1;
    } else {
        check("recovered m (A free)", fit.m, m0, 1e-6);
        check("recovered sigma (A free)", fit.sigma, s, 1e-6);
        check("recovered A (A free)", fit.amp, A, 1e-6);
    }

    /* 解析雅可比 vs 中心差分 */
    {
        double xx = 0.62, mm = 0.51, ss = 0.17, aa = 97.0, dh = 1e-6;
        double f = gauss_model(xx, mm, ss, aa);
        double z = (xx - mm) / ss;
        double num_m, num_s, num_a;

        num_m = (gauss_model(xx, mm + dh, ss, aa) -
                 gauss_model(xx, mm - dh, ss, aa)) / (2.0 * dh);
        num_s = (gauss_model(xx, mm, ss + dh, aa) -
                 gauss_model(xx, mm, ss - dh, aa)) / (2.0 * dh);
        num_a = (gauss_model(xx, mm, ss, aa + dh) -
                 gauss_model(xx, mm, ss, aa - dh)) / (2.0 * dh);

        printf("\n  -- 拟合器内部：解析导数 vs 数值差分 --\n");
        check("df/dm", f * z / ss, num_m, 1e-5 * fabs(num_m));
        check("df/dsigma", f * (z * z - 1.0) / ss, num_s, 1e-5 * fabs(num_s));
        check("df/dA", f / aa, num_a, 1e-5 * fabs(num_a));
    }
    printf("\n");
}

int main(void)
{
    check_irwin_hall();
    check_lagrange();
    check_gauss();

    if (g_fail) {
        printf("SELFTEST: FAIL\n");
        return 1;
    }
    printf("SELFTEST: ALL PASS\n");
    return 0;
}
