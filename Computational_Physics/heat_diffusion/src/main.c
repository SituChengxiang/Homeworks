#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "config.h"
#include "material.h"
#include "grid.h"
#include "solver.h"
#include "analytic.h"
#include "crank.h"
#include "converge.h"
#include "export.h"
#include "verify.h"

/* 本步的检查计数器：任何一项 FAIL 都让程序非零退出 */
static long checks_run = 0;
static long checks_failed = 0;

static void check(const char *id, const char *what, int ok, const char *detail)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
    }
    printf("  [%s] %-4s %-46s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/*
 * 附图数据用的上限与缓存。分解表在 report_error_decomposition 里填、
 * 由 export_demo_data 写出：两个函数之间不传参，只靠这几个静态量。
 */
#define V_DEMO_NN 256
static double demo_dx[3];
static double demo_fmt[3];
static double demo_rep[3];
static double demo_tot[3];
static long demo_n = 0;

static void report_material(void)
{
    double a;
    double arc;
    double dev;
    char buf[160];

    a = material_alpha();
    arc = material_alpha_from_krc();
    dev = material_alpha_rel_dev();

    printf("== 材料：各向同性铁 ==\n");
    printf("  采用 alpha            = %.10e m^2/s (= %.4f mm^2/s)\n",
           a, a * 1e6);
    printf("  k/(rho*cp)            = %.10e m^2/s\n", arc);
    printf("    k = %.1f W/(m*K), rho = %.1f kg/m^3, cp = %.1f J/(kg*K)\n",
           IRON_K, IRON_RHO, IRON_CP);
    printf("  相对偏差              = %+.4f%%\n", 100.0 * dev);
    sprintf(buf, "表值与 k/(rho*cp) 差 %+.3f%%", 100.0 * dev);
    check("M1", "alpha 表值与 k/(rho*cp) 一致（2% 内）", fabs(dev) < 0.02, buf);

    printf("  扩散时间 tau（最低模衰减到 1/e）:\n");
    printf("    Dirichlet  l^2/(pi^2 alpha)   = %.6f s\n",
           material_tau_dirichlet(a, ROD_LENGTH));
    printf("    周期       l^2/(4 pi^2 alpha) = %.6f s\n",
           material_tau_periodic(a, ROD_LENGTH));
    printf("    比值 = %.1f（周期边界收敛快 4 倍）\n",
           material_tau_dirichlet(a, ROD_LENGTH)
               / material_tau_periodic(a, ROD_LENGTH));
    printf("  题目要画的时刻 10/20/30 s 相当于 Dirichlet 的 %.3f/%.3f/%.3f tau\n",
           SNAP_T1 / material_tau_dirichlet(a, ROD_LENGTH),
           SNAP_T2 / material_tau_dirichlet(a, ROD_LENGTH),
           SNAP_T3 / material_tau_dirichlet(a, ROD_LENGTH));
}

static int export_initial(const Grid *g, const double *naive, int periodic,
                          const char *outdir, const char *name)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# initial temperature field, finite-volume vs naive\n");
    fprintf(fp, "# length=%.17g dx=%.17g n=%ld periodic=%d\n",
            g->length, g->dx, g->n, periodic);
    fprintf(fp, "# hot=[%.17g,%.17g] hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# i x_i u_fv u_naive cell_width\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%ld %.17g %.17g %.17g %.17g\n",
                i, g->x[i], g->u[i], naive[i],
                grid_cell_width(g, i, periodic));
    }
    fclose(fp);
    return 0;
}

static int report_grid(double dx_req, const char *tag, const char *name,
                       const char *outdir)
{
    Grid g_dir;   /* Dirichlet 口径的有限体积初值 */
    Grid g_per;   /* 周期口径的有限体积初值（端节点控制体跨界回绕） */
    Grid g_naive; /* 负向对照：闭区间打点 */
    double *naive;
    double exact_heat;
    double heat_d;
    double heat_p;
    double mean_p;
    double sym;
    double r;
    double dtmax;
    double naive_heat;
    double umin;
    double umax;
    long i;
    long n_hot;
    long n_hot_naive;
    int rc;
    char buf[192];

    printf("\n== 网格与初值：%s（Dx = %.4f m = %.1f cm）==\n",
           tag, dx_req, dx_req * 100.0);

    /* 先清零：短路求值下后面的 grid_init 可能根本没被调用，
       而错误分支要对三个 Grid 都调 grid_free */
    memset(&g_dir, 0, sizeof(Grid));
    memset(&g_per, 0, sizeof(Grid));
    memset(&g_naive, 0, sizeof(Grid));

    if (grid_init(&g_dir, ROD_LENGTH, dx_req) != 0
            || grid_init(&g_per, ROD_LENGTH, dx_req) != 0
            || grid_init(&g_naive, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        grid_free(&g_dir);
        grid_free(&g_per);
        grid_free(&g_naive);
        return 1;
    }
    naive = (double *)malloc((size_t)g_dir.nn * sizeof(double));
    if (naive == NULL) {
        grid_free(&g_dir);
        grid_free(&g_per);
        grid_free(&g_naive);
        return 1;
    }

    printf("  n = %ld 单元, %ld 节点, 实际 dx = %.17g m\n",
           g_dir.n, g_dir.nn, g_dir.dx);
    printf("  高温段 [%.4f, %.4f] m，居中\n", HOT_LO, HOT_HI);

    /* 三份初值各自独立存放，互不覆盖 */
    grid_initial_fv(&g_dir, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    grid_initial_fv(&g_per, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 1);
    grid_initial_naive(&g_naive, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    for (i = 0; i < g_dir.nn; i++) {
        naive[i] = g_naive.u[i];
    }

    exact_heat = (HOT_TEMP - COLD_TEMP) * HOT_WIDTH + COLD_TEMP * ROD_LENGTH;
    heat_d = grid_heat_dirichlet(&g_dir);
    heat_p = grid_heat_periodic(&g_per);
    mean_p = grid_mean_periodic(&g_per);
    sym = grid_symmetry_defect(&g_dir);
    naive_heat = grid_heat_dirichlet(&g_naive);

    printf("  精确 int u0 dx        = %.12f 摄氏度*m\n", exact_heat);
    printf("  FV 热量（梯形权重）   = %.12f\n", heat_d);
    printf("  FV 热量（周期求和）   = %.12f\n", heat_p);
    printf("  FV 周期平均温度       = %.12f 摄氏度\n", mean_p);
    printf("  FV 对称性偏差         = %.3e\n", sym);
    printf("  naive 闭区间打点热量  = %.12f（偏差 %+.4f%%）\n",
           naive_heat, 100.0 * (naive_heat - exact_heat) / exact_heat);

    umin = g_dir.u[0];
    umax = g_dir.u[0];
    for (i = 1; i < g_dir.nn; i++) {
        if (g_dir.u[i] < umin) {
            umin = g_dir.u[i];
        }
        if (g_dir.u[i] > umax) {
            umax = g_dir.u[i];
        }
    }
    n_hot = grid_count_equal(&g_dir, HOT_TEMP, 1e-9);
    n_hot_naive = grid_count_equal(&g_naive, HOT_TEMP, 1e-9);
    printf("  FV 取值范围 [%.6f, %.6f]，恰好 100 度的节点 %ld 个\n",
           umin, umax, n_hot);
    printf("  naive 恰好 100 度的节点 %ld 个\n", n_hot_naive);

    printf("  非零节点（x, u_FV, u_naive）:\n   ");
    for (i = 0; i < g_dir.nn; i++) {
        if (g_dir.u[i] != COLD_TEMP || naive[i] != COLD_TEMP) {
            printf(" (%.4f,%.1f,%.1f)", g_dir.x[i], g_dir.u[i], naive[i]);
        }
    }
    printf("\n");

    dtmax = material_dt_max_ftcs(material_alpha(), g_dir.dx);
    r = material_fourier_number(material_alpha(), g_dir.dx, DEFAULT_DT);
    printf("  FTCS 稳定上限 dt_max  = dx^2/(2 alpha) = %.10f s\n", dtmax);
    printf("  自设 dt = %.4f s -> r = alpha*dt/dx^2 = %.10f\n", DEFAULT_DT, r);
    printf("  到 30 s 需要 %.0f 步\n", SNAP_T3 / DEFAULT_DT);

    /* ---- 本步验收 ---- */
    sprintf(buf, "n*dx=%.17g vs l=%.17g",
            (double)g_dir.n * g_dir.dx, ROD_LENGTH);
    check("G1", "网格正好铺满棍长",
          fabs((double)g_dir.n * g_dir.dx - ROD_LENGTH) < 1e-15, buf);

    sprintf(buf, "%.12f vs %.12f", heat_d, exact_heat);
    check("G2", "FV 初值热量 = int u0 dx（Dirichlet 权重）",
          fabs(heat_d - exact_heat) <= 1e-12 * exact_heat, buf);

    sprintf(buf, "%.12f vs %.12f", heat_p, exact_heat);
    check("G3", "FV 初值热量 = int u0 dx（周期求和）",
          fabs(heat_p - exact_heat) <= 1e-12 * exact_heat, buf);

    sprintf(buf, "mean=%.12f, 目标 %.1f", mean_p, exact_heat / ROD_LENGTH);
    check("G4", "周期平均温度 = 10 摄氏度（终态值）",
          fabs(mean_p - exact_heat / ROD_LENGTH) <= 1e-12, buf);

    sprintf(buf, "max|u_i-u_(n-i)| = %.3e", sym);
    check("G5", "初值关于棍中点对称", sym <= 1e-12, buf);

    sprintf(buf, "[%.6f, %.6f] 落在 [0,100] 内", umin, umax);
    check("G6", "初值不越界（离散极值原理的起点）",
          umin >= COLD_TEMP - 1e-12 && umax <= HOT_TEMP + 1e-12, buf);

    sprintf(buf, "r=%.8f <= 0.5, dt_max=%.8f s", r, dtmax);
    check("G7", "自设 dt 满足 FTCS 稳定性 r<=1/2", r <= FTCS_R_MAX, buf);

    sprintf(buf, "naive 热量 %.6f, 偏差 %+.4f%%", naive_heat,
            100.0 * (naive_heat - exact_heat) / exact_heat);
    printf("  [NOTE] %-4s %-46s %s\n", "G8",
           "naive 闭区间打点的热量偏差（负向对照，不是判据）", buf);

    rc = export_initial(&g_dir, naive, 0, outdir, name);

    free(naive);
    grid_free(&g_dir);
    grid_free(&g_per);
    grid_free(&g_naive);
    return rc;
}

/* ================= 步骤 2：显式 FTCS 推进 ================= */

static double field_max(const Grid *g)
{
    long i;
    double m;

    m = g->u[0];
    for (i = 1; i < g->nn; i++) {
        if (g->u[i] > m) {
            m = g->u[i];
        }
    }
    return m;
}

static double field_min(const Grid *g)
{
    long i;
    double m;

    m = g->u[0];
    for (i = 1; i < g->nn; i++) {
        if (g->u[i] < m) {
            m = g->u[i];
        }
    }
    return m;
}

static double field_max_abs(const Grid *g)
{
    long i;
    double m;
    double a;

    m = 0.0;
    for (i = 0; i < g->nn; i++) {
        a = fabs(g->u[i]);
        if (a > m) {
            m = a;
        }
    }
    return m;
}

/*
 * 逐步推进到 30 s，每一步都盯着四个不变量/单调量：
 *   周期：平均温度守恒（差分离散拉普拉斯在圆上求和为 0）
 *   Dirichlet：总热量单调流失（两端是热汇）
 *   两者：最大值不增、最小值不减（r<=1/2 时是凸组合，离散极值原理）
 *   两者：关于中点的对称性保持
 */
static void report_evolution(double dx_req, const char *tag,
                             BoundaryKind bc, const char *bctag)
{
    Grid g;
    Ftcs s;
    double mean0;
    double heat_prev;
    double heat_now;
    double max_prev;
    double min_prev;
    double max_now;
    double min_now;
    double drift;
    double sym;
    double worst_drift;
    double worst_sym;
    double terr;
    double tbound;
    long viol_max;
    long viol_min;
    long viol_heat;
    long step;
    long total_steps;
    long snap1;
    long snap2;
    long snap3;
    int periodic;
    int finite_ok;
    char buf[192];

    periodic = (bc == BC_PERIODIC);
    printf("\n== FTCS 演化：%s + %s（dt = %.4f s, r = %.6f）==\n",
           tag, bctag, DEFAULT_DT,
           material_fourier_number(material_alpha(), dx_req, DEFAULT_DT));

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        checks_run++;
        checks_failed++;
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), DEFAULT_DT) != 0) {
        fprintf(stderr, "ftcs_init failed\n");
        grid_free(&g);
        checks_run++;
        checks_failed++;
        return;
    }

    mean0 = periodic ? grid_mean_periodic(&g) : 0.0;
    heat_prev = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
    max_prev = field_max(&g);
    min_prev = field_min(&g);
    worst_drift = 0.0;
    worst_sym = grid_symmetry_defect(&g);
    viol_max = 0;
    viol_min = 0;
    viol_heat = 0;
    finite_ok = 1;

    total_steps = (long)(SNAP_T3 / DEFAULT_DT + 0.5);
    snap1 = (long)(SNAP_T1 / DEFAULT_DT + 0.5);
    snap2 = (long)(SNAP_T2 / DEFAULT_DT + 0.5);
    snap3 = total_steps;

    printf("  %8s %14s %14s %12s %12s\n",
           "t/s", periodic ? "周期均值" : "总热量", "对称性偏差",
           "max u", "min u");
    printf("  %8.2f %14.10f %14.3e %12.6f %12.6f\n",
           0.0, heat_prev, worst_sym, max_prev, min_prev);

    for (step = 1; step <= total_steps; step++) {
        ftcs_step(&s);
        if (!ftcs_all_finite(&s)) {
            finite_ok = 0;
            break;
        }
        max_now = field_max(&g);
        min_now = field_min(&g);
        heat_now = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
        sym = grid_symmetry_defect(&g);

        if (max_now > max_prev + 1e-10) {
            viol_max++;
        }
        if (min_now < min_prev - 1e-10) {
            viol_min++;
        }
        if (!periodic && heat_now > heat_prev + 1e-12) {
            viol_heat++;
        }
        if (periodic) {
            drift = fabs(heat_now / g.length - mean0);
            if (drift > worst_drift) {
                worst_drift = drift;
            }
        }
        if (sym > worst_sym) {
            worst_sym = sym;
        }

        if (step == snap1 || step == snap2 || step == snap3) {
            printf("  %8.2f %14.10f %14.3e %12.6f %12.6f\n",
                   s.time, heat_now, sym, max_now, min_now);
        }
        max_prev = max_now;
        min_prev = min_now;
        heat_prev = heat_now;
    }

    printf("  走完 %ld 步，t = %.10f s，末步 r = %.10f\n",
           s.steps, s.time, s.r_last);

    sprintf(buf, "全程有限 = %d", finite_ok);
    check("E1", "推进过程不出现 NaN/inf", finite_ok, buf);

    terr = fabs(s.time - SNAP_T3);
    /* 这里是手动 ftcs_step 逐步走，没有 ftcs_advance 的末刻吸附，
       所以 3000 次 time += dt 的舍入误差原样留着。顺序求和的误差
       上界约 n*u*T（u = DBL_EPSILON），留 4 倍余量当判据，
       而不是随手拍一个 1e-12。 */
    tbound = 4.0 * (double)total_steps * DBL_EPSILON * SNAP_T3;
    sprintf(buf, "步数 %ld/%ld，|t-30|=%.3e（求和上界 %.3e）",
            s.steps, total_steps, terr, tbound);
    check("E2", "步数精确、时刻误差在舍入上界内",
          s.steps == total_steps && terr <= tbound, buf);

    if (periodic) {
        sprintf(buf, "max|mean(t)-mean(0)| = %.3e（%ld 步）",
              worst_drift, s.steps);
        check("E3", "周期边界平均温度守恒", worst_drift <= 1e-10, buf);
    } else {
        sprintf(buf, "热量上升的次数 = %ld", viol_heat);
        check("E3", "Dirichlet 总热量单调不增", viol_heat == 0, buf);
    }

    sprintf(buf, "最大值上升 %ld 次，最小值下降 %ld 次", viol_max, viol_min);
    check("E4", "离散极值原理：max 不增、min 不减",
          viol_max == 0 && viol_min == 0, buf);

    sprintf(buf, "全程 max|u_i-u_(n-i)| = %.3e", worst_sym);
    check("E5", "对称性在演化中保持", worst_sym <= 1e-9, buf);

    ftcs_free(&s);
    grid_free(&g);
}

