/* =====================================================================
 * 随机数统计与分布拟合 —— 单文件版（C89）
 *
 * 本文件由 make_single_source.py 从 include/ + src/ 自动合并生成，
 * 与模块化版本（正式交付）内容对应，只是并成一个翻译单元，
 * 方便只拷一个 .c 到别的机器上编译运行。
 *
 * 编译：  gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 \
 *             random_mean_stats_single.c -o rms -lm
 * 运行：  ./rms [输出目录] [-n 样本量] [--seed 种子]
 * ===================================================================== */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/* ---------- config.h ---------- */

/* 题目参数：每个 x_i 由 3 个 [0,1) 均匀随机数取算术平均 */
#define N_UNIFORM 3

/* 重复次数 N = 1000 */
#define DEFAULT_N_SAMPLES 1000L

/* 固定种子：任何人重跑都得到同一组数据，方便核对 */
#define RNG_DEFAULT_SEED 20261024UL


/* ---------- rng.h ---------- */

/*
 * 均匀随机数：直接包一层 C89 的 rand()。
 *
 * srand(seed) 固定种子后，本机上每次运行得到同一串数，报告里印的数字
 * 与 data 目录里的 .dat 一一对应；但 C89 只规定 rand() 的取值范围（RAND_MAX >= 32767），
 * 不规定序列，所以换 libc（glibc / MinGW）会得到另一串数。统计结论不受影响
 * （见 test/rng_sensitivity.c 的对照），具体计数以提交的 data 目录下的数据文件为准。
 *
 * rand() 的精度可能只有 15 位，所以每次用两个 rand() 拼出约 30 位小数，
 * 免得采样结果的网格粗到影响分箱与拟合。
 */
void rng_seed(unsigned long seed);

/* 返回 [0, 1) 上的均匀随机数 */
double rng_uniform(void);


/* ---------- sampling.h ---------- */

/*
 * 生成 n 个样本 x_i，每个 x_i 是 k 个 [0,1) 均匀随机数的算术平均。
 * 采样与随机数发生器分离：换分布只改这一层，rng.c 不用动。
 */
void sample_means(double *out, long n, int k);


/* ---------- stats.h ---------- */

double stats_mean(const double *x, long n);

/* 样本标准差，分母取 n-1（无偏估计） */
double stats_std(const double *x, long n);

double stats_min(const double *x, long n);
double stats_max(const double *x, long n);


/* ---------- histogram.h ---------- */

/*
 * 等宽分箱。箱 i 覆盖 [lo + i*w, lo + (i+1)*w)，
 * 最后一箱的右端点闭合，这样 x 恰好等于上界时也能落进去。
 */
long hist_bin_index(double x, double lo, double width, long nbins);

/*
 * 把 x[0..n-1] 统计进 counts[0..nbins-1]，counts 需由调用方清零。
 * 返回落在范围外的样本数（正常应为 0）。
 */
long hist_compute(const double *x, long n, double lo, double width,
                  long nbins, long *counts);

/* 箱中心 */
double hist_center(long i, double lo, double width);


/* ---------- irwin_hall.h ---------- */

/*
 * 三个独立 U(0,1) 随机数之和 S 服从 Irwin-Hall 分布；
 * x = S/3 的分布是它的缩放版本，题目里 x_i 就是这种变量。
 *
 * 这是分箱统计的标准答案：它的密度不是正态分布，
 * 而是一段分段二次曲线（0 与 1 处为零）。第 5 步的正态拟合
 * 只是近似，所以这里单独实现精确分布来验收分箱计数。
 */

/* x 的概率密度，x 在 [0,1] 之外返回 0 */
double ih3_pdf_mean(double x);

/* x 的累积分布，x<0 返回 0，x>1 返回 1 */
double ih3_cdf_mean(double x);


/* ---------- lagrange.h ---------- */

/* 分箱数固定为 10，留一点余量给以后改箱数 */
#define LAGRANGE_MAX_N 32

/*
 * 由 n 个节点 (x[i], y[i]) 构造 Lagrange 插值多项式，
 * 输出按升幂排列的系数 c[0..n-1]：
 *     P(x) = c0 + c1 x + c2 x^2 + ... + c_{n-1} x^{n-1}
 * n 必须不超过 LAGRANGE_MAX_N。
 */
void lagrange_coeffs(const double *x, const double *y, int n, double *c);

/* 按升幂系数用 Horner 法求值 */
double poly_eval(const double *c, int n, double x);

/*
 * 直接用 Lagrange 基函数求和求值（O(n^2)）。
 * 它和系数展开走的是两条不同的数值路径，用来互相验证。
 */
double lagrange_eval(const double *x, const double *y, int n, double t);


/* ---------- gauss_fit.h ---------- */

typedef struct {
    double m;
    double sigma;
    double amp;
    double sse;
    double max_abs_res;
    int iterations;
} GaussFit;

/*
 * 用 Levenberg-Marquardt 最小二乘拟合
 *     f(x) = amp / (sigma * sqrt(2*pi)) * exp(-(x-m)^2 / (2*sigma^2))
 *
 * fix_amp = 1 时只拟合 (m, sigma)，amp 固定为 amp_init；
 * fix_amp = 0 时同时拟合幅度。
 * 返回 1 表示成功，0 表示失败（例如所有 y 都为 0）。
 */
