/*
 * 第二级证据：C 单元自检。
 *
 * 与 main.c 末尾那套「程序内实测」的差别在于：这里每一项的期望答案都是
 * 手工算出来或从定义直接推出来的，不依赖被测模块自己。特别是 T13 用测试
 * 文件里自带的稠密高斯消元去解 CN 的线性方程组，和 crank.c 的
 * Thomas + Sherman-Morrison 走完全不同的路径，两者必须给出同一个解。
 *
 * 编译：见 test/run.sh 第 2 步。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "material.h"
#include "grid.h"
#include "solver.h"
#include "crank.h"
#include "analytic.h"
#include "converge.h"
#include "util.h"

static long checks_run = 0;
static long checks_failed = 0;

/* 相对判等：0.045 附近的减法误差本就有几个 ulp，绝对容差不能瞎拍 */
static int close_rel(double a, double b, double tol)
{
    double m = fabs(a);

    if (fabs(b) > m) {
        m = fabs(b);
    }
    if (m < 1.0) {
        m = 1.0;
    }
    return fabs(a - b) <= tol * m;
}

static void ck(const char *id, const char *what, int ok, const char *detail)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
    }
    printf("  [%s] %-4s %-48s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/* ---------- 独立求解器：稠密高斯消元（只为 T13 的交叉核对） ---------- */

static void dense_solve(double *a, double *b, long n)
{
    long i;
    long j;
    long k;
    double piv;
    double f;

    for (k = 0; k < n; k++) {
        piv = a[k * n + k];
        for (j = k; j < n; j++) {
            a[k * n + j] /= piv;
        }
        b[k] /= piv;
        for (i = 0; i < n; i++) {
            if (i == k) {
                continue;
            }
            f = a[i * n + k];
            if (f == 0.0) {
                continue;
            }
            for (j = k; j < n; j++) {
                a[i * n + j] -= f * a[k * n + j];
            }
            b[i] -= f * b[k];
        }
    }
    /* 解已就地放在 b 里 */
}

/* 按 CN 的定义拼出稠密矩阵并求解，返回与 crank 一步之后结果的差 */
static double cn_dense_vs_module(double dx, double dt, int periodic)
{
    Grid g;
    Crank cs;
    double r;
    long n;
    long m;
    long i;
    long j;
    double *a;
    double *b;
    double worst;
    double d;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx) != 0) {
        return -1.0;
    }
    n = g.n;
    /* 一个不规则的初值场，避免任何巧合 */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = 37.0 * sin(2.1 * (double)i) + 11.0 * cos(0.7 * (double)i);
    }
    if (periodic) {
        g.u[n] = g.u[0];
        m = n;
    } else {
        g.u[0] = 0.0;
        g.u[n] = 0.0;
        m = n - 1;
    }
    r = material_alpha() * dt / (g.dx * g.dx);

    a = (double *)malloc((size_t)(m * m) * sizeof(double));
    b = (double *)malloc((size_t)m * sizeof(double));
    if (a == NULL || b == NULL) {
        free(a);
        free(b);
        grid_free(&g);
        return -1.0;
    }
    for (i = 0; i < m * m; i++) {
        a[i] = 0.0;
    }
    for (i = 0; i < m; i++) {
        a[i * m + i] = 1.0 + r;
    }
    for (i = 0; i + 1 < m; i++) {
        a[i * m + (i + 1)] = -0.5 * r;
        a[(i + 1) * m + i] = -0.5 * r;
    }
    if (periodic) {
        a[0 * m + (m - 1)] = -0.5 * r;
        a[(m - 1) * m + 0] = -0.5 * r;
    }
    /* 右端 = (I + r/2 L) u^n */
    for (i = 0; i < m; i++) {
        long gi = periodic ? i : i + 1;
        long ip;
        long im;

        if (periodic) {
            ip = (gi == n - 1) ? 0 : gi + 1;
            im = (gi == 0) ? n - 1 : gi - 1;
        } else {
            ip = gi + 1;
            im = gi - 1;
        }
        b[i] = 0.5 * r * g.u[im] + (1.0 - r) * g.u[gi] + 0.5 * r * g.u[ip];
    }
    dense_solve(a, b, m);

    /* 模块路径 */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = 37.0 * sin(2.1 * (double)i) + 11.0 * cos(0.7 * (double)i);
    }
    if (periodic) {
        g.u[n] = g.u[0];
    } else {
        g.u[0] = 0.0;
        g.u[n] = 0.0;
    }
    if (crank_init(&cs, &g, periodic ? BC_PERIODIC : BC_DIRICHLET,
                   0.0, material_alpha(), dt) != 0) {
        free(a);
        free(b);
        grid_free(&g);
        return -1.0;
    }
    crank_step(&cs);
    crank_free(&cs);

    worst = 0.0;
    for (i = 0; i < m; i++) {
        j = periodic ? i : i + 1;
        d = fabs(g.u[j] - b[i]);
        if (d > worst) {
            worst = d;
        }
    }
    free(a);
    free(b);
    grid_free(&g);
    return worst;
}