/*
 * 稳定性负向对照：把 r 依次取 0.23 / 0.5 / 0.575 / 0.69。
 * r<=0.5 必须稳，r>0.5 必须发散，而且发散速率要等于最高频模态的
 * 理论放大因子 |1-4r sin^2(q dx/2)|——光"炸了"不够，要炸得对。
 */
static void report_stability(double dx_req, const char *tag)
{
    Grid g;
    Ftcs s;
    double targets[4];
    double dt;
    double r;
    double qmax;
    double gtheory;
    double hist[401];
    double measured;
    double endmax;
    long i;
    long j;
    long step;
    long blown;
    int periodic;
    int finite_ok;
    char buf[192];

    printf("\n== 稳定性对照：%s（Dirichlet，最高模 k = n-1）==\n", tag);
    targets[0] = material_fourier_number(material_alpha(), dx_req, DEFAULT_DT);
    targets[1] = 0.5;
    targets[2] = 0.575;
    targets[3] = 0.69;
    periodic = 0;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        return;
    }
    qmax = (double)(g.n - 1) * HD_PI / ROD_LENGTH;
    printf("  最高模波数 q = (n-1)pi/l = %.6f 1/m，q*dx/2 = %.8f rad\n",
           qmax, qmax * g.dx / 2.0);
    printf("  %8s %12s %10s %14s %14s %12s\n",
           "r", "dt/s", "步数", "末态 max|u|", "理论|G|", "实测每步");

    for (i = 0; i < 4; i++) {
        r = targets[i];
        dt = r * dx_req * dx_req / material_alpha();
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt) != 0) {
            fprintf(stderr, "ftcs_init failed\n");
            continue;
        }
        gtheory = fabs(ftcs_growth_factor(r, qmax, g.dx));
        finite_ok = 1;
        blown = -1;

        if (r <= FTCS_R_MAX) {
            /* 稳定分支：推到 30 s，看是否始终有界 */
            if (ftcs_advance(&s, SNAP_T3) != 0) {
                finite_ok = 0;
            }
            endmax = field_max_abs(&g);
            printf("  %8.5f %12.8f %10ld %14.6e %14.8f %12s\n",
                   r, dt, s.steps, endmax, gtheory, "-");
            sprintf(buf, "r=%.5f: 30 s 后 max|u|=%.6f，有限=%d",
                    r, endmax, finite_ok);
            check("S1", "r<=1/2 稳定且不超过初值 100 度",
                  finite_ok && endmax <= HOT_TEMP + 1e-9, buf);
        } else {
            /* 发散分支 a：物理初值，记录它第几步炸穿 1e12。
               200 步内 r=0.575/0.69 都还不到 inf（约 1e22 / 1e49）。 */
            endmax = 0.0;
            for (step = 0; step <= 200; step++) {
                if (step > 0) {
                    ftcs_step(&s);
                }
                hist[step] = field_max_abs(&g);
                endmax = hist[step];
                if (!ftcs_all_finite(&s)) {
                    finite_ok = 0;
                    blown = step;
                    break;
                }
                if (hist[step] > 1e12 && blown < 0) {
                    blown = step;
                }
            }
            ftcs_free(&s);

            /*
             * 发散分支 b：换成精确特征向量 sin(qmax x)。
             * 物理初值是一堆模态的叠加，k=98 与 k=99 的 |G| 只差 0.13%，
             * 干涉会让 max|u| 的增长率偏离 |G_99| 约 1%（实测 1.2875 vs
             * 1.2994）。纯模态下 u_i^m = G^m sin(qmax x_i) 严格成立，
             * 比值就能对到舍入量级，判据也才敢收紧到 1e-9。
             */
            for (j = 0; j < g.nn; j++) {
                g.u[j] = sin(qmax * g.x[j]);
            }
            if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                          material_alpha(), dt) != 0) {
                fprintf(stderr, "ftcs_init failed\n");
                continue;
            }
            for (step = 0; step <= 200; step++) {
                if (step > 0) {
                    ftcs_step(&s);
                }
                hist[step] = field_max_abs(&g);
            }
            measured = pow(hist[200] / hist[100], 1.0 / 100.0);
            printf("  %8.5f %12.8f %10ld %14.6e %14.8f %12.8f\n",
                   r, dt, s.steps, endmax, gtheory, measured);
            sprintf(buf, "r=%.3f: 第 %ld 步越过 1e12；纯模态每步 %.10f vs 理论 %.10f",
                    r, blown, measured, gtheory);
            check("S2", "r>1/2 发散，纯模态速率严格等于 |G|",
                  blown > 0 && finite_ok == 1
                      && fabs(measured / gtheory - 1.0) < 1e-9, buf);
        }
        ftcs_free(&s);
    }
    grid_free(&g);
}

/*
 * 离散模态的精确核对：sin(q x_i) / cos(q x_i) 是差分拉普拉斯的
 * 精确特征向量，所以走 m 步后参考节点的比值必须等于 G^m，
 * 其中 G = 1-4r sin^2(q dx/2)。这是针对"离散格式"的解析结果，
 * 不依赖 PDE 的解，能独立抓住推进公式写错。
 * 同时列出连续解的 exp(-alpha q^2 t)，两者之差就是离散误差。
 */
static void report_mode(double dx_req, BoundaryKind bc,
                        const char *bctag, long k, long msteps)
{
    Grid g;
    Ftcs s;
    double q;
    double u0ref;
    double uref;
    double measured;
    double gfac;
    double gcont;
    double theory;
    double exact;
    long i;
    long ref;
    long mcap;
    char buf[192];

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        return;
    }
    q = (bc == BC_DIRICHLET) ? (double)k * HD_PI / ROD_LENGTH
                             : 2.0 * (double)k * HD_PI / ROD_LENGTH;
    for (i = 0; i < g.nn; i++) {
        g.u[i] = (bc == BC_DIRICHLET) ? sin(q * g.x[i]) : cos(q * g.x[i]);
    }
    ref = g.n / 2;
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), DEFAULT_DT) != 0) {
        fprintf(stderr, "ftcs_init failed\n");
        grid_free(&g);
        return;
    }
    u0ref = g.u[ref];
    gfac = ftcs_growth_factor(s.r, q, g.dx);
    gcont = exp(-material_alpha() * q * q * DEFAULT_DT);

    /*
     * 强阻尼模态不能硬走 1000 步：k=99 时 G=0.0802，G^1000 约 1e-1096，
     * double 直接下溢成 0，而舍入误差注入的伪低频模还停在 1e-16，
     * 于是"实测/理论"变成 0/0。按 |G|^m >= 1e-6 反推步数上限，
     * 让真信号始终比伪模高 10 个数量级。
     */
    if (fabs(gfac) < 1.0 && fabs(gfac) > 0.0) {
        mcap = (long)(log(1e-6) / log(fabs(gfac)));
        if (mcap < 1) {
            mcap = 1;
        }
        if (msteps > mcap) {
            msteps = mcap;
        }
    }
    for (i = 0; i < msteps; i++) {
        ftcs_step(&s);
    }
    uref = g.u[ref];
    measured = uref / u0ref;
    theory = pow(gfac, (double)msteps);
    exact = pow(gcont, (double)msteps);

    printf("  %-10s k=%-4ld q=%10.4f 1/m  走 %ld 步 (%.3f s)\n",
           bctag, k, q, msteps, (double)msteps * DEFAULT_DT);
    printf("      每步放大因子：离散 G = %+.10f，连续 exp(-alpha q^2 dt) = %.10f\n",
           gfac, gcont);
    printf("      离散/连续 = %.12f（%+.4e %%）<- 这就是空间离散误差\n",
           gfac / gcont, 100.0 * (gfac / gcont - 1.0));
    printf("      实测 u/u0 = %.12e   理论 G^m = %.12e   连续解 = %.12e\n",
           measured, theory, exact);
    printf("      实测 vs 理论 相对差 = %.3e\n", fabs(measured / theory - 1.0));

    sprintf(buf, "k=%ld: 实测/理论-1 = %.3e（走 %ld 步）",
            k, fabs(measured / theory - 1.0), msteps);
    check("D1", "单模衰减等于离散特征值 G^m",
          theory > 0.0 && fabs(measured / theory - 1.0) < 1e-9, buf);

    ftcs_free(&s);
    grid_free(&g);
}