int gauss_fit(const double *x, const double *y, int n,
              double amp_init, int fix_amp, GaussFit *out);

double gauss_model(double x, double m, double sigma, double amp);


/* ---------- verify.h ---------- */

/*
 * 代入式核对：只把已经写进 data 目录里的 .dat 的数字读回来，代进恒等式检查。
 *
 * 为什么要有这一步：Python 交叉核对能发现「C 写错了」，但如果 Python 本身
 * 也写错了，两边可以一起错得自洽。代入式检查用的是定义级恒等式
 * （插值必须过节点、最小二乘的驻点条件、计数之和等于 N），
 * 任何人拿报告里的数字用计算器都能复算，不依赖第二份实现。
 *
 * 返回失败的检查项数（0 表示全部通过）。
 */
int verify_exported(const char *outdir);


/* ========== rng.c ========== */


/*
 * C89 的 rand() 只保证返回 [0, RAND_MAX]，RAND_MAX 的最小保证值是 32767。
 * 一次调用只有 15 位分辨率，所以这里用两次调用拼出约 30 位：
 *
 *     u = (rand() + rand() / (RAND_MAX + 1)) / (RAND_MAX + 1)
 *
 * 结果落在 [0, 1) 内，且与 RAND_MAX 的具体取值无关。
 */
void rng_seed(unsigned long seed)
{
    /* C89 的 srand 只保证接受非负 int；高低位都参与，避免不同种子被截成同一个 */
    srand((unsigned int)(seed & 0xFFFFFFFFUL));
}

double rng_uniform(void)
{
    double a = (double)rand();
    double b = (double)rand() / ((double)RAND_MAX + 1.0);
    return (a + b) / ((double)RAND_MAX + 1.0);
}


/* ========== sampling.c ========== */

void sample_means(double *out, long n, int k)
{
    long i;
    int j;

    for (i = 0; i < n; i++) {
        double sum = 0.0;
        for (j = 0; j < k; j++) {
            sum += rng_uniform();
        }
        out[i] = sum / (double)k;
    }
}


/* ========== stats.c ========== */

double stats_mean(const double *x, long n)
{
    long i;
    double s = 0.0;

    for (i = 0; i < n; i++) {
        s += x[i];
    }
    return s / (double)n;
}

double stats_std(const double *x, long n)
{
    long i;
    double m;
    double s = 0.0;

    if (n < 2) {
        return 0.0;
    }
    m = stats_mean(x, n);
    for (i = 0; i < n; i++) {
        double d = x[i] - m;
        s += d * d;
    }
    return sqrt(s / (double)(n - 1));
}

double stats_min(const double *x, long n)
{
    long i;
    double m = x[0];

    for (i = 1; i < n; i++) {
        if (x[i] < m) {
            m = x[i];
        }
    }
    return m;
}

double stats_max(const double *x, long n)
{
    long i;
    double m = x[0];

    for (i = 1; i < n; i++) {
        if (x[i] > m) {
            m = x[i];
        }
    }
    return m;
}


/* ========== histogram.c ========== */

long hist_bin_index(double x, double lo, double width, long nbins)
{
    long idx;

    if (x < lo) {
        return -1;
    }
    idx = (long)((x - lo) / width);
    if (idx >= nbins) {
        /* 只有恰好等于最右端点时才允许归入最后一箱 */
        if (x <= lo + width * (double)nbins) {
            idx = nbins - 1;
        } else {
            return -1;
        }
    }
    return idx;
}

long hist_compute(const double *x, long n, double lo, double width,
                  long nbins, long *counts)
{
    long i;
    long outside = 0;

    for (i = 0; i < n; i++) {
        long idx = hist_bin_index(x[i], lo, width, nbins);
        if (idx < 0) {
            outside++;
        } else {
            counts[idx]++;
        }
    }
    return outside;
}

double hist_center(long i, double lo, double width)
{
    return lo + ((double)i + 0.5) * width;
}


/* ========== irwin_hall.c ========== */

/*
 * S = X1 + X2 + X3 的密度：
 *     f_S(s) = s^2 / 2                   0 <= s <= 1
 *            = (-2s^2 + 6s - 3) / 2     1 <= s <= 2
 *            = (3 - s)^2 / 2            2 <= s <= 3
 * x = S/3，所以 f_x(x) = 3 f_S(3x)。
 */
double ih3_pdf_mean(double x)
{
    double s;

    if (x < 0.0 || x > 1.0) {
        return 0.0;
    }
    s = 3.0 * x;
    if (s <= 1.0) {
        return 1.5 * s * s;
    }
    if (s <= 2.0) {
        return 1.5 * (-2.0 * s * s + 6.0 * s - 3.0);
    }
    return 1.5 * (3.0 - s) * (3.0 - s);
}

double ih3_cdf_mean(double x)
{
    double s;

    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }
    s = 3.0 * x;
    if (s <= 1.0) {
        return s * s * s / 6.0;
    }
    if (s <= 2.0) {
        return (-2.0 * s * s * s + 9.0 * s * s - 9.0 * s + 3.0) / 6.0;
    }
    return 1.0 - (3.0 - s) * (3.0 - s) * (3.0 - s) / 6.0;
}


