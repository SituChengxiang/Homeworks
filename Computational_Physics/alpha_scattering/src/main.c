#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "units.h"
#include "cases.h"
#include "trajectory.h"
#include "rutherford.h"
#include "export.h"

/* ---------------------------------------------------------------------------
 * 模块八：主程序
 *
 * 只做调度：取六组初值 -> 积分 -> 写文件 -> 打表。
 * 物理、数值、导出的实现都不写在这里。
 *
 * 用法： alpha_scattering [输出目录] [-t 时长] [-dt 步长] [--manual]
 *   缺省输出到当前工作目录下的 data/，t_max = 14.0，dt = 0.005，用 RK4。
 *
 *   t_max = 14：六条轨迹最迟在 t = 13.5 穿出 +-22.5 的画框，
 *               取 14 保证没有一条是被时间上限截断的。
 *   dt = 0.005：RK4 的能量漂移降到 1e-15（双精度舍入水平），
 *               最近距离与解析值吻合到 8 位有效数字；
 *               每 2 步存一个点，输出采样间隔 0.01，一条约 1300 行。
 * ------------------------------------------------------------------------- */

#define TRAJ_CAPACITY 50000
#define DT_DEFAULT        0.005
#define RECORD_DEFAULT        2

/* 目录不存在或不可写时（IDE 把工作目录设到别处、或 data/ 还没建），
 * 退回当前目录继续跑，不至于直接失败。
 * C89 没有可移植的 mkdir，所以只能退让，不能自己建目录。
 */
static int dir_is_writable(const char *dir)
{
	char  probe[600];
	FILE *fp;

	sprintf(probe, "%s/.write_test", dir);
	fp = fopen(probe, "w");
	if (fp == NULL) return 0;
	fclose(fp);
	remove(probe);
	return 1;
}