/* ================= 步骤 3：Fourier 级数解析解 ================= */

/* 把 FTCS 推到 t，返回与解析解的逐点最大误差；rms_out 可传 NULL */
static double ftcs_max_error(double dx_req, double dt, double t,
                             BoundaryKind bc, double *rms_out)
{
    Grid g;
    Ftcs s;
    double err;
    double worst;
    double sumsq;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
        grid_free(&g);
        return -1.0;
    }
    if (ftcs_advance(&s, t) != 0) {
        grid_free(&g);
        return -1.0;
    }

    worst = 0.0;
    sumsq = 0.0;
    for (i = 0; i < g.nn; i++) {
        err = g.u[i] - analytic_u(g.x[i], t, 0, periodic);
        if (fabs(err) > worst) {
            worst = fabs(err);
        }
        sumsq += err * err;
    }
    if (rms_out != NULL) {
        *rms_out = sqrt(sumsq / (double)g.nn);
    }
    ftcs_free(&s);
    grid_free(&g);
    return worst;
}

static void report_analytic_coeffs(void)
{
    long k;
    double ba;
    double bn;
    double pa;
    double pn;
    double rel;
    double worst_dir;
    double worst_per;
    double mean;
    char buf[192];

    printf("\n== 解析解的 Fourier 系数：解析式 vs 复合 Simpson 数值投影 ==\n");
    printf("  %4s %18s %18s %12s   %18s %18s %12s\n",
           "k", "b_k 解析", "b_k Simpson", "相对差",
           "A_k 解析", "A_k Simpson", "相对差");

    worst_dir = 0.0;
    worst_per = 0.0;
    for (k = 1; k <= 8; k++) {
        ba = analytic_b_dirichlet(k);
        bn = analytic_project_dirichlet(k, 2000);
        pa = analytic_a_periodic(k);
        pn = analytic_project_periodic(k, 2000);
        /* 恒零系数没法比相对值：|解析式| 低于 1e-10 时改比绝对差 */
        rel = (fabs(ba) > 1e-10) ? fabs(bn / ba - 1.0) : fabs(bn - ba);
        if (rel > worst_dir) {
            worst_dir = rel;
        }
        rel = (fabs(pa) > 1e-10) ? fabs(pn / pa - 1.0) : fabs(pn - pa);
        if (rel > worst_per) {
            worst_per = rel;
        }
        printf("  %4ld %18.12f %18.12f %12.2e   %18.12f %18.12f %12.2e\n",
               k, ba, bn, rel, pa, pn, rel);
    }

    sprintf(buf, "Dirichlet 最大偏差 %.3e", worst_dir);
    check("A1", "解析系数 = Simpson 数值投影（正弦基）", worst_dir < 1e-10, buf);
    sprintf(buf, "周期最大偏差 %.3e", worst_per);
    check("A2", "解析系数 = Simpson 数值投影（余弦基）", worst_per < 1e-10, buf);

    /* 对称性决定的恒零系数：初值关于中点对称 */
    printf("  对称性导致的恒零系数：b_2 = %.3e, b_4 = %.3e, b_6 = %.3e, b_8 = %.3e\n",
           analytic_b_dirichlet(2), analytic_b_dirichlet(4),
           analytic_b_dirichlet(6), analytic_b_dirichlet(8));
    sprintf(buf, "max 偶数项 |b_k| = %.3e", fabs(analytic_b_dirichlet(8)));
    check("A3", "Dirichlet 偶数项恒为 0（初值关于中点对称）",
          fabs(analytic_b_dirichlet(2)) < 1e-12
              && fabs(analytic_b_dirichlet(4)) < 1e-12
              && fabs(analytic_b_dirichlet(6)) < 1e-12
              && fabs(analytic_b_dirichlet(8)) < 1e-12, buf);

    mean = analytic_mean_periodic();
    sprintf(buf, "零模 = %.12f，期望 (100*0.01)/0.1 = 10", mean);
    check("A4", "周期零模 = 平均温度 = 终态值", fabs(mean - 10.0) < 1e-12, buf);
}

/* t=0 的截断级数（不含时间因子），用来单独考察级数本身的收敛性 */
static double series_t0(double x, long K)
{
    long k;
    double sum;

    sum = 0.0;
    for (k = 1; k <= K; k++) {
        sum += analytic_b_dirichlet(k) * sin(analytic_wavenumber(k, 0) * x);
    }
    return sum;
}

/*
 * t=0 截断级数在跳跃点右侧的峰值。
 *
 * 关键：Gibbs 峰到间断点的距离按 l/K 收缩（K=1e5 时只剩 1e-6 量级），
 * 固定步长扫描会把峰整个漏掉。实测用固定 2e-6 步长扫 K=2e5，
 * 读出"峰顶 99.998、过冲 -0.002%"——那是采样伪影，不是物理。
 * 所以窗口取 8*l/K、点数固定 800，步长随 K 自适应。
 */
static double series_t0_peak(long K, long np)
{
    double win;
    double step;
    double v;
    double worst;
    long j;

    win = 8.0 * ROD_LENGTH / (double)K;
    step = win / (double)np;
    worst = series_t0(HOT_LO, K);
    for (j = 1; j < np; j++) {
        v = series_t0(HOT_LO + (double)j * step, K);
        if (v > worst) {
            worst = v;
        }
    }
    return worst;
}

static void report_analytic_series(void)
{
    double n_auto;
    double u_auto;
    double u_double;
    double s1;
    double s2;
    double ext;
    double ideal;
    double m1;
    double m2;
    double e1;
    double e2;
    double mid;
    long K1;
    long K2;
    char buf[192];

    printf("\n== 级数收敛性与 t=0 的 Gibbs 过冲 ==\n");

    /* A5：自动项数够不够——翻倍再比 */
    n_auto = (double)analytic_nmax(SNAP_T1, 1e-12, 0);
    u_auto = analytic_u(ROD_LENGTH / 2.0, SNAP_T1, (long)n_auto, 0);
    u_double = analytic_u(ROD_LENGTH / 2.0, SNAP_T1, (long)(2.0 * n_auto), 0);
    printf("  t=10 s 自动项数 = %.0f；x=l/2 处 %.0f 项 = %.14f，%.0f 项 = %.14f\n",
           n_auto, n_auto, u_auto, 2.0 * n_auto, u_double);
    sprintf(buf, "项数翻倍差 %.3e", fabs(u_auto - u_double));
    check("A5", "级数项数足够（翻倍后不变）",
          fabs(u_auto - u_double) < 1e-12, buf);

    /*
     * A6：Gibbs 过冲。截断级数的峰值 S(K) = S_inf + c/K + O(1/K^2)，
     * 取两个 K 做 Richardson 外推把 1/K 项消掉，再和理论值比。
     * 直接拿单个 K 的读数卡容差，等于把级数截断误差算进物理常数。
     */
    K1 = 10000;
    K2 = 40000;
    s1 = 100.0 * (series_t0_peak(K1, 800) / HOT_TEMP - 1.0);
    s2 = 100.0 * (series_t0_peak(K2, 800) / HOT_TEMP - 1.0);
    ext = (s1 * (double)K1 - s2 * (double)K2) / ((double)K1 - (double)K2);
    /* Si(pi)/pi - 1/2 = 0.089489872236084，由 sin(t)/t 数值积分独立算得 */
    ideal = 8.9489872236084;
    printf("  过冲 K=%-6ld: %.6f%%   K=%-6ld: %.6f%%   1/K 外推: %.6f%%   理论: %.6f%%\n",
           K1, s1, K2, s2, ext, ideal);
    printf("  两档读数之差 %.6f 个百分点；外推后与理论差 %.6f 个百分点\n",
           s1 - s2, ext - ideal);
    sprintf(buf, "外推 %.6f%% vs 理论 %.6f%%", ext, ideal);
    check("A6", "t=0 过冲收敛到 Wilbraham–Gibbs 常数",
          fabs(ext - ideal) < 0.0005, buf);

    /* A7：跳跃点本身收敛到跳变中值，且误差严格按 1/K 缩小 */
    mid = (HOT_TEMP + COLD_TEMP) / 2.0;
    m1 = series_t0(HOT_LO, K1);
    m2 = series_t0(HOT_LO, K2);
    e1 = fabs(m1 - mid);
    e2 = fabs(m2 - mid);
    printf("  t=0, x=0.045（跳跃处）：K=%ld 读 %.6f，K=%ld 读 %.6f，跳变中值 %.1f\n",
           K1, m1, K2, m2, mid);
    printf("  误差 %.4e -> %.4e，比值 %.4f（1/K 律应为 %.1f）\n",
           e1, e2, e1 / e2, (double)K2 / (double)K1);
    sprintf(buf, "K=%ld 读 %.6f，误差比 %.4f vs 期望 %.1f",
            K2, m2, e1 / e2, (double)K2 / (double)K1);
    check("A7", "跳跃点收敛到跳变中值，误差按 1/K",
          e2 < 0.01 && fabs(e1 / e2 - (double)K2 / (double)K1) < 0.05, buf);

    /* A8：终态 */
    printf("  t=10000 s：Dirichlet u(l/2) = %.3e（终态 0），周期 u(l/2) = %.12f（终态 10）\n",
           analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0),
           analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1));
    sprintf(buf, "|u_D(1e4)| = %.3e，|u_P(1e4)-10| = %.3e",
            fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0)),
            fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1) - 10.0));
    check("A8", "解析解的终态：Dirichlet 归 0、周期归 10",
          fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0)) < 1e-12
              && fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1) - 10.0) < 1e-12, buf);
}

static void report_analytic_vs_ftcs(void)
{
    double dxs[2];
    const char *names[2];
    double snaps[3];
    long i;
    long j;
    double err;
    double rms;
    BoundaryKind bc;

    dxs[0] = DEFAULT_DX;
    dxs[1] = COARSE_DX;
    names[0] = "dx=0.1cm";
    names[1] = "dx=0.2cm";
    snaps[0] = SNAP_T1;
    snaps[1] = SNAP_T2;
    snaps[2] = SNAP_T3;

    printf("\n== FTCS 数值解 vs 解析解（dt = %.4f s，逐点最大误差 / RMS，摄氏度）==\n",
           DEFAULT_DT);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            bc = BC_DIRICHLET;
            err = ftcs_max_error(dxs[i], DEFAULT_DT, snaps[j], bc, &rms);
            printf("  %-9s Dirichlet t=%5.1f s  max|err| = %11.5e   rms = %11.5e\n",
                   names[i], snaps[j], err, rms);
            bc = BC_PERIODIC;
            err = ftcs_max_error(dxs[i], DEFAULT_DT, snaps[j], bc, &rms);
            printf("  %-9s 周期      t=%5.1f s  max|err| = %11.5e   rms = %11.5e\n",
                   names[i], snaps[j], err, rms);
        }
    }
    printf("  误差量级 1e-3 ~ 1e-2：这是格式误差，不是 bug——\n");
    printf("  解析解是连续 PDE 的精确解，FTCS 只有一阶时间 + 二阶空间精度。\n");
}

/*
 * 纯单模初值下 FTCS 与精确 PDE 解的最大误差。
 * 初值本身就是差分算子的特征函数，所以没有"初值表示误差"，
 * 读数只剩时间离散 + 空间离散两项，收敛阶才读得干净。
 */
static double ftcs_mode_max_error(double dx_req, double dt, double t,
                                  BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs s;
    double q;
    double ref;
    double err;
    double worst;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    q = analytic_wavenumber(modek, periodic);
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt) != 0) {
        grid_free(&g);
        return -1.0;
    }
    if (ftcs_advance(&s, t) != 0) {
        grid_free(&g);
        return -1.0;
    }
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        ref = exp(-material_alpha() * q * q * t)
              * (periodic ? cos(q * g.x[i]) : sin(q * g.x[i]));
        err = g.u[i] - ref;
        if (fabs(err) > worst) {
            worst = fabs(err);
        }
    }
    ftcs_free(&s);
    grid_free(&g);
    return worst;
}

/*
 * 同一 dx、两种 dt（dt 与 dt/2）的两个数值解之差。
 *
 * 为什么要这样测：FTCS 的稳定性把 dt 和 dx^2 绑在一起，于是
 *   时间误差 / 空间误差 = 12 alpha dt / dx^2 = 12 r <= 6
 * 与模态、网格都无关。也就是说在固定 dx 下，时间误差最多只有空间
 * 误差的 6 倍，细化 dt 必然撞上空间地板，"与精确解比"读不出干净阶数。
 * 两个同网格解相减，空间误差完全相同、自动抵消，剩下的就是纯时间误差。
 */