/* ========== lagrange.c ========== */

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


/* ========== gauss_fit.c ========== */

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


/* ========== verify.c ========== */


#define MAX_SAMPLES 2000000L
#define NB 10
#define BW 0.1

typedef struct {
    double m;
    double sigma;
    double amp;
    double sse;
} Fit;

static int g_fail = 0;

static void check(const char *name, double got, double want, double tol)
{
    double err = fabs(got - want);
    int ok = (err <= tol);

    if (!ok) {
        g_fail++;
    }
    printf("  %-46s % .10e  期望 %.3e  误差 %.2e  容差 %.0e  %s\n",
           name, got, want, err, tol, ok ? "PASS" : "FAIL");
}

/* 升幂系数按 Horner 求值：c[0] + c[1] x + ... */
static double horner(const double *c, int n, double x)
{
    int k;
    double s = 0.0;

    for (k = n - 1; k >= 0; k--) {
        s = s * x + c[k];
    }
    return s;
}

static void info(const char *name, double v)
{
    printf("  %-46s %.6f（参考量，不作判定）\n", name, v);
}

/* 读数据文件，跳过 # 注释行；每次读 ncol 个浮点数存一行 */
static long read_rows(FILE *fp, int ncol, double *buf, long cap)
{
    char line[512];
    long n = 0;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *tok;
        int k = 0;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        if (n >= cap) {
            break;
        }
        tok = strtok(line, " \t\n");
        while (tok != NULL) {
            if (k < ncol) {
                buf[n * ncol + k] = atof(tok);
                k++;
            }
            tok = strtok(NULL, " \t\n");
        }
        if (k < ncol) {
            fprintf(stderr, "  数据行字段不足：%s", line);
            g_fail++;
            continue;
        }
        n++;
    }
    return n;
}

/* 读 tag key value 形式的参数文件，挑出 primary / free / literal 三组 */
static int read_fits(const char *path, Fit *prim, Fit *free_fit, Fit *lit)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    char tag[32], key[32];
    double v;

    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        return 0;
    }
    memset(prim, 0, sizeof(*prim));
    memset(free_fit, 0, sizeof(*free_fit));
    memset(lit, 0, sizeof(*lit));

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        Fit *dst = NULL;

        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line, "%31s %31s %lf", tag, key, &v) != 3) {
            continue;
        }
        if (strcmp(tag, "primary") == 0) {
            dst = prim;
        } else if (strcmp(tag, "free") == 0) {
            dst = free_fit;
        } else if (strcmp(tag, "literal") == 0) {
            dst = lit;
        } else {
            continue;
        }
        if (strcmp(key, "m") == 0) {
            dst->m = v;
        } else if (strcmp(key, "sigma") == 0) {
            dst->sigma = v;
        } else if (strcmp(key, "A") == 0) {
            dst->amp = v;
        } else if (strcmp(key, "SSE") == 0) {
            dst->sse = v;
        }
    }
    fclose(fp);
    return 1;
}

/*
 * 最小二乘驻点条件（法方程）：极小点处 J^T r = 0。
 * 按柯西不等式，|J^T r| <= sqrt(J^T J)·sqrt(r^T r)，所以用这个尺度归一化。
 */
static void stationarity(const char *label, const double *xs, const double *ys,
                         double m, double s, double A, int fit_amp)
{
    int i;
    double sse = 0.0;
    double gm = 0.0, gs = 0.0, ga = 0.0;
    double qm = 0.0, qs = 0.0, qa = 0.0;

    for (i = 0; i < NB; i++) {
        double z = (xs[i] - m) / s;
        double f = gauss_model(xs[i], m, s, A);
        double r = ys[i] - f;
        double jm = f * z / s;
        double js = f * (z * z - 1.0) / s;
        double ja = f / A;

        sse += r * r;
        gm += r * jm;
        gs += r * js;
        ga += r * ja;
        qm += jm * jm;
        qs += js * js;
        qa += ja * ja;
    }

    printf("\n  [%s]  m=%.6f  sigma=%.6f  A=%.3f  SSE=%.4f\n",
           label, m, s, A, sse);
    check("  |sum r·df/dm| / (|J_m|·|r|)", fabs(gm) / (sqrt(qm * sse) + 1e-300),
          0.0, 1e-6);
    check("  |sum r·df/dsigma| / (|J_sigma|·|r|)",
          fabs(gs) / (sqrt(qs * sse) + 1e-300), 0.0, 1e-6);
    if (fit_amp) {
        check("  |sum r·df/dA| / (|J_A|·|r|)",
              fabs(ga) / (sqrt(qa * sse) + 1e-300), 0.0, 1e-6);
    }
}

