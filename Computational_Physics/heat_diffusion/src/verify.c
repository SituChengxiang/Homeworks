#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "verify.h"

#define V_NNCAP   1024
#define V_LINEBUF 65536
#define V_PROF_COLS 6
#define V_SD_COLS   3
#define V_CR_COLS   5
#define V_SU_COLS   16

/* C89 没有 NAN 宏：NA 用这个不可能自然出现的哨兵值表示 */
#define V_NA (-1.23456789012345e300)

static long v_checks_run = 0;
static long v_checks_failed = 0;

static void vcheck(const char *id, const char *what, int ok, const char *detail)
{
    v_checks_run++;
    if (!ok) {
        v_checks_failed++;
    }
    printf("  [%s] %-4s %-54s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/* ---------- 文件读取 ---------- */

/*
 * 读下一个数据行（跳过 # 注释）。NA 解析成 V_NA 哨兵。
 * 返回列数；0 = 到文件末尾；-1 = 格式或容量问题。
 */
static long dat_next(FILE *fp, double *row, long cap)
{
    char line[V_LINEBUF];
    char *p;
    char *end;
    long cols;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '#') {
            continue;
        }
        cols = 0;
        p = line;
        while (*p != '\0') {
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (*p == '\0' || *p == '\n') {
                break;
            }
            if (cols >= cap) {
                return -1;
            }
            if (*p == 'N') {
                while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                    p++;
                }
                row[cols++] = V_NA;
                continue;
            }
            row[cols] = strtod(p, &end);
            if (end == p) {
                return -1;
            }
            cols++;
            p = end;
        }
        if (cols > 0) {
            return cols;
        }
    }
    return 0;
}

/* 整个小文件读进 buf（按行展平），返回行数 */
static long dat_load(const char *outdir, const char *name, double *buf,
                     long cap, long *ncol)
{
    char path[512];
    FILE *fp;
    long n = 0;
    long cols;
    long first = -1;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    while (n < cap) {
        cols = dat_next(fp, buf + n, cap - n);
        if (cols <= 0) {
            break;
        }
        if (first < 0) {
            first = cols;
        } else if (cols != first) {
            fclose(fp);
            return -2;
        }
        n += cols;
    }
    fclose(fp);
    if (first < 0) {
        return -3;
    }
    *ncol = first;
    if (n % first != 0) {
        return -4;
    }
    return n / first;
}

/* 只取第一个和最后一个数据行（演化文件太大，不能整个读进来） */
static int dat_first_last(const char *outdir, const char *name,
                          double *first, double *last, long cap, long *ncol)
{
    char path[512];
    FILE *fp;
    double *tmp;
    long cols;
    long seen = 0;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }
    tmp = (double *)malloc((size_t)cap * sizeof(double));
    if (tmp == NULL) {
        fclose(fp);
        return 2;
    }
    while ((cols = dat_next(fp, tmp, cap)) > 0) {
        if (seen == 0) {
            memcpy(first, tmp, (size_t)cols * sizeof(double));
            *ncol = cols;
        }
        memcpy(last, tmp, (size_t)cols * sizeof(double));
        seen++;
    }
    free(tmp);
    fclose(fp);
    return (seen > 0) ? 0 : 3;
}

