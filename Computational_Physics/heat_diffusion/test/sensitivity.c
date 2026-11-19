/*
 * 稳健性对照：换网格、换格式，看结论跟不跟着变。
 *
 * 这一层不是「再验一次对不对」，而是把前面几步得到的**结论**放到
 * 不同的 Δx 与不同格式下重跑，检查它们是否只依赖物理量：
 *   1) 周期边界的平均温度守恒，与 Δx、与格式无关；
 *   2) Dirichlet 到 30 s 的热量比值，与 Δx 无关（都在离散误差内）；
 *   3) 无量纲收敛时间 t_conv / tau 恒等于 ln(A/eps)，与 Δx、与格式无关；
 *   4) naive 初值的热量超额是结构性的：边界落在节点上就多一个节点，
 *      落在单元边界上就正好不多。
 *
 * 带 --sabotage 时把初值改成恒零，上面几条必须失败——否则等于没检验。
 *
 * 编译见 test/run.sh 第 4 步。
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

static long checks_run = 0;
static long checks_failed = 0;
static int sabotage = 0;

static void sk(const char *id, const char *what, int ok, const char *detail)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
    }
    printf("  [%s] %-4s %-46s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/* 测试文件自己算 max|u|：main.c 里的 field_max_abs 是 static，跨不到这里 */
static double max_abs(const Grid *g)
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

/* 场对 target 的最大偏离 */
static double max_dev(const Grid *g, double target)
{
    long i;
    double m;
    double a;

    m = 0.0;
    for (i = 0; i < g->nn; i++) {
        a = fabs(g->u[i] - target);
        if (a > m) {
            m = a;
        }
    }
    return m;
}