int verify_exported(const char *outdir)
{
    char path[512];
    FILE *fp;
    double *sbuf;
    double hist[NB][6];
    double coef[NB];
    double xs[NB], ys[NB];
    long n, i;
    long recount[NB];
    double sum_counts = 0.0;
    double sum_expected = 0.0;
    double worst_recount = 0.0;
    double worst_edge = 0.0;
    double worst_node = 0.0;
    double mid_sum = 0.0;
    double sse_recomputed = 0.0;
    long out_of_range = 0;
    unsigned long seed = 0;
    double worst_hand = 0.0;
    Fit prim, free_fit, lit;

    printf("== 代入式核对（只读 data 目录里的 .dat 里的数字，不重跑算法）==\n");

    sbuf = (double *)malloc(sizeof(double) * 2 * (size_t)MAX_SAMPLES);
    if (sbuf == NULL) {
        fprintf(stderr, "verify: out of memory\n");
        return 1;
    }

    /* ---- samples.dat：全部样本 + 按定义重数一遍分箱 ---- */
    sprintf(path, "%s/samples.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s，请先运行程序\n", path);
        free(sbuf);
        return 1;
    }
    {
        char hdr[512];

        /* 头一行是 # 注释，read_rows 本来就会跳过，这里先把它读出来取 seed */
        if (fgets(hdr, (int)sizeof(hdr), fp) != NULL) {
            char *p = strstr(hdr, "seed=");
            if (p != NULL) {
                seed = strtoul(p + 5, NULL, 10);
            }
        }
    }
    n = read_rows(fp, 2, sbuf, MAX_SAMPLES);
    fclose(fp);

    for (i = 0; i < NB; i++) {
        recount[i] = 0;
    }
    for (i = 0; i < n; i++) {
        double x = sbuf[2 * i + 1];
        long b;
        if (x < 0.0 || x > 1.0) {
            out_of_range++;
        }
        b = (long)(x / BW);
        if (b >= NB) {
            b = NB - 1;
        }
        if (b < 0) {
            b = 0;
        }
        recount[b]++;
    }
    printf("  样本数 N = %ld，seed = %lu\n", n, seed);

    /* V9：把导出的前 12 个均匀数按 3 个一组取平均，与前 4 个样本对 */
    if (n >= 4) {
        double ubuf[12 * 2];
        long got = -1;

        sprintf(path, "%s/uniforms_head.dat", outdir);
        fp = fopen(path, "r");
        if (fp != NULL) {
            got = read_rows(fp, 2, ubuf, 12);
            fclose(fp);
        }
        if (got != 12) {
            printf("  uniforms_head.dat 缺失或行数不足（读到 %ld 行），V9 判失败\n", got);
            worst_hand = 1.0;
        } else {
            long k;
            for (k = 0; k < 4; k++) {
                double sum = ubuf[(3 * k) * 2 + 1];
                double xhat, d;
                int j;
                for (j = 1; j < 3; j++) {
                    sum += ubuf[(3 * k + j) * 2 + 1];
                }
                xhat = sum / 3.0;
                d = fabs(xhat - sbuf[2 * k + 1]);
                if (d > worst_hand) {
                    worst_hand = d;
                }
            }
        }
        check("V9 前 4 个 x_i == 导出的均匀数按 3 个一组取平均（max 差）",
              worst_hand, 0.0, 1e-12);
    }
    check("V1 越界样本数（每个 x 都必须属于某一箱）", (double)out_of_range,
          0.0, 0.0);

    /* ---- histogram.dat ---- */
    sprintf(path, "%s/histogram.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        free(sbuf);
        return 1;
    }
    if (read_rows(fp, 6, hist[0], NB) != NB) {
        printf("  直方图应为 %d 行\n", NB);
        g_fail++;
    }
    fclose(fp);

    for (i = 0; i < NB; i++) {
        xs[i] = hist[i][3];
        ys[i] = hist[i][4];
        sum_counts += hist[i][4];
        sum_expected += hist[i][5];
        if ((double)fabs(hist[i][4] - (double)recount[i]) > worst_recount) {
            worst_recount = fabs(hist[i][4] - (double)recount[i]);
        }
        if (i + 1 < NB && fabs(hist[i + 1][1] - hist[i][2]) > worst_edge) {
            worst_edge = fabs(hist[i + 1][1] - hist[i][2]);
        }
    }
    check("V2 计数之和 == N", sum_counts, (double)n, 0.0);
    check("V3 记录的计数 == 由样本重数的计数（max 差）", worst_recount, 0.0, 0.0);
    check("V4 期望计数之和 == N（CDF 伸缩和）", sum_expected, (double)n,
          1e-9 * (double)n);
    check("V5 相邻箱首尾相接（max |lo(i+1) - hi(i)|）", worst_edge, 0.0, 1e-12);

    /* ---- lagrange_coeffs.dat：把系数代回节点 ---- */
    sprintf(path, "%s/lagrange_coeffs.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        free(sbuf);
        return 1;
    }
    {
        double tmp[NB * 2];
        if (read_rows(fp, 2, tmp, NB) != NB) {
            printf("  系数应为 %d 个\n", NB);
            g_fail++;
        }
        fclose(fp);
        for (i = 0; i < NB; i++) {
            coef[i] = tmp[2 * i + 1];
        }
    }
    for (i = 0; i < NB; i++) {
        double d = fabs(horner(coef, NB, xs[i]) - ys[i]);
        if (d > worst_node) {
            worst_node = d;
        }
    }
    check("V6 文件系数代回节点 max|P(x_i) - y_i|", worst_node, 0.0, 1e-5);

    /* ---- gauss_fit_params.dat：三种口径都代回检查 ---- */
    sprintf(path, "%s/gauss_fit_params.dat", outdir);
    if (!read_fits(path, &prim, &free_fit, &lit)) {
        free(sbuf);
        return 1;
    }
    for (i = 0; i < NB; i++) {
        double r = ys[i] - gauss_model(xs[i], prim.m, prim.sigma, prim.amp);
        sse_recomputed += r * r;
        mid_sum += gauss_model(xs[i], prim.m, prim.sigma, prim.amp) * BW;
    }
    check("V7 记录的 SSE == 代回重算的 SSE（主拟合）", sse_recomputed, prim.sse,
          1e-6 * prim.sse + 1e-9);
    /* f 是「计数密度」：积分应等于幅度 A = N·dx，而不是 N */
    check("V8 sum f(x_i)·dx == A = N·dx（中点法）", mid_sum, prim.amp,
          0.01 * prim.amp);

    stationarity("主拟合 A=N·dx，拟合 m、sigma", xs, ys, prim.m, prim.sigma,
                 prim.amp, 0);
    stationarity("自由幅度拟合 m、sigma、A", xs, ys, free_fit.m, free_fit.sigma,
                 free_fit.amp, 1);

    printf("\n  [反面对照：字面 A=N]  m=%.6f  sigma=%.6f  A=%.1f  SSE=%.2f\n",
           lit.m, lit.sigma, lit.amp, lit.sse);
    info("  与主拟合的 SSE 之比", lit.sse / (prim.sse + 1e-300));
    printf("  说明：字面 A=N 同样满足上面的驻点条件（它确实是自己那个 SSE 的\n"
           "  极小点），所以「代入恒等式」能证明实现自洽，不能证明模型选得对；\n"
           "  模型好坏要靠 V7 的 SSE 比值和第 5 步的精确分布对照来判。\n");

    free(sbuf);

    if (g_fail) {
        printf("\nSUBSTITUTION: %d 项失败\n\n", g_fail);
    } else {
        printf("\nSUBSTITUTION: ALL PASS\n\n");
    }
    return g_fail;
}