/* 从注释头按 key 取一个数；key 前面必须有一个空格，避免 n= 命中 nn= */
static int dat_param(const char *outdir, const char *name, const char *key,
                     double *out)
{
    char path[512];
    char line[V_LINEBUF];
    char search[64];
    char *p;
    char *end;
    FILE *fp;

    sprintf(path, "%s/%s.dat", outdir, name);
    sprintf(search, " %s=", key);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] != '#') {
            break; /* 参数都在注释头 */
        }
        p = strstr(line, search);
        if (p != NULL) {
            p += strlen(search);
            *out = strtod(p, &end);
            if (end != p) {
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return 2;
}

/*
 * 两个数的相对差。演化文件只存 10 位有效数字，往返误差的量级是
 * 5e-10 x |值|：周期收敛值约 10 度，绝对差就有 5e-9，用绝对容差会误判。
 * 所以跨文件比对一律按相对差，量级小于 1 的值按 1 兜底。
 */
static double rel_diff(double a, double b)
{
    double m = fabs(a);

    if (fabs(b) > m) {
        m = fabs(b);
    }
    if (m < 1.0) {
        m = 1.0;
    }
    return fabs(a - b) / m;
}

/* ---------- 有限体积定义式（自己按定义重算，不调用 grid.c） ---------- */

static double seg_overlap(double a, double b, double lo, double hi)
{
    double l = (a > lo) ? a : lo;
    double r = (b < hi) ? b : hi;
    return (r > l) ? r - l : 0.0;
}

static double fv_overlap(double x, double dx, double lo, double hi,
                         double len, int periodic)
{
    double a = x - dx / 2.0;
    double b = x + dx / 2.0;

    if (!periodic) {
        if (a < 0.0) {
            a = 0.0;
        }
        if (b > len) {
            b = len;
        }
        return seg_overlap(a, b, lo, hi);
    }
    return seg_overlap(a, b, lo, hi)
           + seg_overlap(a, b, lo - len, hi - len)
           + seg_overlap(a, b, lo + len, hi + len);
}

static double fv_width(double x, double dx, double len, int periodic)
{
    double a;
    double b;

    if (periodic) {
        return dx;
    }
    a = x - dx / 2.0;
    b = x + dx / 2.0;
    if (a < 0.0) {
        a = 0.0;
    }
    if (b > len) {
        b = len;
    }
    return b - a;
}

/* ---------- 单个组合 ---------- */

static void verify_case(const char *outdir, const char *dxtag,
                        const char *bctag, int periodic)
{
    static double prof[(long)V_NNCAP * V_PROF_COLS];
    static double sd[(long)V_NNCAP * V_SD_COLS];
    static double cr[(long)V_NNCAP * V_CR_COLS];
    static double ev1[V_NNCAP];
    static double evN[V_NNCAP];
    char name[128];
    double len, dx, nf, nnf, hlo, hhi, htemp, ctemp;
    double target, eps, tconv, perflag;
    double sdt, salpha, sr;
    double ctau, cdt, ceps;
    long n;
    long nn;
    long nrows;
    long ncol;
    long i;
    long c;
    double pred;
    double worst;
    double d;
    double heat0;
    double colmax[V_PROF_COLS];
    double colmin[V_PROF_COLS];
    double dev;
    double jump;
    double bal_pred;
    int ok;
    char buf[256];

    printf("\n  ---- %s / %s ----\n", dxtag, bctag);

    sprintf(name, "profiles_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, prof, (long)V_NNCAP * V_PROF_COLS, &ncol);
    if (nrows < 0 || ncol != V_PROF_COLS) {
        vcheck("V0", "剖面文件可读且列数为 6", 0, name);
        return;
    }
    dat_param(outdir, name, "length", &len);
    dat_param(outdir, name, "dx", &dx);
    dat_param(outdir, name, "n", &nf);
    dat_param(outdir, name, "nn", &nnf);
    dat_param(outdir, name, "hot_lo", &hlo);
    dat_param(outdir, name, "hot_hi", &hhi);
    dat_param(outdir, name, "hot_temp", &htemp);
    dat_param(outdir, name, "cold_temp", &ctemp);
    dat_param(outdir, name, "target", &target);
    dat_param(outdir, name, "eps", &eps);
    dat_param(outdir, name, "t_conv", &tconv);
    dat_param(outdir, name, "periodic", &perflag);
    n = (long)nf;
    nn = (long)nnf;

    if (nrows != nn) {
        sprintf(buf, "行数 %ld vs nn=%ld", nrows, nn);
        vcheck("V0", "剖面行数等于 nn", 0, buf);
        return;
    }

    /* V1 初值列 = 有限体积定义式 */
    worst = 0.0;
    for (i = 0; i < nn; i++) {
        double x = prof[i * V_PROF_COLS + 0];
        double w = fv_width(x, dx, len, periodic);
        double ov = fv_overlap(x, dx, hlo, hhi, len, periodic);
        pred = ctemp + (htemp - ctemp) * (ov / w);
        d = fabs(prof[i * V_PROF_COLS + 1] - pred);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "max|u_t0 - 定义式| = %.3e", worst);
    vcheck("V1", "初值列 = 控制体重叠定义式", worst <= 1e-12, buf);

    /* V2 初值热量 = (hot-cold)*宽度 */
    heat0 = 0.0;
    for (i = 0; i < nn; i++) {
        double x = prof[i * V_PROF_COLS + 0];
        if (periodic && i == n) {
            break; /* i=n 与 i=0 是同一点，不重复计入 */
        }
        heat0 += prof[i * V_PROF_COLS + 1] * fv_width(x, dx, len, periodic);
    }
    pred = (htemp - ctemp) * (hhi - hlo);
    sprintf(buf, "sum u_t0 w = %.15f，(100-0)*0.01 = %.15f，差 %.3e",
            heat0, pred, fabs(heat0 - pred));
    vcheck("V2", "初值总热量 = (hot-cold) x 高温段长",
           fabs(heat0 - pred) <= 1e-12 * pred, buf);

    /* V3 边界条件逐列成立 */
    worst = 0.0;
    for (c = 1; c < V_PROF_COLS; c++) {
        if (periodic) {
            d = fabs(prof[0 * V_PROF_COLS + c] - prof[n * V_PROF_COLS + c]);
        } else {
            d = fabs(prof[0 * V_PROF_COLS + c] - ctemp);
            if (fabs(prof[n * V_PROF_COLS + c] - ctemp) > d) {
                d = fabs(prof[n * V_PROF_COLS + c] - ctemp);
            }
        }
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "五条剖面全部满足，最大偏差 %.3e", worst);
    vcheck("V3", periodic ? "周期：u(0,t) 与 u(l,t) 是同一点"
                          : "Dirichlet：两端恒等于 cold_temp",
           worst == 0.0, buf);

    /* V4 极值原理（r<=1/2 时凸组合的推论），V5 最大值沿时间单调不增 */
    for (c = 1; c < V_PROF_COLS; c++) {
        colmax[c] = prof[0 * V_PROF_COLS + c];
        colmin[c] = colmax[c];
        for (i = 1; i < nn; i++) {
            if (prof[i * V_PROF_COLS + c] > colmax[c]) {
                colmax[c] = prof[i * V_PROF_COLS + c];
            }
            if (prof[i * V_PROF_COLS + c] < colmin[c]) {
                colmin[c] = prof[i * V_PROF_COLS + c];
            }
        }
    }
    ok = 1;
    for (c = 2; c < V_PROF_COLS; c++) {
        if (colmax[c] > colmax[1] + 1e-12 || colmin[c] < colmin[1] - 1e-12) {
            ok = 0;
        }
    }
    sprintf(buf, "初值范围 [%.6f, %.6f]，后续剖面均未越出", colmin[1], colmax[1]);
    vcheck("V4", "极值原理：max 不增、min 不减", ok, buf);

    ok = 1;
    for (c = 2; c < V_PROF_COLS; c++) {
        if (colmax[c] > colmax[c - 1] + 1e-12) {
            ok = 0;
        }
    }
    sprintf(buf, "max u: %.6f -> %.6f -> %.6f -> %.6f -> %.3e",
            colmax[1], colmax[2], colmax[3], colmax[4], colmax[5]);
    vcheck("V5", "max u 沿 0/10/20/30/收敛 单调不增", ok, buf);

    /* V6 对称性（关于棍中点） */
    worst = 0.0;
    for (c = 1; c < V_PROF_COLS; c++) {
        for (i = 0; i < nn; i++) {
            d = fabs(prof[i * V_PROF_COLS + c]
                     - prof[(n - i) * V_PROF_COLS + c]);
            if (d > worst) {
                worst = d;
            }
        }
    }
    sprintf(buf, "五条剖面 max|u_i - u_(n-i)| = %.3e", worst);
    vcheck("V6", "关于中点对称", worst <= 1e-9, buf);

    /* V7 收敛判据自洽 */
    dev = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(prof[i * V_PROF_COLS + 5] - target);
        if (d > dev) {
            dev = d;
        }
    }
    sprintf(buf, "max|u_conv - target| = %.4e，eps = %.1e", dev, eps);
    vcheck("V7", "收敛列满足主判据且没有多跑",
           dev <= eps && dev > eps / 1000.0, buf);

    /* V8 终态取值 */
    pred = periodic ? heat0 / len : ctemp;
    sprintf(buf, "target = %.12f，定义式给出 %.12f", target, pred);
    vcheck("V8", periodic ? "周期终态 = 初值热量 / l = 10 度"
                          : "Dirichlet 终态 = 边界温度 0 度",
           fabs(target - pred) <= 1e-9, buf);

    /* V9 FTCS 差分方程本身 */
    sprintf(name, "stepdt_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, sd, (long)V_NNCAP * V_SD_COLS, &ncol);
    if (nrows < 0 || ncol != V_SD_COLS || nrows != nn) {
        vcheck("V9", "stepdt 文件可读且行数等于 nn", 0, name);
    } else {
        dat_param(outdir, name, "dt", &sdt);
        dat_param(outdir, name, "alpha", &salpha);
        dat_param(outdir, name, "r", &sr);
        jump = 0.0;
        /* 只有独立未知量才满足递推：Dirichlet 是 i=1..n-1，
           周期是 i=0..n-1（i=n 是 i=0 的副本，代进去会越界读一列） */
        for (i = periodic ? 0 : 1; i < n; i++) {
            long im;
            long ip;
            double res;

            if (periodic) {
                im = (i == 0) ? n - 1 : i - 1;
                ip = (i == n - 1) ? 0 : i + 1;
            } else {
                im = i - 1;
                ip = i + 1;
            }
            res = sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1]
                  - sr * (sd[ip * V_SD_COLS + 1] - 2.0 * sd[i * V_SD_COLS + 1]
                          + sd[im * V_SD_COLS + 1]);
            if (fabs(res) > jump) {
                jump = fabs(res);
            }
        }
        sprintf(buf, "max 残差 = %.3e（r=%.17g 取自文件头）", jump, sr);
        vcheck("V9", "导出数字逐个满足 FTCS 差分方程", jump <= 1e-14, buf);

        /* V10 离散热量收支 */
        if (periodic) {
            double s = 0.0;
            for (i = 0; i < n; i++) {
                s += sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1];
            }
            sprintf(buf, "sum(u^{n+1}-u^n) = %.3e", s);
            vcheck("V10", "周期：离散步热量严格守恒", fabs(s) <= 1e-12, buf);
        } else {
            double s = 0.0;
            for (i = 1; i < n; i++) {
                s += sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1];
            }
            bal_pred = -sr * (sd[1 * V_SD_COLS + 1]
                              + sd[(n - 1) * V_SD_COLS + 1]);
            sprintf(buf, "内部和 = %.12f，-r(u_1+u_{n-1}) = %.12f，差 %.3e",
                    s, bal_pred, fabs(s - bal_pred));
            vcheck("V10", "Dirichlet：离散步热量收支 = 两端净流出",
                   fabs(s - bal_pred) <= 1e-12, buf);
        }
    }

    /* V11 副判据的松紧 */
    sprintf(name, "criterion_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, cr, (long)V_NNCAP * V_CR_COLS, &ncol);
    if (nrows < 0 || ncol != V_CR_COLS) {
        vcheck("V11", "criterion 文件可读", 0, name);
    } else {
        dat_param(outdir, name, "tau", &ctau);
        dat_param(outdir, name, "dt", &cdt);
        dat_param(outdir, name, "eps_main", &ceps);
        worst = 0.0;
        ok = 0;
        for (i = 0; i < nrows; i++) {
            double es = cr[i * V_CR_COLS + 0];
            double st = cr[i * V_CR_COLS + 1];
            double dv = cr[i * V_CR_COLS + 3];
            if (st < 0.0 || st == V_NA) {
                continue; /* NA / 未达到 */
            }
            pred = es * (ctau / cdt);
            d = fabs(dv / pred - 1.0);
            if (d > worst) {
                worst = d;
            }
            ok++;
        }
        sprintf(buf, "%d 档达到，实测/预测 最大相对差 %.4f%%",
                ok, 100.0 * worst);
        vcheck("V11", "副判据停手残差 = eps_step x tau/dt",
               ok >= 3 && worst <= 0.05, buf);
    }

    /* V12 演化文件首末行与剖面一致 */
    sprintf(name, "evolution_%s_%s", dxtag, bctag);
    if (dat_first_last(outdir, name, ev1, evN, V_NNCAP, &ncol) != 0
        || ncol != nn + 1) {
        sprintf(buf, "读到 %ld 列，应为 nn+1 = %ld（多一列 t）", ncol, nn + 1);
        vcheck("V12", "演化文件可读且列数等于 nn+1", 0, buf);
    } else {
        worst = 0.0;
        for (i = 0; i < nn; i++) {
            d = rel_diff(ev1[i + 1], prof[i * V_PROF_COLS + 1]);
            if (d > worst) {
                worst = d;
            }
        }
        sprintf(buf, "首行 t=%.10g 与 u_t0 列 max 相对差 %.3e", ev1[0], worst);
        vcheck("V12", "演化首行 = 初值剖面", worst <= 1e-9, buf);

        worst = 0.0;
        for (i = 0; i < nn; i++) {
            d = rel_diff(evN[i + 1], prof[i * V_PROF_COLS + 5]);
            if (d > worst) {
                worst = d;
            }
        }
        sprintf(buf, "末行 t=%.10g（t_conv=%.10g）与收敛列 max 相对差 %.3e",
                evN[0], tconv, worst);
        vcheck("V13", "演化末行 = 收敛剖面", worst <= 1e-9, buf);
    }
}