int main(int argc, char **argv)
{
	const Case  *cases = cases_get();
	RunControl   ctl;
	TrajReport   reps[N_CASES];
	RutherfordRef ref[N_CASES];
	char         outdir[512];
	char         path[600];
	int          i;

	strcpy(outdir, "data");

	ctl.integrator   = INTEGRATOR_RK4;
	ctl.dt           = DT_DEFAULT;
	ctl.t_max        = 14.0;
	ctl.stop_box     = PLOT_LIMIT + RUNOUT_MARGIN;   /* 22.5 + 2.5 = 25.0 */
	ctl.record_every = RECORD_DEFAULT;

	/* ---- 命令行 ---- */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			ctl.t_max = atof(argv[++i]);
		} else if (strcmp(argv[i], "-dt") == 0 && i + 1 < argc) {
			ctl.dt = atof(argv[++i]);
		} else if (strcmp(argv[i], "--manual") == 0) {
			ctl.integrator = INTEGRATOR_MANUAL;
		} else if (strcmp(argv[i], "--utf8") == 0) {
			console_set_ascii(0);
		} else if (strcmp(argv[i], "--ascii") == 0) {
			console_set_ascii(1);
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("usage: %s [outdir] [-t T] [-dt H] [--manual] [--ascii|--utf8]\n",
			       argv[0]);
			return 0;
		} else if (strlen(argv[i]) < 400) {
			strcpy(outdir, argv[i]);
		}
	}

	if (!dir_is_writable(outdir)) {
		printf("%s\n", T("提示: 目录不存在或不可写，数据改写到当前目录",
		                 "note: output dir missing or unwritable, writing to cwd"));
		strcpy(outdir, ".");
	}
	if (ctl.integrator == INTEGRATOR_MANUAL && ctl.dt > 0.02) {
		printf("%s\n\n", T("提示: 平均速度格式只有一阶精度，建议 --manual 配 -dt 0.02",
		                   "note: the average-velocity scheme is first order; "
		                   "pair --manual with -dt 0.02"));
	}


	printf("%s\n", T("约化单位: 电荷 e / 质量 相对分子质量 / 长度 A / k = 1",
	                "reduced units: charge e / relative molecular mass / angstrom / k = 1"));
	printf(T("核 %.0f e 固定于 (%g, %g)   a 粒子 %.0f e 质量 %.0f\n",
	         "nucleus %.0f e fixed at (%g, %g);  alpha %.0f e, mass %.0f\n"),
	       Z_NUCLEUS, X_NUCLEUS, Y_NUCLEUS, Z_ALPHA, MASS_ALPHA);
	printf(T("k q1 q2 = %g   加速度前系数 k q1 q2 / m = %g\n",
	         "k q1 q2 = %g   acceleration factor k q1 q2 / m = %g\n"),
	       COUPLING, ACCEL_FACTOR);
	printf(T("积分器 = %s   dt = %.6f   t_max = %.3f   停算框 = +-%.1f\n\n",
	         "integrator = %s   dt = %.6f   t_max = %.3f   stop box = +-%.1f\n\n"),
	       ctl.integrator == INTEGRATOR_RK4 ? "RK4" : "manual",
	       ctl.dt, ctl.t_max, ctl.stop_box);

	printf("%-4s %-18s %-6s %8s %12s %12s %11s %10s %8s\n",
	       "id", "label", "n_pts", "t_end", "x_end", "y_end",
	       "max|dE|/E", "r_min", "theta");

	for (i = 0; i < N_CASES; i++) {
		Trajectory  traj;
		TrajReport *rep = &reps[i];
		int         rows;

		if (!traj_alloc(&traj, TRAJ_CAPACITY)) {
			fprintf(stderr, "错误: 轨迹缓冲区分配失败\n");
			return 1;
		}

		alpha_run(cases[i].x0, cases[i].y0, cases[i].vx0, cases[i].vy0,
			  &ctl, &traj, rep);

		/* 解析参考量：同一组 (E, L) 代进卢瑟福公式，作为数值解的标准答案 */
		ref[i].b_asym      = rutherford_impact_parameter(rep->e0, rep->l0);
		ref[i].ecc         = rutherford_eccentricity(rep->e0, rep->l0);
		ref[i].r_min_exact = rutherford_closest_approach(rep->e0, rep->l0);
		ref[i].theta_exact = rutherford_deflection_deg(rep->e0, rep->l0);

		if (rep->overflow) {
			fprintf(stderr, "警告: case %d 采样点超出容量，轨迹被截断\n",
			        cases[i].id);
		}

		sprintf(path, "%s/traj_%d.dat", outdir, cases[i].id);
		rows = traj_export(&traj, &cases[i], rep, &ctl, path);
		if (rows < 0) {
			fprintf(stderr, "错误: 无法写出 %s\n", path);
			fprintf(stderr, "提示: 请先 mkdir -p %s\n", outdir);
			traj_free(&traj);
			return 1;
		}

		traj_summary_print(&cases[i], rep, traj.n);
		traj_free(&traj);
	}

	/* ---- 数值解 vs 解析解：终端一份，文件一份 ---- */
	summary_print(cases, reps, ref, N_CASES);

	sprintf(path, "%s/summary.md", outdir);
	if (summary_export(cases, reps, ref, N_CASES, path, 1, &ctl) != 0) {
		fprintf(stderr, "错误: 无法写出 %s\n", path);
	}
	sprintf(path, "%s/summary.txt", outdir);
	if (summary_export(cases, reps, ref, N_CASES, path, 0, &ctl) != 0) {
		fprintf(stderr, "错误: 无法写出 %s\n", path);
	}

	printf(T("数据已写入 %s/ ：traj_1.dat ... traj_%d.dat, summary.md, summary.txt\n",
	         "wrote %s/ : traj_1.dat ... traj_%d.dat, summary.md, summary.txt\n"),
	       outdir, N_CASES);
	return 0;
}