/* ========== main.c ========== */


/* 单独抽一批原始均匀数，用来检查发生器本身是否均匀 */
#define RAW_DRAWS 30000L

/* 分箱设置：题目要求箱宽 0.1，x 的理论范围是 [0,1] */
#define BIN_WIDTH 0.1
#define BIN_COUNT 10L

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-n samples] [--seed seed] [outdir]\n", prog);
}

static int cmp_double(const void *pa, const void *pb)
{
    double a = *(const double *)pa;
    double b = *(const double *)pb;

    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static void print_step1(const double *x, long n, unsigned long seed)
{
    long i;
    double mean;
    double sd;
    double se;
    double z;
    double rel;

    mean = stats_mean(x, n);
    sd = stats_std(x, n);
    se = (1.0 / 6.0) / sqrt((double)n);
    z = (mean - 0.5) / se;
    rel = (sd - 1.0 / 6.0) / (1.0 / 6.0);

    printf("== Step 1: x_i = mean of %d uniform numbers ==\n", N_UNIFORM);
    printf("N = %ld, seed = %lu\n", n, seed);
    printf("first 5 x_i:");
    for (i = 0; i < 5 && i < n; i++) {
        printf(" %.6f", x[i]);
    }
    printf("\n");
    printf("min = %.6f, max = %.6f (must lie in [0,1])\n",
           stats_min(x, n), stats_max(x, n));
    printf("mean = %.6f  theory = 0.500000  SE = %.6f  z = %+.2f\n",
           mean, se, z);
    printf("std  = %.6f  theory = 0.166667  rel.dev = %+.2f%%\n",
           sd, 100.0 * rel);

    /* 前 3 个 u 与 x_1 的关系：数据由 uniforms_head.dat 导出，读者可代回核对 */
    {
        double u[3], xhat;
        int j;

        rng_seed(seed);
        for (j = 0; j < 3; j++) {
            u[j] = rng_uniform();
        }
        xhat = (u[0] + u[1] + u[2]) / 3.0;
        printf("  u1..u3 = %.12f %.12f %.12f\n", u[0], u[1], u[2]);
        printf("  x_hat_1 = (u1+u2+u3)/3 = %.12f  vs  x_1 = %.12f  差 = %.2e\n",
               xhat, x[0], fabs(xhat - x[0]));
        printf("\n");
    }

    {
        long m = RAW_DRAWS;
        long j;
        double s1 = 0.0;
        double s2 = 0.0;
        double lo = 1.0;
        double hi = 0.0;

        /* 重新开始，跳过采样用掉的前 3n 个数，让这批原始均匀数与样本错开 */
        rng_seed(seed);
        for (j = 0; j < 3L * n; j++) {
            (void)rng_uniform();
        }
        for (j = 0; j < m; j++) {
            double u = rng_uniform();
            s1 += u;
            s2 += u * u;
            if (u < lo) {
                lo = u;
            }
            if (u > hi) {
                hi = u;
            }
        }
        printf("raw uniform: %ld draws, mean = %.6f (0.5), "
               "var = %.6f (0.083333), min = %.6f, max = %.6f\n",
               m, s1 / (double)m,
               s2 / (double)m - (s1 / (double)m) * (s1 / (double)m),
               lo, hi);
    }

    if (fabs(z) > 3.0) {
        printf("WARNING: sample mean deviates by more than 3 standard errors\n");
    }
    if (fabs(rel) > 0.05) {
        printf("WARNING: sample std deviates by more than 5%%\n");
    }
    printf("\n");
}

static void expected_counts(long n, double *exp_counts)
{
    long i;

    for (i = 0; i < BIN_COUNT; i++) {
        double lo = BIN_WIDTH * (double)i;
        double hi = lo + BIN_WIDTH;
        exp_counts[i] = (double)n * (ih3_cdf_mean(hi) - ih3_cdf_mean(lo));
    }
}

static double ks_statistic(const double *sorted, long n)
{
    long i;
    double d = 0.0;

    for (i = 0; i < n; i++) {
        double f = ih3_cdf_mean(sorted[i]);
        double d1 = fabs((double)(i + 1) / (double)n - f);
        double d2 = fabs((double)i / (double)n - f);
        if (d1 > d) {
            d = d1;
        }
        if (d2 > d) {
            d = d2;
        }
    }
    return d;
}

static void print_step2(const double *x, long n, const long *counts,
                        const double *exp_counts)
{
    long i;
    long total = 0;
    double chi2 = 0.0;
    double *sorted;
    double d;
    double d_crit;

    for (i = 0; i < BIN_COUNT; i++) {
        total += counts[i];
    }

    printf("== Step 2: histogram, width = %.1f, %ld bins ==\n",
           BIN_WIDTH, BIN_COUNT);
    printf(" bin   range          center   count   expected\n");
    for (i = 0; i < BIN_COUNT; i++) {
        printf(" %2ld   [%.2f, %.2f)   %.2f   %5ld   %8.2f\n",
               i, BIN_WIDTH * (double)i, BIN_WIDTH * (double)(i + 1),
               hist_center(i, 0.0, BIN_WIDTH), counts[i], exp_counts[i]);
        if (exp_counts[i] > 0.0) {
            double dlt = (double)counts[i] - exp_counts[i];
            chi2 += dlt * dlt / exp_counts[i];
        }
    }

    sorted = (double *)malloc((size_t)n * sizeof(double));
    if (sorted == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    memcpy(sorted, x, (size_t)n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), cmp_double);
    d = ks_statistic(sorted, n);
    d_crit = 1.36 / sqrt((double)n);
    free(sorted);

    printf("\nsum of counts = %ld   (N = %ld, difference = %ld)\n",
           total, n, total - n);
    printf("chi-square = %.3f  (df = %ld, 5%% critical = 16.919)\n",
           chi2, BIN_COUNT - 1);
    printf("K-S D = %.5f   (5%% critical ~ %.5f)\n", d, d_crit);

    printf("\ncheck: P(x in [0.4,0.5)) = %.6f, expected count = %.2f\n",
           ih3_cdf_mean(0.5) - ih3_cdf_mean(0.4),
           (double)n * (ih3_cdf_mean(0.5) - ih3_cdf_mean(0.4)));
    printf("check: P(x in [0.9,1.0)) = %.6f, expected count = %.2f\n",
           ih3_cdf_mean(1.0) - ih3_cdf_mean(0.9),
           (double)n * (ih3_cdf_mean(1.0) - ih3_cdf_mean(0.9)));
    printf("check: symmetry count[i] vs count[%ld-i] (mirror about 0.5)\n",
           BIN_COUNT - 1);
    for (i = 0; i < BIN_COUNT / 2; i++) {
        printf("  bin %ld = %4ld   |   bin %ld = %4ld   | diff = %ld\n",
               i, counts[i], BIN_COUNT - 1 - i, counts[BIN_COUNT - 1 - i],
               counts[i] - counts[BIN_COUNT - 1 - i]);
    }

    if (total != n) {
        printf("WARNING: %ld samples fell outside the histogram range\n",
               n - total);
    }
    printf("\n");
}

static void run_lagrange(const long *counts, const char *outdir)
{
    int n = (int)BIN_COUNT;
    int i;
    int npts = 901;
    double xs[LAGRANGE_MAX_N];
    double ys[LAGRANGE_MAX_N];
    double c[LAGRANGE_MAX_N];
    double max_node_res = 0.0;
    double max_node_res_direct = 0.0;
    double max_diff = 0.0;
    double cmax = 0.0;
    double p_left;
    double p_right;
    double x0;
    double x1;
    double dx;
    char path[512];
    FILE *fp;

    for (i = 0; i < n; i++) {
        xs[i] = hist_center((long)i, 0.0, BIN_WIDTH);
        ys[i] = (double)counts[i];
    }
    lagrange_coeffs(xs, ys, n, c);

    printf("== Step 4: Lagrange interpolation through %d binned points ==\n", n);
    printf("degree = %d, nodes span [%.2f, %.2f]\n", n - 1, xs[0], xs[n - 1]);
    printf("coefficients (ascending powers):\n");
    for (i = 0; i < n; i++) {
        printf("  c%-2d = % .10e\n", i, c[i]);
        if (fabs(c[i]) > cmax) {
            cmax = fabs(c[i]);
        }
    }

    for (i = 0; i < n; i++) {
        double r = fabs(poly_eval(c, n, xs[i]) - ys[i]);
        double rd = fabs(lagrange_eval(xs, ys, n, xs[i]) - ys[i]);
        if (r > max_node_res) {
            max_node_res = r;
        }
        if (rd > max_node_res_direct) {
            max_node_res_direct = rd;
        }
    }

    /* [0,1] 上 1001 点：系数展开与直接基函数求和互相验证 */
    for (i = 0; i <= 1000; i++) {
        double t = (double)i / 1000.0;
        double d = fabs(poly_eval(c, n, t) - lagrange_eval(xs, ys, n, t));
        if (d > max_diff) {
            max_diff = d;
        }
    }

    p_left = poly_eval(c, n, 0.0);
    p_right = poly_eval(c, n, 1.0);

    printf("\nmax |P_direct(x_i) - y_i| over nodes = %.3e\n", max_node_res_direct);
    printf("max |P_monomial(x_i) - y_i|          = %.3e "
           "(degree-9 monomial basis loses digits)\n", max_node_res);
    printf("max |monomial - direct Lagrange|     = %.3e (1001 grid points)\n",
           max_diff);
    printf("extrapolation: P(0) = %.3f, P(1) = %.3f "
           "(nodes only cover [0.05, 0.95])\n", p_left, p_right);
    printf("largest |coefficient| = %.3e\n", cmax);

    /* 只导出节点区间：区间外的值属于外推，振荡没有物理意义 */
    x0 = xs[0];
    x1 = xs[n - 1];
    dx = (x1 - x0) / (double)(npts - 1);

    sprintf(path, "%s/lagrange.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# x P(x)  Lagrange degree %d, %d nodes [%.2f, %.2f]\n",
            n - 1, n, x0, x1);
    for (i = 0; i < npts; i++) {
        double t = x0 + dx * (double)i;
        fprintf(fp, "%.6f %.10e\n", t, lagrange_eval(xs, ys, n, t));
    }
    fclose(fp);
    printf("wrote %s\n", path);

    sprintf(path, "%s/lagrange_coeffs.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# degree %d, ascending coefficients c0..c%d\n", n - 1, n - 1);
    for (i = 0; i < n; i++) {
        fprintf(fp, "%d %.15e\n", i, c[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    printf("\n");
}

static void run_gauss_fit(const long *counts, long n_samples,
                          double raw_mean, double raw_std, const char *outdir)
{
    int n = (int)BIN_COUNT;
    int i;
    double xs[LAGRANGE_MAX_N];
    double ys[LAGRANGE_MAX_N];
    double amp_bin = (double)n_samples * BIN_WIDTH;
    GaussFit fit_bin;
    GaussFit fit_free;
    GaussFit fit_literal;
    char path[512];
    FILE *fp;
    int ok1;
    int ok2;
    int ok3;

    for (i = 0; i < n; i++) {
        xs[i] = hist_center((long)i, 0.0, BIN_WIDTH);
        ys[i] = (double)counts[i];
    }

    ok1 = gauss_fit(xs, ys, n, amp_bin, 1, &fit_bin);
    ok2 = gauss_fit(xs, ys, n, amp_bin, 0, &fit_free);
    ok3 = gauss_fit(xs, ys, n, (double)n_samples, 1, &fit_literal);

    printf("== Step 5: Gaussian fit to binned counts ==\n");
    printf("model: f(x) = A/(sigma*sqrt(2*pi)) * exp(-(x-m)^2/(2*sigma^2))\n");
    printf("N = %ld, bin width = %.1f, count scale A = N*dx = %.3f\n\n",
           n_samples, BIN_WIDTH, amp_bin);

    if (!ok1) {
        printf("primary fit failed\n");
        return;
    }

    printf("primary fit (A = N*dx fixed):\n");
    printf("  m = %.6f, sigma = %.6f, A = %.3f\n",
           fit_bin.m, fit_bin.sigma, fit_bin.amp);
    printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
           fit_bin.sse, fit_bin.max_abs_res, fit_bin.iterations);
    printf("  vs sample mean = %.6f  (difference %+.6f)\n",
           raw_mean, fit_bin.m - raw_mean);
    printf("  vs sample std  = %.6f  (sigma is %+.2f%%)\n",
           raw_std, 100.0 * (fit_bin.sigma / raw_std - 1.0));
    printf("  vs theory 1/6  = %.6f  (sigma is %+.2f%%)\n",
           1.0 / 6.0, 100.0 * (fit_bin.sigma / (1.0 / 6.0) - 1.0));

    if (ok2) {
        printf("\ncross-check, A free:\n");
        printf("  m = %.6f, sigma = %.6f, A = %.3f (A/(N*dx) = %.4f)\n",
               fit_free.m, fit_free.sigma, fit_free.amp,
               fit_free.amp / amp_bin);
        printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
               fit_free.sse, fit_free.max_abs_res, fit_free.iterations);
    }
    if (ok3) {
        printf("\ncross-check, literal A = N fixed (formula without bin width):\n");
        printf("  m = %.6f, sigma = %.6f, A = %.3f\n",
               fit_literal.m, fit_literal.sigma, fit_literal.amp);
        printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
               fit_literal.sse, fit_literal.max_abs_res,
               fit_literal.iterations);
    }

    printf("\n bin  center  count   fitted  residual\n");
    for (i = 0; i < n; i++) {
        double f = gauss_model(xs[i], fit_bin.m, fit_bin.sigma, fit_bin.amp);
        printf("  %2d  %.2f  %6.0f  %7.2f  %+8.2f\n",
               i, xs[i], ys[i], f, ys[i] - f);
    }

    sprintf(path, "%s/gauss_fit.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# x fitted_count  Gaussian A=N*dx, m=%.6f sigma=%.6f\n",
            fit_bin.m, fit_bin.sigma);
    for (i = 0; i <= 500; i++) {
        double t = (double)i / 500.0;
        fprintf(fp, "%.6f %.10e\n", t,
                gauss_model(t, fit_bin.m, fit_bin.sigma, fit_bin.amp));
    }
    fclose(fp);
    printf("wrote %s\n", path);

    sprintf(path, "%s/gauss_fit_params.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# model f(x)=A/(sigma*sqrt(2*pi))*exp(-(x-m)^2/(2*sigma^2))\n");
    fprintf(fp, "primary m %.12f\n", fit_bin.m);
    fprintf(fp, "primary sigma %.12f\n", fit_bin.sigma);
    fprintf(fp, "primary A %.12f\n", fit_bin.amp);
    fprintf(fp, "primary SSE %.12f\n", fit_bin.sse);
    if (ok2) {
        fprintf(fp, "free m %.12f\n", fit_free.m);
        fprintf(fp, "free sigma %.12f\n", fit_free.sigma);
        fprintf(fp, "free A %.12f\n", fit_free.amp);
        fprintf(fp, "free SSE %.12f\n", fit_free.sse);
    }
    if (ok3) {
        fprintf(fp, "literal m %.12f\n", fit_literal.m);
        fprintf(fp, "literal sigma %.12f\n", fit_literal.sigma);
        fprintf(fp, "literal A %.12f\n", fit_literal.amp);
        fprintf(fp, "literal SSE %.12f\n", fit_literal.sse);
    }
    fclose(fp);
    printf("wrote %s\n\n", path);
}

static int write_samples(const char *outdir, const double *x, long n,
                         unsigned long seed)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/samples.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fprintf(fp, "# i x_i  (mean of %d uniform numbers, N=%ld, seed=%lu)\n",
            N_UNIFORM, n, seed);
    for (i = 0; i < n; i++) {
        fprintf(fp, "%ld %.12f\n", i + 1, x[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

static int write_uniforms_head(const char *outdir, unsigned long seed)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/uniforms_head.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    /* 前 4 个样本用到的 12 个均匀数，供「u -> x」代入核对 */
    fprintf(fp, "# k u_k  (first %d draws after srand(%lu))\n",
            4 * N_UNIFORM, seed);
    rng_seed(seed);
    for (i = 0; i < 4L * N_UNIFORM; i++) {
        fprintf(fp, "%ld %.12f\n", i + 1, rng_uniform());
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

static int write_histogram(const char *outdir, const long *counts,
                           const double *exp_counts, long n)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/histogram.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fprintf(fp, "# bin lo hi center count expected  (N=%ld, width=%.3f)\n",
            n, BIN_WIDTH);
    for (i = 0; i < BIN_COUNT; i++) {
        fprintf(fp, "%ld %.3f %.3f %.6f %ld %.6f\n",
                i, BIN_WIDTH * (double)i, BIN_WIDTH * (double)(i + 1),
                hist_center(i, 0.0, BIN_WIDTH), counts[i], exp_counts[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

int main(int argc, char **argv)
{
    long n = DEFAULT_N_SAMPLES;
    unsigned long seed = RNG_DEFAULT_SEED;
    const char *outdir = "data";
    double *x;
    long *counts;
    double exp_counts[BIN_COUNT];
    long i;
    long outside;
    int argi;

    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "-n") == 0 && argi + 1 < argc) {
            n = atol(argv[++argi]);
        } else if (strcmp(argv[argi], "--seed") == 0 && argi + 1 < argc) {
            seed = (unsigned long)strtoul(argv[++argi], NULL, 10);
        } else if (argv[argi][0] != '-') {
            outdir = argv[argi];
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (n < 2) {
        fprintf(stderr, "n must be >= 2\n");
        return 1;
    }

    x = (double *)malloc((size_t)n * sizeof(double));
    counts = (long *)malloc((size_t)BIN_COUNT * sizeof(long));
    if (x == NULL || counts == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    for (i = 0; i < BIN_COUNT; i++) {
        counts[i] = 0;
    }

    rng_seed(seed);
    sample_means(x, n, N_UNIFORM);

    print_step1(x, n, seed);

    outside = hist_compute(x, n, 0.0, BIN_WIDTH, BIN_COUNT, counts);
    expected_counts(n, exp_counts);
    print_step2(x, n, counts, exp_counts);
    if (outside != 0) {
        printf("WARNING: %ld samples outside [0,1]\n", outside);
    }

    write_samples(outdir, x, n, seed);
    write_uniforms_head(outdir, seed);
    write_histogram(outdir, counts, exp_counts, n);
    run_lagrange(counts, outdir);
    run_gauss_fit(counts, n, stats_mean(x, n), stats_std(x, n), outdir);

    /* 代入式核对：把刚写进 data 目录里的 .dat 的数字读回来验恒等式 */
    {
        int bad = verify_exported(outdir);
        free(counts);
        free(x);
        return bad ? 1 : 0;
    }
}