static double ftcs_time_diff(double dx_req, double dt, double t,
                             BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs s;
    double q;
    double *ua;
    double worst;
    double err;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    q = analytic_wavenumber(modek, periodic);

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    ua = (double *)malloc((size_t)g.nn * sizeof(double));
    if (ua == NULL) {
        grid_free(&g);
        return -1.0;
    }

    /* 第一次：dt */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt) != 0
        || ftcs_advance(&s, t) != 0) {
        free(ua);
        grid_free(&g);
        return -1.0;
    }
    memcpy(ua, g.u, (size_t)g.nn * sizeof(double));
    ftcs_free(&s);

    /* 第二次：dt/2，同一网格、同一初值 */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt / 2.0) != 0
        || ftcs_advance(&s, t) != 0) {
        free(ua);
        grid_free(&g);
        return -1.0;
    }
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        err = fabs(g.u[i] - ua[i]);
        if (err > worst) {
            worst = err;
        }
    }
    ftcs_free(&s);
    free(ua);
    grid_free(&g);
    return worst;
}

/*
 * 离散 FV 初值当作分段常数函数时的精确正弦系数：
 *   FV(x) = u_i 于控制体 [x_i-dx/2, x_i+dx/2] ∩ [0,l]
 *   b_k^FV = (2/l) sum_i u_i [cos(q a_i) - cos(q c_i)] / q
 *
 * 有了它就能把"数值 vs 解析"的总误差拆成两块：
 *   格式误差   = FTCS(FV初值) - 精确演化(FV初值)   同一份初值，只差格式
 *   表示误差   = 精确演化(FV初值) - 精确演化(方波)  同一套解析演化，只差初值
 * 两者相加正好是总误差。
 */
static double fv_coeff_dirichlet(const Grid *g, long k)
{
    double q;
    double a;
    double b;
    double sum;
    long i;

    q = analytic_wavenumber(k, 0);
    sum = 0.0;
    for (i = 0; i < g->nn; i++) {
        a = g->x[i] - g->dx / 2.0;
        b = g->x[i] + g->dx / 2.0;
        if (a < 0.0) {
            a = 0.0;
        }
        if (b > g->length) {
            b = g->length;
        }
        sum += g->u[i] * (cos(q * a) - cos(q * b)) / q;
    }
    return (2.0 / ROD_LENGTH) * sum;
}

/* 用给定系数集在节点上求 t 时刻的精确演化值 */
static void evolve_coeffs(const Grid *g, const double *b, long nb, double t,
                          double *out)
{
    long i;
    long k;
    double q;
    double v;

    for (i = 0; i < g->nn; i++) {
        v = 0.0;
        for (k = 1; k <= nb; k++) {
            q = analytic_wavenumber(k, 0);
            v += b[k - 1] * sin(q * g->x[i]) * exp(-q * q * IRON_ALPHA * t);
        }
        out[i] = v;
    }
}

#define DECOMP_KMAX 64
#define DECOMP_NMAX 512

static void report_error_decomposition(void)
{
    static double bp[DECOMP_KMAX];
    double bf[DECOMP_KMAX];
    double a_ex[DECOMP_NMAX];
    double a_fv[DECOMP_NMAX];
    Grid g;
    Ftcs s;
    double dts[4];
    double dxs[3];
    double fmt;
    double rep;
    double tot;
    double fmt_max;
    double rep_max;
    double tot_max;
    double rep_at_aligned;
    double dt_i;
    double last_sign;
    double first_sign;
    int sign_changed;
    long i;
    long j;
    long k;
    long nb;
    long worst_i;
    int rc;
    char buf[192];

    printf("\n== 总误差的分解：格式误差 vs 初值表示误差（Dirichlet, t = %.0f s）==\n",
           SNAP_T1);

    nb = DECOMP_KMAX;
    for (k = 1; k <= nb; k++) {
        bp[k - 1] = analytic_b_dirichlet(k);
    }

    printf("  %8s %8s %14s %14s %14s %10s\n",
           "dx", "dt", "格式误差max", "表示误差max", "总误差max", "表示占比");
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    rep_at_aligned = -1.0;
    for (i = 0; i < 3; i++) {
        /* dt 随 dx^2 缩，保持 r = 0.23：直接用 DEFAULT_DT 的话粗网格
           稳定、细网格反而 r>1/2 发散，三档之间根本没有可比性 */
        dt_i = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT)
               * dxs[i] * dxs[i] / material_alpha();
        memset(&g, 0, sizeof(Grid));
        if (grid_init(&g, ROD_LENGTH, dxs[i]) != 0) {
            return;
        }
        if (g.nn > DECOMP_NMAX) {
            printf("      dx=%.4f 节点数 %ld 超出分解数组，跳过\n", dxs[i], g.nn);
            grid_free(&g);
            continue;
        }
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        for (k = 1; k <= nb; k++) {
            bf[k - 1] = fv_coeff_dirichlet(&g, k);
        }
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt_i) != 0) {
            grid_free(&g);
            return;
        }
        rc = ftcs_advance(&s, SNAP_T1);
        if (rc != 0) {
            printf("      dx=%.4f 推进失败 rc=%d\n", dxs[i], rc);
            ftcs_free(&s);
            grid_free(&g);
            continue;
        }
        evolve_coeffs(&g, bp, nb, SNAP_T1, a_ex);
        evolve_coeffs(&g, bf, nb, SNAP_T1, a_fv);

        tot_max = 0.0;
        fmt_max = 0.0;
        rep_max = 0.0;
        worst_i = 0;
        for (j = 0; j < g.nn; j++) {
            tot = fabs(g.u[j] - a_ex[j]);
            if (tot > tot_max) {
                tot_max = tot;
                worst_i = j;
            }
            fmt = fabs(g.u[j] - a_fv[j]);
            if (fmt > fmt_max) {
                fmt_max = fmt;
            }
            rep = fabs(a_fv[j] - a_ex[j]);
            if (rep > rep_max) {
                rep_max = rep;
            }
        }
        printf("  %8.4f %8.5f %14.4e %14.4e %14.4e %9.1f%%\n",
               dxs[i], dt_i, fmt_max, rep_max, tot_max,
               100.0 * rep_max / (rep_max + fmt_max));
        if (i == 0) {
            rep_at_aligned = rep_max;
        }
        if (i < 3) {
            demo_dx[i] = dxs[i];
            demo_fmt[i] = fmt_max;
            demo_rep[i] = rep_max;
            demo_tot[i] = tot_max;
            demo_n = i + 1;
        }
        ftcs_free(&s);
        grid_free(&g);
    }
    printf("  dx=0.002 那一档表示误差恒等于 0：高温段边界 0.045/0.055 正好落在单元\n");
    printf("  边界上，分段常数的 FV 重构与精确方波是同一个函数，系数逐位相同。\n");
    printf("  dx=0.001 时边界落在节点中心，重构多出两个 50 度的半格，表示误差比\n");
    printf("  格式误差大一个量级——总误差由初值表示主导，而不是由格式主导。\n");

    sprintf(buf, "dx=0.002 表示误差 = %.3e", rep_at_aligned);
    check("A13", "边界与单元对齐时初值表示误差恒为 0",
          rep_at_aligned >= 0.0 && rep_at_aligned < 1e-10, buf);

    /* 格式误差随 dt 变号：时间项过阻尼（+）、空间项欠阻尼（-），两者反号 */
    printf("\n  dx = 0.001 m 固定，扫 dt 看格式误差变号：\n");
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    for (k = 1; k <= nb; k++) {
        bf[k - 1] = fv_coeff_dirichlet(&g, k);
    }
    evolve_coeffs(&g, bp, nb, SNAP_T1, a_ex);
    evolve_coeffs(&g, bf, nb, SNAP_T1, a_fv);

    rep = 0.0;
    worst_i = 0;
    for (j = 0; j < g.nn; j++) {
        if (fabs(a_fv[j] - a_ex[j]) > fabs(rep)) {
            rep = a_fv[j] - a_ex[j];
            worst_i = j;
        }
    }
    printf("      表示误差在 x=%.4f 处 = %+.6e（与 dt 无关）\n",
           g.x[worst_i], rep);
    printf("      %8s %8s %16s %16s\n", "dt", "r", "该点格式误差", "该点总误差");
    first_sign = 0.0;
    last_sign = 0.0;
    sign_changed = 0;
    for (i = 0; i < 4; i++) {
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dts[i]) != 0) {
            continue;
        }
        if (ftcs_advance(&s, SNAP_T1) != 0) {
            printf("      dt=%.5f 推进失败（可能 r>1/2）\n", dts[i]);
            ftcs_free(&s);
            continue;
        }
        fmt = g.u[worst_i] - a_fv[worst_i];
        printf("      %8.5f %8.4f %+16.6e %+16.6e   %s\n",
               dts[i], s.r, fmt, g.u[worst_i] - a_ex[worst_i],
               (fmt * rep > 0.0) ? "与表示误差同号" : "与表示误差反号(抵消)");
        if (fmt != 0.0) {
            if (first_sign == 0.0) {
                first_sign = fmt;
            }
            if (last_sign != 0.0 && fmt * last_sign < 0.0) {
                sign_changed = 1;
            }
            last_sign = fmt;
        }
        ftcs_free(&s);
    }
    grid_free(&g);
    printf("      格式误差在 dt 扫描中%s变号：时间项与空间项反号，所以固定 dx\n",
           sign_changed ? "" : "没有");
    printf("      细化 dt 时总误差不是干净地按 2 倍下降，而是先穿过一个\n");
    printf("      \"格式误差本身几乎为零\"的点（见上面收敛阶 (4) 的 2.30/2.85/1.20）。\n");

    sprintf(buf, "变号 = %d，首末 %+.3e -> %+.3e",
            sign_changed, first_sign, last_sign);
    check("A14", "格式误差随 dt 变号（时间项与空间项反号）",
          sign_changed == 1, buf);
}