int main(void)
{
    char buf[224];
    double v;
    double dx;
    double len;
    double lo;
    double hi;
    double hist[6];
    long n;

    printf("== C 单元自检（期望答案全部手工算出，不调用被测模块）==\n");

    /* ---------- 有限体积重叠 ---------- */
    dx = 0.002;
    len = 0.1;
    lo = 0.045;
    hi = 0.055;
    v = grid_cell_overlap(0.044, dx, lo, hi, len, 0);
    sprintf(buf, "x=0.044 单元[0.043,0.045] 重叠 = %.17g（期望 0）", v);
    ck("T1", "单元完全在左：重叠为 0", v == 0.0, buf);

    v = grid_cell_overlap(0.046, dx, lo, hi, len, 0);
    sprintf(buf, "x=0.046 单元[0.045,0.047] 重叠 = %.17g（期望 0.002）", v);
    ck("T2", "单元完全在内：重叠为 dx", close_rel(v, 0.002, 1e-12), buf);

    v = grid_cell_overlap(0.045, dx, lo, hi, len, 0);
    sprintf(buf, "x=0.045 单元[0.044,0.046] 重叠 = %.17g（期望 0.001）", v);
    ck("T3", "单元跨左边界：重叠为 dx/2", close_rel(v, 0.001, 1e-12), buf);

    v = grid_cell_overlap(0.056, dx, lo, hi, len, 0);
    sprintf(buf, "x=0.056 单元[0.055,0.057] 重叠 = %.17g（期望 0）", v);
    ck("T4", "单元跨右边界外侧：重叠为 0", v == 0.0, buf);

    /*
     * 跨界回绕：圆上的高温段 [0.09, 0.11] 平移一份变成 [-0.01, 0.01]，
     * 宽度仍是 0.02。x=0 的单元是 [-0.001, 0.001]，整格都在这份平移里，
     * 所以重叠是 0.002 而不是 0.001——第一版手算把它当成「只收到一半」，
     * 是错的。这条检查纠正的正是这个直觉。
     */
    v = grid_cell_overlap(0.0, dx, 0.09, 0.11, len, 1);
    sprintf(buf, "x=0 单元[-0.001,0.001] 整格落入平移段 = %.17g（期望 0.002）", v);
    ck("T5", "周期回绕：x=0 的单元收到 wrap 部分", close_rel(v, 0.002, 1e-12), buf);

    v = grid_cell_overlap(0.099, dx, 0.09, 0.11, len, 1);
    sprintf(buf, "x=0.099 单元[0.098,0.100] 与 [0.09,0.11] 重叠 = %.17g（期望 0.002）", v);
    ck("T6", "周期回绕：右端整格在内", close_rel(v, 0.002, 1e-12), buf);

    /* 半格落入平移段：单元 [-0.005, -0.001] 对应 x = -0.003，平移段只覆盖到 -0.001 */
    v = grid_cell_overlap(len - 0.003, dx, 0.09, 0.11, len, 1);
    sprintf(buf, "x=0.097 单元[0.096,0.098] 与 [0.09,0.11] 重叠 = %.17g（期望 0.002）", v);
    ck("T5b", "回绕不改变未跨界单元的重叠", close_rel(v, 0.002, 1e-12), buf);

    /* ---------- 控制体宽度 ---------- */
    {
        Grid g;
        double wd[3];

        memset(&g, 0, sizeof(Grid));
        if (grid_init(&g, 0.1, 0.02) != 0) {
            printf("grid_init failed\n");
            return 1;
        }
        wd[0] = grid_cell_width(&g, 0, 0);
        wd[1] = grid_cell_width(&g, 1, 0);
        wd[2] = grid_cell_width(&g, g.n, 0);
        sprintf(buf, "端点 %.17g / 内部 %.17g / 端点 %.17g（期望 0.01, 0.02, 0.01）",
                wd[0], wd[1], wd[2]);
        ck("T7", "Dirichlet 端节点控制体是半格",
           wd[0] == 0.01 && wd[1] == 0.02 && wd[2] == 0.01, buf);
        v = grid_cell_width(&g, 0, 1);
        sprintf(buf, "周期 x=0 宽度 = %.17g（期望整格 0.02）", v);
        ck("T8", "周期端节点控制体是整格", v == 0.02, buf);
        grid_free(&g);
    }

    /* ---------- 守恒量与对称性 ---------- */
    {
        Grid g;
        double h1;
        double h2;
        double mean;

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, 0.1, 0.02);
        for (n = 0; n < g.nn; n++) {
            g.u[n] = 100.0;
        }
        h1 = grid_heat_dirichlet(&g);
        h2 = grid_heat_periodic(&g);
        mean = grid_mean_periodic(&g);
        /* 常值 100 的场：热量 = 100 * l = 10，均值 = 热量 / l = 100（就是原值） */
        sprintf(buf, "梯形 %.17g / 周期 %.17g / 均值 %.17g（热量应为 10，均值应为 100）",
                h1, h2, mean);
        ck("T9", "常值场：两种热量口径一致，均值还原为场值",
           fabs(h1 - 10.0) < 1e-14 && fabs(h2 - 10.0) < 1e-14
               && fabs(mean - 100.0) < 1e-14, buf);

        for (n = 0; n < g.nn; n++) {
            g.u[n] = (double)n;
        }
        v = grid_symmetry_defect(&g);
        sprintf(buf, "u_i = i 的对称性偏差 = %.17g（期望 n = %ld）", v, g.n);
        ck("T10", "对称性偏差的定义式", fabs(v - (double)g.n) < 1e-12, buf);

        for (n = 0; n < g.nn; n++) {
            g.u[n] = (n % 3 == 0) ? 5.0 : 0.0;
        }
        n = grid_count_equal(&g, 5.0, 1e-12);
        sprintf(buf, "每 3 个一格取 5，计数 = %ld（期望 %ld）",
                n, (long)(g.n / 3) + 1);
        ck("T11", "grid_count_equal 的计数", n == (long)(g.n / 3) + 1, buf);
        grid_free(&g);
    }

    /* ---------- FTCS 单步：手工期望值 ---------- */
    {
        Grid g;
        Ftcs fs;
        double exp1[5];

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, 0.1, 0.025); /* n = 4 */
        g.u[0] = 0.0;
        g.u[1] = 1.0;
        g.u[2] = 0.0;
        g.u[3] = 0.0;
        g.u[4] = 0.0;
        if (ftcs_init(&fs, &g, BC_DIRICHLET, 0.0, material_alpha(), 0.0) == 0) {
            ftcs_free(&fs);
        }
        /* 直接按公式手算：r = 0.25 */
        g.dx = 0.025;
        exp1[0] = 0.0;
        exp1[1] = 1.0 + 0.25 * (0.0 - 2.0 * 1.0 + 0.0);
        exp1[2] = 0.0 + 0.25 * (0.0 - 2.0 * 0.0 + 1.0);
        exp1[3] = 0.0 + 0.25 * (0.0 - 2.0 * 0.0 + 0.0);
        exp1[4] = 0.0;
        if (ftcs_init(&fs, &g, BC_DIRICHLET, 0.0,
                      0.25 * g.dx * g.dx / 0.01, 0.01) != 0) {
            printf("ftcs_init failed\n");
            return 1;
        }
        ftcs_step(&fs);
        v = 0.0;
        for (n = 0; n < g.nn; n++) {
            if (fabs(g.u[n] - exp1[n]) > v) {
                v = fabs(g.u[n] - exp1[n]);
            }
        }
        sprintf(buf, "r=0.25 尖峰场一步后与手算值最大差 %.3e", v);
        ck("T12", "FTCS 单步 = 手工代入差分公式", v < 1e-15, buf);
        ftcs_free(&fs);
        grid_free(&g);
    }

    {
        Grid g;
        Ftcs fs;
        double e[5];

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, 0.1, 0.025); /* n = 4，周期未知量 0..3 */
        g.u[0] = 1.0;
        g.u[1] = 0.0;
        g.u[2] = 0.0;
        g.u[3] = 0.0;
        g.u[4] = 1.0;
        if (ftcs_init(&fs, &g, BC_PERIODIC, 0.0,
                      0.25 * g.dx * g.dx / 0.01, 0.01) != 0) {
            printf("ftcs_init failed\n");
            return 1;
        }
        ftcs_step(&fs);
        /* u_0' = 1 + .25(0 - 2 + 0) = 0.5；u_1' = .25(0-0+1) = 0.25
           u_2' = .25(0-0+0) = 0；u_3' = .25(1-0+0) = 0.25；u_4' = u_0' */
        e[0] = 0.5;
        e[1] = 0.25;
        e[2] = 0.0;
        e[3] = 0.25;
        e[4] = 0.5;
        v = 0.0;
        for (n = 0; n < g.nn; n++) {
            if (fabs(g.u[n] - e[n]) > v) {
                v = fabs(g.u[n] - e[n]);
            }
        }
        sprintf(buf, "回绕一步与手算最大差 %.3e", v);
        ck("T13", "FTCS 周期回绕取的是 i-1 = n-1、i+1 = 0", v < 1e-15, buf);
        ftcs_free(&fs);
        grid_free(&g);
    }

    /* ---------- 离散放大因子的精确值 ---------- */
    /*
     * Nyquist 模：q dx/2 = pi/2，sin^2 = 1。
     * FTCS 的 G = 1 - 4r：r = 1/4 才归零，r = 1/2 时 G = -1，
     * 也就是稳定极限上最高频模**不衰减、只每步翻号**（|G| = 1，临界）。
     * 第一版把「极限处归零」记成了 r = 1/2，是算错。
     */
    v = ftcs_growth_factor(0.25, HD_PI / 0.02, 0.02);
    sprintf(buf, "FTCS G(r=1/4, Nyquist) = %.17g（期望 0）", v);
    ck("T14", "FTCS 的 Nyquist 模在 r=1/4 处一步归零", v == 0.0, buf);

    v = ftcs_growth_factor(0.5, HD_PI / 0.02, 0.02);
    sprintf(buf, "FTCS G(r=1/2, Nyquist) = %.17g（期望 -1，|G|=1 临界）", v);
    ck("T14b", "FTCS 在稳定极限处 Nyquist 模不衰减只翻号", v == -1.0, buf);

    v = ftcs_growth_factor(0.75, HD_PI / 0.02, 0.02);
    sprintf(buf, "FTCS G(r=3/4, Nyquist) = %.17g（期望 -2）", v);
    ck("T15", "FTCS r>1/2 时 Nyquist 模 |G|>1", fabs(v + 2.0) < 1e-15, buf);

    /* CN 的 G = (1-2r)/(1+2r) 在 Nyquist 处：r=1/2 给 0，r=1 给 -1/3 */
    v = crank_growth_factor(0.5, HD_PI / 0.02, 0.02);
    sprintf(buf, "CN G(r=1/2, Nyquist) = %.17g（期望 0）", v);
    ck("T16", "CN 在 Nyquist 模的精确放大因子（r=1/2）", v == 0.0, buf);

    v = crank_growth_factor(1.0, HD_PI / 0.02, 0.02);
    sprintf(buf, "CN G(r=1, Nyquist) = %.17g（期望 -1/3）", v);
    ck("T16b", "CN 在 Nyquist 模的精确放大因子（r=1）",
       close_rel(v, -1.0 / 3.0, 1e-14), buf);

    {
        long k;
        long bad;

        bad = 0;
        for (k = 0; k < 400; k++) {
            double rr = pow(10.0, -2.0 + 8.0 * (double)k / 399.0); /* 1e-2..1e6 */
            double q = HD_PI / 0.02 * (double)((k % 50) + 1) / 50.0;
            double gg = crank_growth_factor(rr, q, 0.02);

            if (fabs(gg) > 1.0 + 1e-15) {
                bad++;
            }
        }
        sprintf(buf, "r 从 1e-2 到 1e6、q 扫 50 档，|G|>1 的个数 = %ld", bad);
        ck("T17", "CN 无条件稳定：|G| <= 1", bad == 0, buf);
    }

    /* ---------- CN 线性求解：与稠密高斯消元交叉 ---------- */
    v = cn_dense_vs_module(0.001, 0.01, 0);
    sprintf(buf, "Dirichlet 一步，模块 vs 稠密消元 max 差 %.3e", v);
    ck("T18", "Thomas 追赶法解三对角正确", v >= 0.0 && v < 1e-12, buf);

    v = cn_dense_vs_module(0.001, 0.01, 1);
    sprintf(buf, "周期一步，模块 vs 稠密消元 max 差 %.3e", v);
    ck("T19", "Sherman-Morrison 解循环三对角正确", v >= 0.0 && v < 1e-12, buf);

    v = cn_dense_vs_module(0.002, 0.25, 1);
    sprintf(buf, "周期 r=28.75 大步长，max 差 %.3e", v);
    ck("T20", "循环求解在大 r 下仍正确", v >= 0.0 && v < 1e-11, buf);

    /* CN 对周期常值场必须原样不动（L·const = 0，A 的行和为 1） */
    {
        Grid g;
        Crank cs;
        long step;

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, 0.1, 0.001);
        for (n = 0; n < g.nn; n++) {
            g.u[n] = 7.5;
        }
        if (crank_init(&cs, &g, BC_PERIODIC, 0.0, material_alpha(), 0.01) != 0) {
            return 1;
        }
        for (step = 0; step < 50; step++) {
            crank_step(&cs);
        }
        v = 0.0;
        for (n = 0; n < g.nn; n++) {
            if (fabs(g.u[n] - 7.5) > v) {
                v = fabs(g.u[n] - 7.5);
            }
        }
        sprintf(buf, "50 步后 max|u - 7.5| = %.3e", v);
        ck("T21", "CN 周期常值场是不动点", v < 1e-13, buf);
        crank_free(&cs);
        grid_free(&g);
    }

    /* ---------- 解析解 ---------- */
    v = fabs(analytic_u(0.0, 12.34, 0, 0))
        + fabs(analytic_u(ROD_LENGTH, 12.34, 0, 0));
    sprintf(buf, "u(0,t) + u(l,t) = %.3e（Dirichlet 应恒为 0）", v);
    ck("T22", "解析解满足 Dirichlet 边界", v < 1e-13, buf);

    v = fabs(analytic_u(0.0, 3.21, 0, 1) - analytic_u(ROD_LENGTH, 3.21, 0, 1));
    sprintf(buf, "u(0,t) - u(l,t) = %.3e（周期是同一点）", v);
    ck("T23", "解析解满足周期性识别", v < 1e-13, buf);

    /* t=0 的级数：内部 100、外部 0、跳跃点 50 */
    v = analytic_u(ROD_LENGTH / 2.0, 0.0, 200000, 0);
    sprintf(buf, "u(0.05, 0) = %.6f（期望 100，K=2e5）", v);
    ck("T24", "t=0 级数在脉冲内部趋于 100", fabs(v - 100.0) < 0.02, buf);

    v = analytic_u(0.02, 0.0, 200000, 0);
    sprintf(buf, "u(0.02, 0) = %.6f（期望 0）", v);
    ck("T25", "t=0 级数在脉冲外部趋于 0", fabs(v) < 0.02, buf);

    v = analytic_u(HOT_LO, 0.0, 200000, 0);
    sprintf(buf, "u(0.045, 0) = %.6f（期望跳变中值 50）", v);
    ck("T26", "t=0 级数在跳跃点收敛到中值", fabs(v - 50.0) < 0.002, buf);

    /*
     * 周期解析解的积分：复合 Simpson 应回到总热量 1.0。
     * 项数与采样点都要省：级数每项都要算一次 cos，20000 点 x 200000 项
     * 是 4e9 次超越函数调用，单这一条就把自检拖到 89 秒。
     * 截断误差按 1/K 收敛，K=4000 时远小于这里 0.02 的容差。
     */
    {
        long j;
        long npts = 4000;
        long K = 4000;
        double h = ROD_LENGTH / (double)npts;
        double sum = analytic_u(0.0, 0.0, K, 1) + analytic_u(ROD_LENGTH, 0.0, K, 1);

        for (j = 1; j < npts; j++) {
            sum += ((j % 2) == 0 ? 2.0 : 4.0) * analytic_u(h * (double)j, 0.0, K, 1);
        }
        sum *= h / 3.0;
        sprintf(buf, "int u_per(x,0) dx = %.8f（期望 1.0，K=%ld）", sum, K);
        ck("T27", "周期解析解的积分 = 初始总热量", fabs(sum - 1.0) < 0.02, buf);
    }

    /* 自动项数：最后一项必须低于容差 */
    {
        long K = analytic_nmax(10.0, 1e-12, 0);
        double q = analytic_wavenumber(K + 1, 0);
        double term = fabs(analytic_b_dirichlet(K + 1)) * exp(-q * q * IRON_ALPHA * 10.0);

        sprintf(buf, "t=10 自动项数 %ld，第 %ld 项 = %.3e（需 < 1e-12）",
                K, K + 1, term);
        ck("T28", "analytic_nmax 的判据自洽", term < 1e-12, buf);
    }

    /* ---------- 收敛判据 ---------- */
    {
        double u[5];
        double p[5];

        u[0] = 1.0; u[1] = 2.0; u[2] = 3.0; u[3] = 0.5; u[4] = -1.0;
        v = converge_max_dev(u, 5, 2.0);
        sprintf(buf, "max|u-2| = %.17g（期望 3，来自 u_4 = -1）", v);
        ck("T29", "converge_max_dev 的定义式", v == 3.0, buf);

        p[0] = 1.0; p[1] = 2.0; p[2] = 3.0; p[3] = 0.5; p[4] = -1.0;
        u[0] = 1.5; u[1] = 2.0; u[2] = 2.2; u[3] = 0.5; u[4] = -1.0;
        v = converge_max_jump(p, u, 5);
        sprintf(buf, "max|u-p| = %.17g（期望 0.8，来自 u_2）", v);
        ck("T30", "converge_max_jump 的定义式", fabs(v - 0.8) < 1e-15, buf);

        v = converge_target(BC_DIRICHLET, (Grid *)NULL);
        sprintf(buf, "Dirichlet 终态 = %.17g（期望 0）", v);
        ck("T31", "Dirichlet 的 u_inf 是边界值 0", v == COLD_TEMP, buf);
    }

    /* converge_first_permanent 的下标语义 */
    hist[0] = 5.0; hist[1] = 1.0; hist[2] = 0.5;
    hist[3] = 2.0; hist[4] = 0.1; hist[5] = 0.05;
    n = converge_first_permanent(hist, 6, 0.1);
    sprintf(buf, "hist=[5,1,.5,2,.1,.05] thr=0.1 -> 步数 %ld（期望 5）", n);
    ck("T32", "从后往前找：中途跌破又弹回不算", n == 5, buf);

    n = converge_first_permanent(hist, 6, 10.0);
    sprintf(buf, "thr=10 从头就满足 -> %ld（期望 1）", n);
    ck("T33", "全程满足则返回第 1 步", n == 1, buf);

    n = converge_first_permanent(hist, 6, 0.01);
    sprintf(buf, "thr=0.01 最后一步仍超 -> %ld（期望 -1）", n);
    ck("T34", "从未永久满足则返回 -1", n == -1, buf);

    /* ---------- 材料层 ---------- */
    {
        double dtx;

        dx = 0.001;
        dtx = material_dt_max_ftcs(material_alpha(), dx);
        v = material_fourier_number(material_alpha(), dx, dtx);
        sprintf(buf, "r(dt_max) = %.17g（期望恰好 0.5）", v);
        ck("T35", "dt_max 定义与 r 定义互逆", v == 0.5, buf);

        v = material_dt_max_ftcs(material_alpha(), dx)
            * 2.0 * material_alpha() - dx * dx;
        sprintf(buf, "dt_max*2*alpha - dx^2 = %.3e", fabs(v));
        ck("T36", "dt_max = dx^2/(2 alpha) 的恒等式", fabs(v) < 1e-20, buf);
    }

    /* ---------- util_is_finite ---------- */
    {
        double inf = HUGE_VAL;
        double nan = 0.0 / 0.0;
        int a;
        int b;
        int c;

        a = util_is_finite(inf);
        b = util_is_finite(-inf);
        c = util_is_finite(nan);
        sprintf(buf, "inf=%d -inf=%d nan=%d（期望全 0），有限值=%d",
                a, b, c, util_is_finite(1.0));
        ck("T37", "util_is_finite 认出 NaN 与 ±inf",
           a == 0 && b == 0 && c == 0 && util_is_finite(1.0) == 1, buf);
    }

    printf("\n== C 单元自检：%ld 项，%ld 项失败 ==\n", checks_run, checks_failed);
    if (checks_failed != 0) {
        return 1;
    }
    printf("SELFCHECK: PASS\n");
    return 0;
}