/* ---------- 跨组合 ---------- */

static void verify_global(const char *outdir)
{
    static double su[(long)V_NNCAP * V_SU_COLS];
    long nrows;
    long ncol;
    long i;
    double worst;
    double d;
    double rel;
    double eps;
    char buf[256];

    printf("\n  ---- 跨组合（summary.dat）----\n");
    nrows = dat_load(outdir, "summary", su, (long)V_NNCAP * V_SU_COLS, &ncol);
    if (nrows < 0 || ncol != V_SU_COLS) {
        vcheck("V14", "summary 可读且列数为 16", 0, "summary.dat");
        return;
    }
    dat_param(outdir, "profiles_fine_dirichlet", "eps", &eps);

    /* V14 每行 t_pred 的定义式 */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        double tau = su[i * V_SU_COLS + 5];
        double amp = su[i * V_SU_COLS + 4];
        double t_pred = su[i * V_SU_COLS + 6];
        double p = tau * log(amp / eps);

        d = fabs(t_pred / p - 1.0);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "四行 t_pred vs tau*ln(amp/eps) 最大相对差 %.3e", worst);
    vcheck("V14", "预测收敛时刻满足它自己的定义式", worst <= 1e-12, buf);

    /* V15 实测 t_conv 与预测 t_pred 一致 */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        rel = fabs(su[i * V_SU_COLS + 7] / su[i * V_SU_COLS + 6] - 1.0);
        if (rel > worst) {
            worst = rel;
        }
    }
    sprintf(buf, "最大相对差 %.4f%%", 100.0 * worst);
    vcheck("V15", "实测收敛时刻与单指数预测一致", worst <= 0.05, buf);

    /* V16 步数与时刻自洽：steps_conv * dt = t_conv */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        d = fabs(su[i * V_SU_COLS + 8] * su[i * V_SU_COLS + 2]
                 - su[i * V_SU_COLS + 7]);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "max |steps*dt - t_conv| = %.3e", worst);
    vcheck("V16", "步数、dt、收敛时刻三者自洽", worst <= 1e-9, buf);

    /* V17 停手残差不超过 eps */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        d = su[i * V_SU_COLS + 9];
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "四组合最大停手残差 %.4e，eps %.1e", worst, eps);
    vcheck("V17", "所有组合的停手残差都不超 eps", worst <= eps, buf);
}

long verify_checks_run(void)
{
    return v_checks_run;
}

long verify_run(const char *outdir)
{
    printf("\n== 代入式核对：读回 data/*.dat 自己验定义式 ==\n");
    verify_case(outdir, "fine", "dirichlet", 0);
    verify_case(outdir, "fine", "periodic", 1);
    verify_case(outdir, "coarse", "dirichlet", 0);
    verify_case(outdir, "coarse", "periodic", 1);
    verify_global(outdir);
    printf("\n== 代入式核对：%ld 项，%ld 项失败 ==\n",
           v_checks_run, v_checks_failed);
    return v_checks_failed;
}