static void report_convergence_order(void)
{
    double dts[4];
    double dxs[3];
    double errs[4];
    double e0;
    double e1;
    double e2;
    double r_fixed;
    double dx_fine;
    double p0;
    double p1;
    double p2;
    long i;
    char buf[192];

    printf("\n== 收敛阶 ==\n");
    printf("  方波初值同时带着三种误差：时间离散 O(dt)、空间离散 O(dx^2)、\n");
    printf("  以及\"离散初值 vs 精确方波\"的表示误差。三者混在一起，比值不干净。\n");
    printf("  所以先用纯单模初值把格式阶数读干净，再回到方波如实报告。\n");

    /* ---- (1) 时间一阶：同网格两种 dt 相减，空间误差自动抵消 ---- */
    dx_fine = DEFAULT_DX;
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    printf("\n  (1) 纯模 Dirichlet k=3，dx = %.4f m 固定，同网格 dt 与 dt/2 相减：\n",
           dx_fine);
    printf("      时间误差/空间误差 = 12r；dt=%.4f 时 r=%.4f，只差 %.2f 倍，\n",
           DEFAULT_DT, material_fourier_number(material_alpha(), dx_fine, DEFAULT_DT),
           12.0 * material_fourier_number(material_alpha(), dx_fine, DEFAULT_DT));
    printf("      所以\"与精确解比\"读不出时间阶，必须同网格相减。\n");
    printf("  %10s %10s %16s\n", "dt/s", "r", "|u(dt)-u(dt/2)|");
    for (i = 0; i < 4; i++) {
        errs[i] = ftcs_time_diff(dx_fine, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %10.5f %10.5f %16.6e", dts[i],
               material_fourier_number(material_alpha(), dx_fine, dts[i]),
               errs[i]);
        if (i > 0) {
            printf("   相邻比 %.4f", errs[i - 1] / errs[i]);
        }
        printf("\n");
    }
    sprintf(buf, "相邻比 %.4f / %.4f / %.4f",
            errs[0] / errs[1], errs[1] / errs[2], errs[2] / errs[3]);
    check("A9", "时间一阶收敛（dt 减半差值减半）",
          fabs(errs[0] / errs[1] - 2.0) < 0.02
              && fabs(errs[1] / errs[2] - 2.0) < 0.02
              && fabs(errs[2] / errs[3] - 2.0) < 0.02, buf);

    /* ---- (2) 空间二阶：r 固定，dt = r dx^2/alpha 随 dx^2 一起缩 ---- */
    r_fixed = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT);
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    printf("\n  (2) 纯模 Dirichlet k=3，r 固定 = %.4f，dx 减半：\n", r_fixed);
    e0 = ftcs_mode_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    e1 = ftcs_mode_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    e2 = ftcs_mode_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    printf("      dx=0.0020 err=%.6e | dx=0.0010 err=%.6e | dx=0.0005 err=%.6e\n",
           e0, e1, e2);
    printf("      误差比 %.4f 与 %.4f（二阶应为 4.000）\n", e0 / e1, e1 / e2);
    sprintf(buf, "误差比 %.4f / %.4f", e0 / e1, e1 / e2);
    check("A10", "纯模下空间二阶收敛（dx 减半误差降 4 倍）",
          fabs(e0 / e1 - 4.0) < 0.2 && fabs(e1 / e2 - 4.0) < 0.2, buf);

    /* ---- (3) 周期边界同样跑 ---- */
    e0 = ftcs_mode_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    e1 = ftcs_mode_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    e2 = ftcs_mode_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    printf("  (3) 纯模 周期 k=1，r 固定：err=%.6e / %.6e / %.6e，误差比 %.4f 与 %.4f\n",
           e0, e1, e2, e0 / e1, e1 / e2);
    sprintf(buf, "周期误差比 %.4f", e1 / e2);
    check("A11", "周期边界同样二阶", fabs(e1 / e2 - 4.0) < 0.2, buf);

    /* ---- (4) 物理方波：如实报告，比值不干净是三种误差混在一起 ---- */
    printf("\n  (4) 物理方波初值（dx = 0.001 m），dt 减半：\n");
    for (i = 0; i < 4; i++) {
        errs[i] = ftcs_max_error(DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, NULL);
        printf("      dt=%.5f err=%.6e%s\n", dts[i], errs[i],
               i > 0 ? "" : "");
        if (i > 0) {
            printf("               相邻比 %.4f", errs[i - 1] / errs[i]);
            if (i == 3) {
                printf("  <- 见上面的误差分解：格式误差在此已与表示误差反号抵消");
            }
        }
        printf("\n");
    }
    printf("  (5) 物理方波初值，r 固定 dx 减半：\n");
    p0 = ftcs_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    p1 = ftcs_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    p2 = ftcs_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    printf("      dx=0.0020 err=%.6e | dx=0.0010 err=%.6e | dx=0.0005 err=%.6e\n",
           p0, p1, p2);
    printf("      误差比 %.4f 与 %.4f\n", p0 / p1, p1 / p2);
    printf("      dx=0.002 那一档反而更小，这不是二阶失效——见上面的误差分解：\n");
    printf("      该档高温段边界正好落在单元边界上，初值表示误差恒等于 0，\n");
    printf("      总误差只剩格式误差；而 dx=0.001 的总误差由表示误差主导。\n");
    printf("      纯模的 (2) 里没有初值误差，同一组 dx 才给出干净的 4.00。\n");

    /* 方波误差随 t 单调下降：活着的高频模越来越少 */
    e0 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_DIRICHLET, NULL);
    e1 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T2, BC_DIRICHLET, NULL);
    e2 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T3, BC_DIRICHLET, NULL);
    printf("  (6) 方波误差随时间下降：t=10 %.4e -> 20 %.4e -> 30 %.4e\n",
           e0, e1, e2);
    sprintf(buf, "%.3e -> %.3e -> %.3e", e0, e1, e2);
    check("A12", "方波误差随 t 单调下降（高频模先死掉）",
          e0 > e1 && e1 > e2, buf);
}

/* ================= 步骤 4：Crank-Nicolson 对照 ================= */

/*
 * 两个格式共用的误差测量。which: 0 = FTCS，1 = Crank-Nicolson。
 * modek <= 0 用物理方波初值；modek > 0 用纯单模初值（去掉初值表示误差）。
 * 返回负数表示推进失败（例如 FTCS 的 r > 1/2）。
 */
static double scheme_error(int which, double dx_req, double dt, double t,
                           BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    double q;
    double ref;
    double err;
    double worst;
    long i;
    int periodic;
    int rc;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    if (modek <= 0) {
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    } else {
        q = analytic_wavenumber(modek, periodic);
        for (i = 0; i < g.nn; i++) {
            g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
        }
    }

    if (which == 0) {
        if (ftcs_init(&fs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1.0;
        }
        rc = ftcs_advance(&fs, t);
    } else {
        if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1.0;
        }
        rc = crank_advance(&cs, t);
    }
    if (rc != 0) {
        if (which == 0) {
            ftcs_free(&fs);
        } else {
            crank_free(&cs);
        }
        grid_free(&g);
        return -1.0;
    }

    q = (modek > 0) ? analytic_wavenumber(modek, periodic) : 0.0;
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        if (modek <= 0) {
            ref = analytic_u(g.x[i], t, 0, periodic);
        } else {
            ref = exp(-material_alpha() * q * q * t)
                  * (periodic ? cos(q * g.x[i]) : sin(q * g.x[i]));
        }
        err = fabs(g.u[i] - ref);
        if (err > worst) {
            worst = err;
        }
    }
    if (which == 0) {
        ftcs_free(&fs);
    } else {
        crank_free(&cs);
    }
    grid_free(&g);
    return worst;
}

/*
 * 同一 dx、dt 与 dt/2 两个数值解之差。
 * 空间离散误差与初值表示误差在两边完全相同，相减自动抵消，
 * 读数就是纯时间误差，才能干净地判时间收敛阶。
 */
static double scheme_time_diff(int which, double dx_req, double dt, double t,
                               BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    double q;
    double *ua;
    double worst;
    double err;
    double step_dt;
    long i;
    int pass;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    ua = (double *)malloc((size_t)g.nn * sizeof(double));
    if (ua == NULL) {
        grid_free(&g);
        return -1.0;
    }
    worst = -1.0;

    for (pass = 0; pass < 2; pass++) {
        step_dt = (pass == 0) ? dt : dt / 2.0;

        if (modek <= 0) {
            grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
        } else {
            q = analytic_wavenumber(modek, periodic);
            for (i = 0; i < g.nn; i++) {
                g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
            }
        }

        if (which == 0) {
            if (ftcs_init(&fs, &g, bc, COLD_TEMP, material_alpha(), step_dt) != 0
                || ftcs_advance(&fs, t) != 0) {
                free(ua);
                grid_free(&g);
                return -1.0;
            }
            ftcs_free(&fs);
        } else {
            if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(),
                           step_dt) != 0
                || crank_advance(&cs, t) != 0) {
                free(ua);
                grid_free(&g);
                return -1.0;
            }
            crank_free(&cs);
        }

        if (pass == 0) {
            memcpy(ua, g.u, (size_t)g.nn * sizeof(double));
        } else {
            worst = 0.0;
            for (i = 0; i < g.nn; i++) {
                err = fabs(g.u[i] - ua[i]);
                if (err > worst) {
                    worst = err;
                }
            }
        }
    }
    free(ua);
    grid_free(&g);
    return worst;
}

/* CN 推到 t 后的最大最小值。返回 0 表示成功跑完 */
static int crank_extremes(double dx_req, double dt, double t,
                          BoundaryKind bc, double *pmax, double *pmin)
{
    Grid g;
    Crank cs;
    int periodic;
    int rc;

    periodic = (bc == BC_PERIODIC);
    *pmax = 0.0;
    *pmin = 0.0;
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return 1;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
        grid_free(&g);
        return 2;
    }
    rc = crank_advance(&cs, t);
    if (rc == 0 && !crank_all_finite(&cs)) {
        rc = 3;
    }
    if (rc == 0) {
        *pmax = field_max(&g);
        *pmin = field_min(&g);
    }
    crank_free(&cs);
    grid_free(&g);
    return rc;
}

static void report_crank_mode(void)
{
    Grid g;
    Crank cs;
    double q;
    double gfac;
    double gcont;
    double u0ref;
    double measured;
    double theory;
    double msteps;
    double mcap;
    long c;
    long i;
    long ref;
    long ks[3];
    int pers[3];
    const char *labels[3];
    char buf[192];

    printf("\n== CN 的离散特征值：G = (1 - 2r s^2)/(1 + 2r s^2) ==\n");
    ks[0] = 1;  pers[0] = 0; labels[0] = "Dirichlet k=1 ";
    ks[1] = 99; pers[1] = 0; labels[1] = "Dirichlet k=99";
    ks[2] = 1;  pers[2] = 1; labels[2] = "周期     k=1 ";

    for (c = 0; c < 3; c++) {
        memset(&g, 0, sizeof(Grid));
        if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
            return;
        }
        q = analytic_wavenumber(ks[c], pers[c]);
        for (i = 0; i < g.nn; i++) {
            g.u[i] = pers[c] ? cos(q * g.x[i]) : sin(q * g.x[i]);
        }
        if (crank_init(&cs, &g, pers[c] ? BC_PERIODIC : BC_DIRICHLET,
                       0.0, material_alpha(), DEFAULT_DT) != 0) {
            grid_free(&g);
            return;
        }
        ref = g.n / 2;
        u0ref = g.u[ref];
        gfac = crank_growth_factor(cs.r, q, g.dx);
        gcont = exp(-material_alpha() * q * q * DEFAULT_DT);

        /* 和 FTCS 一样防下溢：|G|^m 不能掉到伪模的 1e-16 底下 */
        msteps = 1000.0;
        if (fabs(gfac) < 1.0 && fabs(gfac) > 0.0) {
            mcap = log(1e-6) / log(fabs(gfac));
            if (mcap < 1.0) {
                mcap = 1.0;
            }
            if (msteps > mcap) {
                msteps = floor(mcap);
            }
        }
        for (i = 0; i < (long)msteps; i++) {
            crank_step(&cs);
        }
        measured = g.u[ref] / u0ref;
        theory = pow(gfac, msteps);

        printf("  %s  q = %10.4f  G = %+.14f  连续每步 = %.14f\n",
               labels[c], q, gfac, gcont);
        printf("      走 %.0f 步：实测 G^m = %.14e  理论 = %.14e  相对差 %.3e\n",
               msteps, measured, theory, fabs(measured / theory - 1.0));
        printf("      离散/连续 = %.14f（%+.4e %%）；同 r 下 FTCS 是 %+.14f\n",
               gfac / gcont, 100.0 * (gfac / gcont - 1.0),
               ftcs_growth_factor(cs.r, q, g.dx));
        sprintf(buf, "%s 相对差 %.3e", labels[c], fabs(measured / theory - 1.0));
        check("C1", "CN 单模衰减等于其离散特征值 G^m",
              theory > 0.0 && fabs(measured / theory - 1.0) < 1e-9, buf);
        crank_free(&cs);
        grid_free(&g);
    }
}

static void report_crank_stability(void)
{
    double rs[6];
    double dt;
    long i;
    long nstep;
    double mx;
    double mn;
    double e_cn;
    double e_ft;
    int rc;
    int all_ok;
    char buf[192];

    printf("\n== CN 无条件稳定，但不无条件单调 ==\n");
    printf("  %8s %10s %7s %11s %11s %13s %13s\n",
           "r", "dt/s", "步数", "max u", "min u", "CN 误差", "FTCS 同 r");
    rs[0] = 0.23;
    rs[1] = 0.5;
    rs[2] = 1.0;
    rs[3] = 5.0;
    rs[4] = 50.0;
    rs[5] = 500.0;
    all_ok = 1;
    for (i = 0; i < 6; i++) {
        dt = rs[i] * DEFAULT_DX * DEFAULT_DX / material_alpha();
        nstep = (long)ceil(SNAP_T1 / dt);
        rc = crank_extremes(DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, &mx, &mn);
        if (rc != 0) {
            all_ok = 0;
        }
        e_cn = scheme_error(1, DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, 0);
        printf("  %8.4f %10.5f %7ld %11.6f %11.6f %13.4e",
               rs[i], dt, nstep, mx, mn, e_cn);
        if (rs[i] <= FTCS_R_MAX) {
            e_ft = scheme_error(0, DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, 0);
            printf("   FTCS %.4e", e_ft);
        } else {
            printf("   FTCS 已发散");
        }
        printf("\n");
    }
    printf("  max u 不超过 100、min u 不小于 0 只在 r 小的那几档成立。\n");
    printf("  r 一大，右端矩阵 B 的对角权重 1-r 变负，陡峭前沿下方被打出\n");
    printf("  负温度。CN 无条件稳定，但离散极值原理不再成立——这是它换来\n");
    printf("  的东西，不是免费午餐。FTCS 在 r<=1/2 时反过来两者都保。\n");

    sprintf(buf, "r=500（1 步走完 10 s）max=%.4f min=%.4f，有限", mx, mn);
    check("C2", "CN 在大 r 下仍稳定，不出现 NaN/inf",
          all_ok && fabs(mx) < 1e30 && fabs(mn) < 1e30, buf);
}