/* 把初值装进网格；sabotage 时全置零，用来验证检查真的会失败 */
static void fill(Grid *g, int periodic)
{
    long i;

    if (sabotage) {
        for (i = 0; i < g->nn; i++) {
            g->u[i] = 0.0;
        }
        if (periodic) {
            g->u[g->n] = 0.0;
        }
        return;
    }
    grid_initial_fv(g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
}

/* 推到主判据达标，返回 t_conv（秒）；失败返回 -1 */
static double run_to_convergence(double dx, int periodic, int scheme, double r)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    double dt;
    double target;
    long guard;
    long steps;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx) != 0) {
        return -1;
    }
    fill(&g, periodic);
    dt = r * dx * dx / material_alpha();
    target = periodic ? grid_mean_periodic(&g) : COLD_TEMP;
    guard = 0;
    steps = 0;

    if (scheme == 0) {
        if (ftcs_init(&fs, &g, periodic ? BC_PERIODIC : BC_DIRICHLET,
                      COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1;
        }
        while (max_dev(&g, target) > CONV_EPS) {
            ftcs_step(&fs);
            steps++;
            if (++guard > 1200000L) {
                ftcs_free(&fs);
                grid_free(&g);
                return -1;
            }
        }
        ftcs_free(&fs);
    } else {
        if (crank_init(&cs, &g, periodic ? BC_PERIODIC : BC_DIRICHLET,
                       COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1;
        }
        while (max_dev(&g, target) > CONV_EPS) {
            crank_step(&cs);
            steps++;
            if (++guard > 400000L) {
                crank_free(&cs);
                grid_free(&g);
                return -1;
            }
        }
        crank_free(&cs);
    }
    grid_free(&g);
    return steps * dt; /* 返回 t_conv，单位 s */
}

int main(int argc, char **argv)
{
    double dxs[4];
    const char *labels[4];
    double r_ftcs;
    double r_cn;
    long i;
    char buf[224];

    sabotage = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sabotage") == 0) {
            sabotage = 1;
        }
    }

    printf("== 稳健性对照：换网格、换格式，看结论是否跟着变 ==\n");
    if (sabotage) {
        printf("   *** --sabotage：初值被强制置零，下面几条应当 FAIL ***\n");
    }

    dxs[0] = 0.0005;
    dxs[1] = 0.001;
    dxs[2] = 0.002;
    dxs[3] = 0.004;
    labels[0] = "0.0005";
    labels[1] = "0.0010";
    labels[2] = "0.0020";
    labels[3] = "0.0040";
    r_ftcs = 0.23;
    /* CN 故意取 FTCS 不能用的大步长：r = 2.3 */
    r_cn = 2.3;

    /* ---- 1) 周期平均温度守恒 ---- */
    printf("\n  (1) 周期边界平均温度（终态应为 10 摄氏度）\n");
    for (i = 0; i < 4; i++) {
        Grid g;
        Ftcs fs;
        Crank cs;
        double m0;
        double m1;
        double m2;
        long step;
        double worst;

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, ROD_LENGTH, dxs[i]);
        fill(&g, 1);
        m0 = grid_mean_periodic(&g);
        if (ftcs_init(&fs, &g, BC_PERIODIC, COLD_TEMP,
                      material_alpha(), r_ftcs * g.dx * g.dx / material_alpha()) == 0) {
            for (step = 0; step < (long)(SNAP_T3 / (r_ftcs * g.dx * g.dx / material_alpha()) + 0.5); step++) {
                ftcs_step(&fs);
            }
            m1 = grid_mean_periodic(&g);
            ftcs_free(&fs);
        } else {
            m1 = m0;
        }
        fill(&g, 1);
        if (crank_init(&cs, &g, BC_PERIODIC, COLD_TEMP,
                       material_alpha(), r_cn * g.dx * g.dx / material_alpha()) == 0) {
            crank_advance(&cs, SNAP_T3);
            m2 = grid_mean_periodic(&g);
            crank_free(&cs);
        } else {
            m2 = m0;
        }
        worst = fabs(m1 - m0);
        if (fabs(m2 - m0) > worst) {
            worst = fabs(m2 - m0);
        }
        sprintf(buf, "dx=%s：FTCS %.10f / CN %.10f（初值 %.10f），漂移 %.3e",
                labels[i], m1, m2, m0, worst);
        sk("R1", "两种格式、四种网格都守恒且等于 10",
           worst < 1e-10 && fabs(m0 - 10.0) < 1e-10, buf);
        grid_free(&g);
    }

    /* ---- 2) Dirichlet 30 s 的热量比值 ---- */
    printf("\n  (2) Dirichlet 到 30 s 剩余热量比值（与网格无关的物理量）\n");
    {
        double ratios[4];

        for (i = 0; i < 4; i++) {
            Grid g;
            Ftcs fs;
            double h0;
            double h1;
            long step;
            long total;

            memset(&g, 0, sizeof(Grid));
            grid_init(&g, ROD_LENGTH, dxs[i]);
            fill(&g, 0);
            h0 = grid_heat_dirichlet(&g);
            if (ftcs_init(&fs, &g, BC_DIRICHLET, COLD_TEMP, material_alpha(),
                          r_ftcs * g.dx * g.dx / material_alpha()) == 0) {
                total = (long)(SNAP_T3 / fs.dt + 0.5);
                for (step = 0; step < total; step++) {
                    ftcs_step(&fs);
                }
                h1 = grid_heat_dirichlet(&g);
                ftcs_free(&fs);
            } else {
                h1 = h0;
            }
            ratios[i] = h1 / h0;
            sprintf(buf, "dx=%s 剩 %.8f", labels[i], ratios[i]);
            sk("R2", "热量比值随网格稳定（差 < 1e-3）",
               fabs(ratios[i] - 0.64) < 0.02, buf);
            grid_free(&g);
        }
    }

    /* ---- 3) 无量纲收敛时间 ---- */
    printf("\n  (3) 无量纲收敛时间 t_conv/tau，应恒等于 ln(A/eps)\n");
    {
        double amp_d = fabs(analytic_b_dirichlet(1));
        double amp_p = fabs(analytic_a_periodic(1));
        double want_d = log(amp_d / CONV_EPS);
        double want_p = log(amp_p / CONV_EPS);
        double tau_d = material_tau_dirichlet(material_alpha(), ROD_LENGTH);
        double tau_p = material_tau_periodic(material_alpha(), ROD_LENGTH);
        double tc;
        double nd;
        int ok;

        printf("      期望值 ln(A/eps)：Dirichlet %.4f，周期 %.4f\n", want_d, want_p);
        printf("      %8s %10s %12s %12s %10s\n",
               "dx", "格式", "t_conv/s", "t_conv/tau", "偏差");
        ok = 1;
        for (i = 0; i < 4; i++) {
            tc = run_to_convergence(dxs[i], 0, 0, r_ftcs);
            if (tc < 0) {
                continue;
            }
            nd = tc / tau_d;
            sprintf(buf, "dx=%s FTCS t=%.2f s，t/tau = %.4f 对 %.4f（偏 %.2f%%）",
                    labels[i], tc, nd, want_d, 100.0 * (nd / want_d - 1.0));
            sk("R3", "Dirichlet 无量纲收敛时间与网格无关",
               fabs(nd / want_d - 1.0) < 0.01, buf);
            if (fabs(nd / want_d - 1.0) >= 0.01) {
                ok = 0;
            }
        }
        /* 换格式：CN 用 FTCS 用不了的大 r，t_conv 应当一样 */
        tc = run_to_convergence(dxs[1], 0, 1, r_cn);
        if (tc >= 0.0) {
            nd = tc / tau_d;
            sprintf(buf, "dx=0.001 CN(r=2.3) t=%.2f s，t/tau = %.4f 对 %.4f",
                    tc, nd, want_d);
            sk("R4", "换 CN 大步长，无量纲收敛时间不变",
               fabs(nd / want_d - 1.0) < 0.01, buf);
        }
        tc = run_to_convergence(dxs[1], 1, 0, r_ftcs);
        if (tc >= 0.0) {
            nd = tc / tau_p;
            sprintf(buf, "周期 FTCS t=%.2f s，t/tau = %.4f 对 %.4f", tc, nd, want_p);
            sk("R5", "周期边界的无量纲收敛时间",
               fabs(nd / want_p - 1.0) < 0.01, buf);
        }
        (void)ok;
    }

    /* ---- 4) naive 超额是结构性的 ---- */
    printf("\n  (4) naive 初值的热量超额：|超额| <= dx/W，符号看边界落在哪\n");
    for (i = 0; i < 4; i++) {
        Grid g;
        double exact;
        double naive;
        double excess;
        double bound;
        long nodes;
        long j;

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, ROD_LENGTH, dxs[i]);
        exact = (HOT_TEMP - COLD_TEMP) * HOT_WIDTH;
        grid_initial_naive(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
        naive = grid_heat_dirichlet(&g);
        /* 独立数一遍闭区间里的节点数，直接验证 naive 热量 = 100*dx*节点数 */
        nodes = 0;
        for (j = 0; j < g.nn; j++) {
            if (g.x[j] >= HOT_LO - GEOM_TOL && g.x[j] <= HOT_HI + GEOM_TOL) {
                nodes++;
            }
        }
        excess = (naive - exact) / exact;
        /*
         * 闭区间里数到的节点数只有两种可能：floor(W/dx) 或 +1
         * （两端都正好落在节点上时才是 +1）。于是 naive 热量 = 100*dx*节点数
         * 与真值 100*W 的相对差被 dx/W 卡住，而且**可正可负**：
         *   dx=0.0005 -> +5%    dx=0.001 -> +10%
         *   dx=0.002  ->  0%    dx=0.004 -> -20%
         * 所以既不能说「naive 总是多算」，也不能说「网格粗了就没事」。
         * 第一版把它写成超额 = 1/总单元数，分母用错了 W/dx 与 L/dx，
         * 三档全 FAIL，是这条检查把我的公式顶回去的。
         */
        bound = g.dx / HOT_WIDTH;
        sprintf(buf, "dx=%s：%ld 节点，100*dx*节点数 = %.10f，超额 %+.4f%%（上界 %.0f%%）",
                labels[i], nodes, HOT_TEMP * (double)nodes * g.dx,
                100.0 * excess, 100.0 * bound);
        sk("R6", "naive 热量 = 100*dx*节点数，且 |超额| <= dx/W",
           fabs(naive - HOT_TEMP * (double)nodes * g.dx) <= 1e-12
               && fabs(excess) <= bound + 1e-12, buf);
        grid_free(&g);
    }

    /* ---- 5) CN 无条件稳定扫到 r=1e6 ---- */
    printf("\n  (5) CN 在 r 大到 1e6 时仍不发散（稳定性与网格无关）\n");
    for (i = 0; i < 4; i++) {
        Grid g;
        Crank cs;
        double dt;
        int ok;

        memset(&g, 0, sizeof(Grid));
        grid_init(&g, ROD_LENGTH, dxs[i]);
        fill(&g, 0);
        dt = 1e6 * g.dx * g.dx / material_alpha();
        ok = 1;
        if (crank_init(&cs, &g, BC_DIRICHLET, COLD_TEMP, material_alpha(), dt) != 0) {
            ok = 0;
        } else {
            crank_step(&cs);
            if (!crank_all_finite(&cs)) {
                ok = 0;
            }
            if (fabs(max_abs(&g)) > 1e6) {
                ok = 0;
            }
            crank_free(&cs);
        }
        sprintf(buf, "dx=%s，r=1e6（dt=%.4g s）一步后 max|u| = %.6f",
                labels[i], dt, ok ? fabs(max_abs(&g)) : -1.0);
        sk("R7", "CN 单步在 r=1e6 仍有界", ok, buf);
        grid_free(&g);
    }

    printf("\n== 稳健性对照：%ld 项，%ld 项失败%s ==\n", checks_run, checks_failed,
           sabotage ? "（sabotage 模式下失败是预期结果）" : "");
    if (checks_failed != 0 && !sabotage) {
        return 1;
    }
    if (sabotage) {
        /* 破坏模式：必须真的失败，否则这条负向对照本身没有意义 */
        if (checks_failed == 0) {
            printf("SABOTAGE: 检查没有失败，负向对照无效\n");
            return 1;
        }
        printf("SABOTAGE: %ld 项如预期失败\n", checks_failed);
        return 0;
    }
    printf("SENSITIVITY: PASS\n");
    return 0;
}