static void report_crank_order(void)
{
    double dts[5];
    double big[4];
    double d_ft[5];
    double d_cn[5];
    double e_ft[5];
    double e_cn[5];
    double r_fix;
    double dxs[3];
    double c0;
    double c1;
    double c2;
    double rr;
    long i;
    char buf[192];

    printf("\n== CN 的时间二阶：同网格 dt 与 dt/2 相减（纯模 Dirichlet k=3）==\n");
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    dts[4] = 0.00125;
    printf("  %9s %8s %15s %15s %10s %10s\n",
           "dt/s", "r", "CN 差值", "FTCS 差值", "CN 比", "FTCS 比");
    for (i = 0; i < 5; i++) {
        d_cn[i] = scheme_time_diff(1, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        d_ft[i] = scheme_time_diff(0, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %9.5f %8.4f %15.6e %15.6e", dts[i],
               material_fourier_number(material_alpha(), DEFAULT_DX, dts[i]),
               d_cn[i], d_ft[i]);
        if (i > 0) {
            printf(" %10.4f %10.4f", d_cn[i - 1] / d_cn[i], d_ft[i - 1] / d_ft[i]);
        }
        printf("\n");
    }
    printf("  CN 相邻比趋近 4（二阶），FTCS 趋近 2（一阶）。\n");
    sprintf(buf, "CN 比 %.4f / %.4f / %.4f / %.4f",
            d_cn[0] / d_cn[1], d_cn[1] / d_cn[2],
            d_cn[2] / d_cn[3], d_cn[3] / d_cn[4]);
    check("C3", "CN 时间二阶收敛（dt 减半差值降 4 倍）",
          fabs(d_cn[0] / d_cn[1] - 4.0) < 0.05
              && fabs(d_cn[1] / d_cn[2] - 4.0) < 0.05
              && fabs(d_cn[2] / d_cn[3] - 4.0) < 0.05
              && fabs(d_cn[3] / d_cn[4] - 4.0) < 0.05, buf);

    printf("\n== 绝对误差（纯模 k=3，vs 精确 PDE 解，t = %.0f s）==\n", SNAP_T1);
    printf("  %9s %8s %15s %15s %12s\n", "dt/s", "r", "CN err", "FTCS err", "FTCS/CN");
    for (i = 0; i < 5; i++) {
        rr = material_fourier_number(material_alpha(), DEFAULT_DX, dts[i]);
        e_cn[i] = scheme_error(1, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        e_ft[i] = (rr <= FTCS_R_MAX)
                  ? scheme_error(0, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3)
                  : -1.0;
        printf("  %9.5f %8.4f %15.6e", dts[i], rr, e_cn[i]);
        if (e_ft[i] > 0.0) {
            printf(" %15.6e %11.2fx", e_ft[i], e_ft[i] / e_cn[i]);
        } else {
            printf(" %15s %12s", "(发散)", "");
        }
        printf("\n");
    }
    printf("  CN 这一列对 dt 几乎不动（%.4e ~ %.4e，只动 %.1f%%）：绝对误差已经\n",
           e_cn[4], e_cn[0], 100.0 * (e_cn[0] / e_cn[4] - 1.0));
    printf("  被 dx^2 的空间地板钉住，时间误差小到看不见。FTCS 反过来在 r=0.23\n");
    printf("  附近凹下去——那正是步骤 3 实测的时间项与空间项反号抵消点。所以按\n");
    printf("  绝对误差比，FTCS 在某些 dt 上反而更小；那是巧合，不是格式性质。\n");
    printf("  要比阶，只能比上面那张同网格 dt 与 dt/2 相减的纯时间误差表。\n");
    sprintf(buf, "dt=%.5f 纯时间误差 FTCS/CN = %.0f 倍", dts[1],
            d_ft[1] / d_cn[1]);
    check("C4", "同 dt 下 CN 的纯时间误差远小于 FTCS",
          d_cn[1] > 0.0 && d_ft[1] / d_cn[1] > 1000.0, buf);

    /* CN 能走到 FTCS 根本到不了的大步长，二阶阶数还保不保？ */
    big[0] = 0.2;
    big[1] = 0.1;
    big[2] = 0.05;
    big[3] = 0.025;
    printf("\n== CN 单独走大步（这些 dt 上 FTCS 的 r 早已超过 1/2）==\n");
    printf("  %9s %10s %15s %12s\n", "dt/s", "r", "CN 差值", "相邻比");
    for (i = 0; i < 4; i++) {
        d_cn[i] = scheme_time_diff(1, DEFAULT_DX, big[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %9.4f %10.4f %15.6e", big[i],
               material_fourier_number(material_alpha(), DEFAULT_DX, big[i]),
               d_cn[i]);
        if (i > 0) {
            printf(" %12.4f", d_cn[i - 1] / d_cn[i]);
        }
        printf("\n");
    }
    sprintf(buf, "dt=0.1/0.05/0.025 相邻比 %.4f / %.4f",
            d_cn[1] / d_cn[2], d_cn[2] / d_cn[3]);
    check("C7", "CN 在大步长（r 达 11.5）下仍可测且保持二阶",
          d_cn[0] > 0.0 && fabs(d_cn[1] / d_cn[2] - 4.0) < 0.1
              && fabs(d_cn[2] / d_cn[3] - 4.0) < 0.1, buf);

    /* 空间二阶与周期边界 */
    r_fix = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT);
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    c0 = scheme_error(1, dxs[0], r_fix * dxs[0] * dxs[0] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    c1 = scheme_error(1, dxs[1], r_fix * dxs[1] * dxs[1] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    c2 = scheme_error(1, dxs[2], r_fix * dxs[2] * dxs[2] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    printf("\n  CN 空间二阶（r 固定 = %.4f，纯模 k=3）：\n", r_fix);
    printf("      %.6e / %.6e / %.6e，误差比 %.4f 与 %.4f（应为 4.00）\n",
           c0, c1, c2, c0 / c1, c1 / c2);
    sprintf(buf, "CN dx 减半误差比 %.4f", c1 / c2);
    check("C5", "CN 空间二阶收敛", fabs(c1 / c2 - 4.0) < 0.2, buf);

    c0 = scheme_time_diff(1, DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_PERIODIC, 1);
    c1 = scheme_time_diff(0, DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_PERIODIC, 1);
    printf("  周期纯模 k=1，dt=%.4f：纯时间误差 CN = %.6e，FTCS = %.6e，"
           "FTCS/CN = %.0f 倍\n", DEFAULT_DT, c0, c1, c1 / c0);
    sprintf(buf, "周期纯时间误差 CN %.3e vs FTCS %.3e", c0, c1);
    check("C6", "周期边界下 CN 的纯时间误差同样远优于 FTCS",
          c0 > 0.0 && c1 / c0 > 1000.0, buf);
}

static void report_crank_vs_ftcs_pulse(void)
{
    double snaps[3];
    long i;
    double e_cn;
    double e_ft;
    double mean_now;
    double mean0;
    Grid g;
    Crank cs;
    double worst_drift;
    long step;
    long total;
    char buf[192];

    printf("\n== 物理方波：CN 与 FTCS 在交付时刻上的误差（dx=0.001, dt=0.01）==\n");
    snaps[0] = SNAP_T1;
    snaps[1] = SNAP_T2;
    snaps[2] = SNAP_T3;
    printf("  %8s %16s %16s %12s\n", "t/s", "CN max|err|", "FTCS max|err|", "倍差");
    for (i = 0; i < 3; i++) {
        e_cn = scheme_error(1, DEFAULT_DX, DEFAULT_DT, snaps[i], BC_DIRICHLET, 0);
        e_ft = scheme_error(0, DEFAULT_DX, DEFAULT_DT, snaps[i], BC_DIRICHLET, 0);
        printf("  %8.1f %16.6e %16.6e %11.1fx\n", snaps[i], e_cn, e_ft,
               e_ft / e_cn);
    }
    printf("  方波情形 CN 的优势远没有纯模那么大：总误差里约 85%% 是初值表示\n");
    printf("  误差，两个格式共用同一份 FV 初值，这部分谁也消不掉。\n");

    /* CN 的周期均值守恒：1^T A = 1^T 且 1^T B = 1^T，所以必须严格守恒 */
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 1);
    mean0 = grid_mean_periodic(&g);
    if (crank_init(&cs, &g, BC_PERIODIC, COLD_TEMP,
                   material_alpha(), DEFAULT_DT) != 0) {
        grid_free(&g);
        return;
    }
    worst_drift = 0.0;
    total = (long)(SNAP_T3 / DEFAULT_DT + 0.5);
    for (step = 1; step <= total; step++) {
        crank_step(&cs);
        mean_now = fabs(grid_mean_periodic(&g) - mean0);
        if (mean_now > worst_drift) {
            worst_drift = mean_now;
        }
    }
    printf("  CN 周期 t=30 s（%ld 步）：平均温度最大漂移 %.3e，max/min = %.6f/%.6f\n",
           cs.steps, worst_drift, field_max(&g), field_min(&g));
    printf("  对照 FTCS 同条件：漂移 1.243e-14，max/min = 11.290223/8.710466\n");
    sprintf(buf, "CN 均值漂移 %.3e", worst_drift);
    check("C8", "CN 周期边界平均温度守恒", worst_drift <= 1e-10, buf);
    crank_free(&cs);
    grid_free(&g);
}


/* ============ 步骤 5：收敛判据、调度与数据导出 ============ */

#define CRIT_N 6

/* 副判据的扫描阈值；1e-6 那一档专门用来和主判据同量级地比 */
static const double CRIT_EPS[CRIT_N] = { 1e-4, 1e-6, 1e-8, 1e-9, 1e-11, 1e-13 };

typedef struct {
    double *v;
    long n;
    long cap;
} DArr;

static int darr_init(DArr *a, long cap0)
{
    a->v = (double *)malloc((size_t)cap0 * sizeof(double));
    a->n = 0;
    a->cap = (a->v != NULL) ? cap0 : 0;
    return (a->v != NULL) ? 0 : 1;
}

static int darr_push(DArr *a, double x)
{
    double *nv;

    if (a->n >= a->cap) {
        nv = (double *)realloc(a->v, (size_t)(a->cap * 2 + 1) * sizeof(double));
        if (nv == NULL) {
            return 1;
        }
        a->v = nv;
        a->cap = a->cap * 2 + 1;
    }
    a->v[a->n++] = x;
    return 0;
}

static void darr_free(DArr *a)
{
    free(a->v);
    a->v = NULL;
    a->n = 0;
    a->cap = 0;
}

static double *dup_field(const Grid *g)
{
    double *p;

    p = (double *)malloc((size_t)g->nn * sizeof(double));
    if (p != NULL) {
        memcpy(p, g->u, (size_t)g->nn * sizeof(double));
    }
    return p;
}

typedef struct {
    const char *dxtag;
    const char *bctag;
    double dx;
    int periodic;
    double target;
    double amp;
    double tau;
    double t_pred;
    long steps_conv;
    double t_conv;
    double dev_conv;
    double jump_conv;
    long last_above[CRIT_N];
    long stop_step[CRIT_N];
    double stop_time[CRIT_N];
    double stop_dev[CRIT_N];
    double *u0;
    double *u10;
    double *u20;
    double *u30;
    double *u10dt;
    double *ucv;
    double heat[4];
    double mx[4];
    double mn[4];
    double err[4];
    long nslice;
    int rc;
} Case;

static int run_case(Case *c, const char *outdir)
{
    Grid g;
    Ftcs s;
    Converge cv;
    EvoWriter ew;
    DArr devh;
    double *prev;
    double *uwork;
    double tau;
    double est_steps;
    double ratio;
    double next_slice;
    long i;
    long k;
    long st;
    long s1;
    long s2;
    long s3;
    char name[128];
    char meta[400];
    int periodic;
    BoundaryKind bc;

    periodic = c->periodic;
    bc = periodic ? BC_PERIODIC : BC_DIRICHLET;
    c->rc = 0;
    c->nslice = 0;
    memset(&g, 0, sizeof(Grid));
    memset(&cv, 0, sizeof(Converge));
    memset(&ew, 0, sizeof(EvoWriter));
    if (grid_init(&g, ROD_LENGTH, c->dx) != 0) {
        return 1;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    c->u0 = dup_field(&g);
    c->target = converge_target(bc, &g);
    c->amp = periodic ? fabs(analytic_a_periodic(1))
                      : fabs(analytic_b_dirichlet(1));
    tau = periodic ? material_tau_periodic(material_alpha(), ROD_LENGTH)
                   : material_tau_dirichlet(material_alpha(), ROD_LENGTH);
    c->tau = tau;
    c->t_pred = converge_predict_time(material_alpha(), ROD_LENGTH,
                                      c->amp, periodic, CONV_EPS);
    /*
     * 几何抽稀：ratio = est_steps^(1/MAX_SLICES)。均匀间隔会把 0..30 s
     * 的快速瞬态抹平（Dirichlet 收敛要七万多步，均匀取就只能给到两秒一片），
     * 而题目要画的恰恰是 10/20/30 s。几何级数早期几乎每步一张、后期越来越疏。
     */
    est_steps = ceil(c->t_pred / DEFAULT_DT * 1.2) + 100.0;
    ratio = pow(est_steps, 1.0 / (double)EXPORT_MAX_SLICES);
    if (ratio < 1.02) {
        ratio = 1.02;
    }
    next_slice = 1.0;

    if (darr_init(&devh, 4096) != 0 || c->u0 == NULL) {
        grid_free(&g);
        return 2;
    }
    prev = (double *)malloc((size_t)g.nn * sizeof(double));
    uwork = (double *)malloc((size_t)g.nn * sizeof(double));
    if (prev == NULL || uwork == NULL) {
        darr_free(&devh);
        grid_free(&g);
        return 3;
    }
    for (k = 0; k < CRIT_N; k++) {
        c->last_above[k] = 0;
    }

    sprintf(name, "evolution_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "完整时空演化 u(x_i,t_n)；dx=%.17g dt=%.17g 边界=%s "
                  "时间按公比 %.6f 几何抽稀（上限 %d 片）",
            g.dx, DEFAULT_DT, c->bctag, ratio, EXPORT_MAX_SLICES);
    if (evo_open(&ew, outdir, name, &g, meta) != 0) {
        free(prev);
        darr_free(&devh);
        grid_free(&g);
        return 4;
    }
    evo_write(&ew, 0.0, g.u);
    c->nslice++;

    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), DEFAULT_DT) != 0) {
        evo_close(&ew);
        free(prev);
        darr_free(&devh);
        grid_free(&g);
        return 5;
    }
    converge_init(&cv, c->target, CONV_EPS);

    s1 = (long)(SNAP_T1 / DEFAULT_DT + 0.5);
    s2 = (long)(SNAP_T2 / DEFAULT_DT + 0.5);
    s3 = (long)(SNAP_T3 / DEFAULT_DT + 0.5);

    while (!cv.converged) {
        memcpy(prev, g.u, (size_t)g.nn * sizeof(double));
        ftcs_step(&s);
        converge_observe(&cv, prev, g.u, g.nn);
        if (darr_push(&devh, cv.max_dev) != 0) {
            c->rc = 6;
            break;
        }
        for (k = 0; k < CRIT_N; k++) {
            if (cv.max_jump > CRIT_EPS[k]) {
                c->last_above[k] = cv.steps;
            }
        }
        if (cv.steps == s1) {
            c->u10 = dup_field(&g);
        } else if (cv.steps == s2) {
            c->u20 = dup_field(&g);
        } else if (cv.steps == s3) {
            c->u30 = dup_field(&g);
        }
        /* 相隔正好一个 dt 的一对剖面：给第 6 步把差分方程本身代进数核对 */
        if (cv.steps == s1 + 1) {
            c->u10dt = dup_field(&g);
        }
        if ((double)cv.steps >= next_slice) {
            evo_write(&ew, s.time, g.u);
            c->nslice++;
            next_slice *= ratio;
        }
        if (cv.steps >= CONV_MAX_STEPS) {
            c->rc = 7;
            break;
        }
    }

    c->steps_conv = cv.steps;
    c->t_conv = (double)cv.steps * DEFAULT_DT;
    c->dev_conv = cv.max_dev;
    c->jump_conv = cv.max_jump;
    c->ucv = dup_field(&g);
    evo_write(&ew, s.time, g.u);
    c->nslice++;
    evo_close(&ew);

    /* 副判据的停手时刻：从"最近一次超阈值"往后挪一步 */
    for (k = 0; k < CRIT_N; k++) {
        st = c->last_above[k] + 1;
        c->stop_time[k] = (double)st * DEFAULT_DT;
        if (st > c->steps_conv || st < 1 || st > devh.n) {
            c->stop_step[k] = -1;
            c->stop_dev[k] = 0.0;
        } else {
            c->stop_step[k] = st;
            c->stop_dev[k] = devh.v[st - 1];
        }
    }

    /* 四个交付时刻的统计量 */
    {
        double *src[4];
        double tt[4];
        src[0] = c->u0;  tt[0] = 0.0;
        src[1] = c->u10; tt[1] = SNAP_T1;
        src[2] = c->u20; tt[2] = SNAP_T2;
        src[3] = c->u30; tt[3] = SNAP_T3;
        for (i = 0; i < 4; i++) {
            double heat;
            double mx;
            double mn;
            double er;
            if (src[i] == NULL) {
                c->heat[i] = c->mx[i] = c->mn[i] = c->err[i] = 0.0;
                continue;
            }
            memcpy(uwork, src[i], (size_t)g.nn * sizeof(double));
            memcpy(g.u, src[i], (size_t)g.nn * sizeof(double));
            heat = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
            mx = field_max(&g);
            mn = field_min(&g);
            er = 0.0;
            for (k = 0; k < g.nn; k++) {
                double d = fabs(uwork[k] - analytic_u(g.x[k], tt[i], 0, periodic));
                if (d > er) {
                    er = d;
                }
            }
            c->heat[i] = heat;
            c->mx[i] = mx;
            c->mn[i] = mn;
            c->err[i] = er;
        }
        /* 收敛后剖面：与终态比，同时给出与解析解的差 */
    }

    /* 导出剖面文件 */
    sprintf(name, "profiles_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "题目 (d)(e) 的交付剖面：10/20/30 s 与完全收敛后；"
                  "dx=%.17g dt=%.17g r=%.17g 边界=%s 终态=%.17g eps=%.17g "
                  "收敛于 t=%.17g s（%ld 步）",
            g.dx, DEFAULT_DT, s.r, c->bctag, c->target, CONV_EPS,
            c->t_conv, c->steps_conv);
    if (c->u10 != NULL && c->u20 != NULL && c->u30 != NULL && c->ucv != NULL) {
        if (export_profiles(outdir, name, &g, c->u0, c->u10, c->u20,
                            c->u30, c->ucv, c->target, CONV_EPS,
                            c->t_conv, periodic, meta) != 0) {
            c->rc = 8;
        }
    } else {
        c->rc = 9;
    }
    sprintf(name, "stepdt_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "相隔正好一个 dt 的两条剖面（t=%.17g 与 t+dt）；"
                  "第 6 步把 FTCS 差分方程直接代进这些数核对",
            SNAP_T1);
    if (c->u10 != NULL && c->u10dt != NULL) {
        if (export_stepdt(outdir, name, &g, c->u10, c->u10dt, SNAP_T1,
                          DEFAULT_DT, material_alpha(), periodic, meta) != 0) {
            c->rc = 11;
        }
    } else {
        c->rc = 12;
    }
    sprintf(name, "criterion_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "副判据（逐step变化量）在不同阈值下的停手时刻与那时的残差；"
                  "主判据 eps=%.17g，tau=%.17g s，tau/dt=%.17g",
            CONV_EPS, tau, tau / DEFAULT_DT);
    if (export_criterion(outdir, name, meta, CRIT_EPS, c->stop_step,
                         c->stop_time, c->stop_dev, CRIT_N, CONV_EPS,
                         c->tau, DEFAULT_DT) != 0) {
        c->rc = 10;
    }

    free(prev);
    free(uwork);
    darr_free(&devh);
    grid_free(&g);
    return c->rc;
}

/*
 * 附图用的数据。图里不许现算物理，所以这些剖面也由 C 写出来：
 *   stability.dat    —— 同一时刻 t=10 s，r 从 0.23 到 0.69；r>0.5 的两列
 *                        已经炸到 1e21/1e47，图上单独用早期步数那一组画。
 *   overshoot.dat    —— CN 在 r=0.23 / 50 / 500 的 t=10 s 剖面，看负温度
 *   decomposition.dat —— 总误差拆成格式误差与初值表示误差
 */
static void export_demo_data(const char *outdir)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    static double xs[V_DEMO_NN];
    static double buf[6][V_DEMO_NN];
    const char *heads[6];
    const double *cols[6];
    char meta[300];
    double dt;
    long i;
    long j;
    long nn;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    nn = g.nn;
    if (nn > V_DEMO_NN) {
        grid_free(&g);
        return;
    }
    for (i = 0; i < nn; i++) {
        xs[i] = g.x[i];
    }

    /* 初值 */
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    for (i = 0; i < nn; i++) {
        buf[0][i] = g.u[i];
    }

    /* FTCS：r=0.23 推到 10 s；r=0.69 只走 1/5/20 步（再多就 1e10 量级） */
    dt = 0.23 * g.dx * g.dx / material_alpha();
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    if (ftcs_init(&fs, &g, BC_DIRICHLET, COLD_TEMP, material_alpha(), dt) == 0) {
        ftcs_advance(&fs, SNAP_T1);
        for (i = 0; i < nn; i++) {
            buf[1][i] = g.u[i];
        }
        ftcs_free(&fs);
    }
    dt = 0.69 * g.dx * g.dx / material_alpha();
    {
        long marks[3];
        long prev;
        long want;
        long step;

        marks[0] = 1;
        marks[1] = 5;
        marks[2] = 20;
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        if (ftcs_init(&fs, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt) == 0) {
            prev = 0;
            for (j = 0; j < 3; j++) {
                want = marks[j];
                for (step = prev; step < want; step++) {
                    ftcs_step(&fs);
                }
                prev = want;
                for (i = 0; i < nn; i++) {
                    buf[2 + j][i] = g.u[i];
                }
            }
            ftcs_free(&fs);
        }
    }

    heads[0] = "u_t0";
    heads[1] = "ftcs_r0.23_t10s";
    heads[2] = "ftcs_r0.69_step1";
    heads[3] = "ftcs_r0.69_step5";
    heads[4] = "ftcs_r0.69_step20";
    for (j = 0; j < 5; j++) {
        cols[j] = buf[j];
    }
    sprintf(meta, "FTCS 稳定性对照：r=0.23 推到 10 s 仍是平滑剖面；"
                  "r=0.69（超过 1/2）每步放大 |1-4r sin^2|=%.4f，"
                  "走 1/5/20 步后已成锯齿", 4.0 * 0.69 - 1.0);
    export_columns(outdir, "stability", meta, xs, cols, 5, heads, nn);

    /* CN：r=0.23 / 50 / 500 都推到 10 s */
    {
        double rs[3];
        long nstep[3];

        rs[0] = 0.23;
        rs[1] = 50.0;
        rs[2] = 500.0;
        for (j = 0; j < 3; j++) {
            dt = rs[j] * g.dx * g.dx / material_alpha();
            grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
            if (crank_init(&cs, &g, BC_DIRICHLET, COLD_TEMP,
                           material_alpha(), dt) == 0) {
                crank_advance(&cs, SNAP_T1);
                for (i = 0; i < nn; i++) {
                    buf[1 + j][i] = g.u[i];
                }
                nstep[j] = cs.steps;
                crank_free(&cs);
            } else {
                nstep[j] = -1;
            }
        }
        heads[0] = "u_t0";
        heads[1] = "cn_r0.23_t10s";
        heads[2] = "cn_r50_t10s";
        heads[3] = "cn_r500_t10s";
        for (j = 0; j < 4; j++) {
            cols[j] = buf[j];
        }
        sprintf(meta, "Crank-Nicolson 无条件稳定但不保单调：r=50 用 %ld 步、"
                      "r=500 用 %ld 步走完 10 s，都不发散，"
                      "却在前沿下方打出负温度（B 阵对角权重 1-r 变负）",
                nstep[1], nstep[2]);
        export_columns(outdir, "overshoot", meta, xs, cols, 4, heads, nn);
    }
    grid_free(&g);

    /* 误差分解：一行一个 dx，列是三种误差 */
    if (demo_n > 0) {
        const char *sheads[4];
        double srow[12];
        long j;

        sheads[0] = "dx";
        sheads[1] = "format_err_max";
        sheads[2] = "representation_err_max";
        sheads[3] = "total_err_max";
        for (j = 0; j < demo_n; j++) {
            srow[j * 4 + 0] = demo_dx[j];
            srow[j * 4 + 1] = demo_fmt[j];
            srow[j * 4 + 2] = demo_rep[j];
            srow[j * 4 + 3] = demo_tot[j];
        }
        sprintf(meta, "t=10 s 总误差的分解（Dirichlet，dt=%.4f）：dx=0.002 时"
                      "高温段边界正好落在单元边界上，表示误差恒为 0；"
                      "dx=0.001 时边界落在节点中心，表示误差约占 85%%",
                DEFAULT_DT);
        export_summary(outdir, "decomposition", srow, 4, demo_n, sheads, meta);
    }
}

static void report_step5(const char *outdir)
{
    Case cs[4];
    Case *c;
    double tau_dt;
    double pred_dev;
    double ratio;
    double worst_pred;
    double row[16];
    double *rows;
    long i;
    long k;
    int all_ok;
    int ok_n2;
    int ok_n3;
    int ok_n4;
    int ok_n6;
    char buf[256];
    const char *heads[16];

    printf("\n== 收敛判据与四种组合的完整调度 ==\n");
    printf("  主判据 max|u-u_inf| <= eps = %.1e 摄氏度；dt = %.4f s\n",
           CONV_EPS, DEFAULT_DT);

    memset(cs, 0, sizeof(cs));
    cs[0].dxtag = "fine";   cs[0].bctag = "dirichlet"; cs[0].dx = DEFAULT_DX; cs[0].periodic = 0;
    cs[1].dxtag = "fine";   cs[1].bctag = "periodic";  cs[1].dx = DEFAULT_DX; cs[1].periodic = 1;
    cs[2].dxtag = "coarse"; cs[2].bctag = "dirichlet"; cs[2].dx = COARSE_DX;  cs[2].periodic = 0;
    cs[3].dxtag = "coarse"; cs[3].bctag = "periodic";  cs[3].dx = COARSE_DX;  cs[3].periodic = 1;

    all_ok = 1;
    for (i = 0; i < 4; i++) {
        if (run_case(&cs[i], outdir) != 0) {
            all_ok = 0;
            printf("  组合 %s/%s 失败 rc=%d\n", cs[i].dxtag, cs[i].bctag,
                   cs[i].rc);
        }
    }

    printf("\n  %-6s %-10s %10s %10s %10s %12s %12s %10s %12s %8s\n",
           "dx", "边界", "终态", "最低模幅", "tau/s", "预测 t/s", "实测 t/s",
           "步数", "停手残差", "片数");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        printf("  %-6s %-10s %10.6f %10.6f %10.4f %12.2f %12.2f %10ld %12.3e %8ld\n",
               c->dxtag, c->bctag, c->target, c->amp, c->tau,
               c->t_pred, c->t_conv, c->steps_conv, c->dev_conv, c->nslice);
    }

    printf("\n  副判据（逐step变化量）在不同阈值下什么时候停手：\n");
    printf("     预测残差 = eps_step * tau/dt，因为单步变化量约等于 残差 x dt/tau\n");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        tau_dt = c->tau / DEFAULT_DT;
        printf("  -- %s/%s：主判据在 t=%.2f s 停手；tau/dt = %.0f\n",
               c->dxtag, c->bctag, c->t_conv, tau_dt);
        printf("     %10s %10s %10s %13s %13s %9s\n",
               "eps_step", "停手步数", "停手 t/s", "实测残差", "预测残差", "残差/eps");
        for (k = 0; k < CRIT_N; k++) {
            if (c->stop_step[k] > 0) {
                pred_dev = CRIT_EPS[k] * tau_dt;
                printf("     %10.0e %10ld %10.2f %13.4e %13.4e %9.2f\n",
                       CRIT_EPS[k], c->stop_step[k], c->stop_time[k],
                       c->stop_dev[k], pred_dev, c->stop_dev[k] / CONV_EPS);
            } else {
                printf("     %10.0e %10s\n", CRIT_EPS[k], "(未达到)");
            }
        }
    }

    /* ---- 验收 ---- */
    sprintf(buf, "四种组合全部收敛，rc 全 0");
    check("N1", "四种组合都跑到主判据达标", all_ok, buf);

    ok_n2 = 1;
    for (i = 0; i < 4; i++) {
        if (!(cs[i].dev_conv <= CONV_EPS && cs[i].dev_conv > CONV_EPS * 0.2)) {
            ok_n2 = 0;
        }
    }
    sprintf(buf, "残差介于 [0.2 eps, eps]，最大 %.3e", cs[0].dev_conv);
    check("N2", "停手时刻不多跑：残差落在 eps 的一个数量级内", ok_n2, buf);

    worst_pred = 0.0;
    for (i = 0; i < 4; i++) {
        ratio = fabs(cs[i].t_conv / cs[i].t_pred - 1.0);
        if (ratio > worst_pred) {
            worst_pred = ratio;
        }
    }
    sprintf(buf, "实测/预测 最大相对差 %.4f%%", 100.0 * worst_pred);
    check("N3", "收敛时刻与单指数预测 tau*ln(A/eps) 一致",
          worst_pred < 0.05, buf);

    /* 副判据的松紧：预测残差 = eps_step * tau/dt */
    ok_n4 = 1;
    printf("\n  副判据松多少（预测残差 = eps_step * tau/dt）：\n");
    printf("  %-6s %-10s %12s %14s %14s %10s\n",
           "dx", "边界", "eps_step", "实测残差", "预测残差", "比值");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        k = 1; /* eps_step = 1e-6 那一档 */
        if (c->stop_step[k] <= 0) {
            ok_n4 = 0;
            continue;
        }
        pred_dev = CRIT_EPS[k] * c->tau / DEFAULT_DT;
        ratio = c->stop_dev[k] / pred_dev;
        printf("  %-6s %-10s %12.1e %14.4e %14.4e %10.3f\n",
               c->dxtag, c->bctag, 1e-6, c->stop_dev[k], pred_dev, ratio);
        if (ratio < 0.8 || ratio > 1.25) {
            ok_n4 = 0;
        }
        if (c->stop_time[k] >= c->t_conv) {
            ok_n4 = 0;
        }
    }
    sprintf(buf, "1e-6 档副判据提前停手，实测/预测残差比见上表");
    check("N4", "副判据在残差约 eps_step*tau/dt 处停手，且早于主判据", ok_n4, buf);

    ok_n6 = 1;
    for (i = 0; i < 4; i++) {
        if (cs[i].nslice > EXPORT_MAX_SLICES + 2) {
            ok_n6 = 0;
        }
    }
    sprintf(buf, "切片数 %ld/%ld/%ld/%ld（上限 %d）",
            cs[0].nslice, cs[1].nslice, cs[2].nslice, cs[3].nslice,
            EXPORT_MAX_SLICES);
    check("N6", "时空演化文件的时间抽稀不超上限", ok_n6, buf);

    /* 终态本身 */
    ok_n3 = 1;
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        if (fabs(c->target - (c->periodic ? 10.0 : 0.0)) > 1e-9) {
            ok_n3 = 0;
        }
    }
    sprintf(buf, "Dirichlet 终态 %.1f，周期终态 %.1f", cs[0].target, cs[1].target);
    check("N5", "两种边界的终态分别是 0 度与 10 度", ok_n3, buf);

    /* 汇总文件 */
    heads[0] = "dx"; heads[1] = "periodic"; heads[2] = "dt"; heads[3] = "target";
    heads[4] = "amp_k1"; heads[5] = "tau"; heads[6] = "t_pred"; heads[7] = "t_conv";
    heads[8] = "steps_conv"; heads[9] = "dev_conv"; heads[10] = "jump_conv";
    heads[11] = "err_t10"; heads[12] = "err_t20"; heads[13] = "err_t30";
    heads[14] = "heat_t30"; heads[15] = "nslice";
    rows = (double *)malloc((size_t)(4 * 16) * sizeof(double));
    if (rows != NULL) {
        for (i = 0; i < 4; i++) {
            c = &cs[i];
            row[0] = c->dx; row[1] = (double)c->periodic; row[2] = DEFAULT_DT;
            row[3] = c->target; row[4] = c->amp; row[5] = c->tau;
            row[6] = c->t_pred; row[7] = c->t_conv; row[8] = (double)c->steps_conv;
            row[9] = c->dev_conv; row[10] = c->jump_conv;
            row[11] = c->err[1]; row[12] = c->err[2]; row[13] = c->err[3];
            row[14] = c->heat[3]; row[15] = (double)c->nslice;
            for (k = 0; k < 16; k++) {
                rows[i * 16 + k] = row[k];
            }
        }
        export_summary(outdir, "summary", rows, 16, 4, heads,
                       "四种组合的收敛与误差汇总；报告表格引这份数");
        free(rows);
    }

    for (i = 0; i < 4; i++) {
        free(cs[i].u0);
        free(cs[i].u10);
        free(cs[i].u20);
        free(cs[i].u30);
        free(cs[i].u10dt);
        free(cs[i].ucv);
    }
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [--verify-only] [outdir]\n"
            "  默认：跑完全部计算并导出到 outdir（默认 data），最后做代入式核对\n"
            "  --verify-only：不重算，只核对 outdir 里已有的数据文件\n",
            prog);
}

int main(int argc, char **argv)
{
    const char *outdir;
    int verify_only = 0;
    int i;

    /*
     * 参数：目录名随意放，--verify-only 表示"不重算，只把 data 里现有的
     * 数字读回来核对"。没有这个入口，代入式核对就永远只在验自己刚写出的
     * 正确数据——篡改过的交付文件会被重算覆盖掉，检查形同虚设。
     */
    outdir = NULL;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify-only") == 0) {
            verify_only = 1;
        } else if (strcmp(argv[i], "-h") == 0
                   || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            outdir = argv[i];
        }
    }
    if (outdir == NULL) {
        outdir = "data";
    }

    if (verify_only) {
        printf("热传导作业 / 只核对不重算：目录 %s\n", outdir);
        printf("（跳过全部计算与导出，直接把 data 里现有的数字代进定义式）\n");
        if (verify_run(outdir) != 0) {
            printf("VERIFY-ONLY: FAIL\n");
            return 1;
        }
        printf("VERIFY-ONLY: PASS\n");
        return 0;
    }

    printf("热传导作业 / 步骤 1-5：参数、初值、FTCS、解析解、CN 对照、收敛与导出\n");
    printf("输出目录: %s\n\n", outdir);

    report_material();
    if (report_grid(DEFAULT_DX, "(a)(d) 细网格", "initial_fine", outdir) != 0) {
        return 1;
    }
    if (report_grid(COARSE_DX, "(e) 粗网格", "initial_coarse", outdir) != 0) {
        return 1;
    }

    report_evolution(DEFAULT_DX, "细网格", BC_DIRICHLET, "两端 0 度");
    report_evolution(DEFAULT_DX, "细网格", BC_PERIODIC, "周期边界");
    report_evolution(COARSE_DX, "粗网格", BC_DIRICHLET, "两端 0 度");
    report_evolution(COARSE_DX, "粗网格", BC_PERIODIC, "周期边界");

    report_stability(DEFAULT_DX, "细网格");

    printf("\n== 离散模态放大因子（细网格，走 1000 步 = 10 s）==\n");
    report_mode(DEFAULT_DX, BC_DIRICHLET, "Dirichlet", 1, 1000);
    report_mode(DEFAULT_DX, BC_DIRICHLET, "Dirichlet", 99, 1000);
    report_mode(DEFAULT_DX, BC_PERIODIC, "周期", 1, 1000);

    report_analytic_coeffs();
    report_analytic_series();
    report_analytic_vs_ftcs();
    report_error_decomposition();
    report_convergence_order();

    report_crank_mode();
    report_crank_stability();
    report_crank_order();
    report_crank_vs_ftcs_pulse();

    report_step5(outdir);
    export_demo_data(outdir);

    checks_failed += verify_run(outdir);

    printf("\n== 验收：程序内 %ld 项 + 代入式核对 %ld 项 = %ld 项，%ld 项失败 ==\n",
           checks_run, verify_checks_run(),
           checks_run + verify_checks_run(), checks_failed);
    if (checks_failed != 0) {
        return 1;
    }
    printf("STEP1-7: PASS\n");
    return 0;
}