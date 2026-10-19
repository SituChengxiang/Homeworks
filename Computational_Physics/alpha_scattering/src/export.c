#include <stdio.h>
#include <string.h>
#include <math.h>
#include "export.h"
#include "units.h"
#include "rutherford.h"

static int g_ascii =
#ifdef _WIN32
    1;      /* Dev-C++ / MinGW 控制台默认 GBK，缺省先说英文 */
#else
    0;
#endif

void console_set_ascii(int use_ascii)
{
    g_ascii = use_ascii;
}

const char *T(const char *zh, const char *en)
{
    return g_ascii ? en : zh;
}

int traj_export(const Trajectory *traj, const Case *cs,
		const TrajReport *rep, const RunControl *ctl,
		const char *path)
{
	FILE *fp;
	int    i;
	const char *kind;
	const char *stop_why;
	char tmax_text[16];

	fp = fopen(path, "w");
	if (fp == NULL) return -1;

	kind = (ctl->integrator == INTEGRATOR_RK4) ? "RK4" : "manual-avg-velocity";

	switch (rep->stopped_by) {
		case 1:  stop_why = "left the plot box"; break;
		case 2:  stop_why = "sample buffer full"; break;
		case 3:  stop_why = "singularity guard";  break;
		default: stop_why = "time limit reached"; break;
	}
	/* C89 没有 snprintf，这里用 sprintf 写进固定 16 字节的缓冲：
	 * %.3f 配 double 时长最多十几位，加上结束符足够宽。 */
	if (ctl->t_max > 0.0) sprintf(tmax_text, "%.3f", ctl->t_max);
	else                  sprintf(tmax_text, "none");

	/* ---- 头部注释：初值、单位约定、积分设置、守恒量诊断 ---- */
	fprintf(fp, "# alpha particle scattering trajectory (reduced units)\n");
	fprintf(fp, "# units     : charge = e, mass = relative molecular mass, "
	            "length = A, Coulomb k = 1\n");
	fprintf(fp, "# nucleus   : charge %.0f e fixed at (%.1f, %.1f)\n",
	        Z_NUCLEUS, X_NUCLEUS, Y_NUCLEUS);
	fprintf(fp, "# projectile: alpha, charge %.0f e, mass %.0f\n",
	        Z_ALPHA, MASS_ALPHA);
	fprintf(fp, "# case      : %d\n", cs->id);
	fprintf(fp, "# label     : %s\n", cs->label);
	fprintf(fp, "# init      : (x0,y0) = (%.4f, %.4f)   (vx0,vy0) = (%.4f, %.4f)\n",
	        cs->x0, cs->y0, cs->vx0, cs->vy0);
	fprintf(fp, "# run       : integrator = %s   dt = %.6f   t_max = %s\n",
	        kind, ctl->dt, tmax_text);
	fprintf(fp, "# stop      : %s   (t_end = %.6f, steps = %ld)\n",
	        stop_why, rep->t_end, rep->steps);
	fprintf(fp, "# invariant : E0 = %.9f   max|dE|/E0 = %.3e\n",
	        rep->e0, rep->de_max);
	fprintf(fp, "#             L0 = %.9f   max|dL|/|L0| = %.3e\n",
	        rep->l0, rep->dl_max);
	fprintf(fp, "# geometry  : r_min = %.6f   (over stored trajectory)\n",
	        rep->r_min);
	fprintf(fp, "# endpoint  : (x,y) = (%.6f, %.6f)   (vx,vy) = (%.6f, %.6f)\n",
	        rep->x_end, rep->y_end, rep->vx_end, rep->vy_end);
	fprintf(fp, "# theta     : %.4f deg  (angle between final and initial "
	            "velocity, in-frame)\n", rep->theta_deg);
	fprintf(fp, "# columns   : t  x  y  vx  vy\n");

	for (i = 0; i < traj->n; i++) {
		fprintf(fp, "%.8f %.8f %.8f %.8f %.8f\n",
		        traj->pt[i].t, traj->pt[i].x, traj->pt[i].y,
		        traj->pt[i].vx, traj->pt[i].vy);
	}

	if (fclose(fp) != 0) return -1;
	return traj->n;
}

void traj_summary_print(const Case *cs, const TrajReport *rep, int n)
{
	printf("%-4d %-18s %-6d %8.3f %12.6f %12.6f %11.3e %10.5f %8.3f\n",
	       cs->id, cs->label, n, rep->t_end,
	       rep->x_end, rep->y_end, rep->de_max, rep->r_min, rep->theta_deg);
}


/* ---------------------------------------------------------------------------
 * 数值解 vs 解析解
 * ------------------------------------------------------------------------- */

void summary_print(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n)
{
	int i;

	printf("\n%s\n", T("数值解与卢瑟福解析解对照",
	                   "Numeric solution vs Rutherford analytic solution"));
	printf("%-4s %-18s %-9s %-9s %-11s %-11s %-9s %-9s\n",
	       "id", "label", "E", "|L|", "r_min数值", "r_min解析",
	       "b渐近", "theta解析");
	for (i = 0; i < n; i++) {
		printf("%-4d %-18s %-9.4f %-9.1f %-11.5f %-11.5f %-9.3f %-9.2f\n",
		       cases[i].id, cases[i].label,
		       reps[i].e0, fabs(reps[i].l0),
		       reps[i].r_min, ref[i].r_min_exact,
		       ref[i].b_asym, ref[i].theta_exact);
	}
	printf("\n%s\n", T(
	   "框内偏转角 vs 渐近偏转角：前者是粒子在 +-22.5 画框内被截断时已经转过的角度，\n"
	   "后者是 r -> 无穷 的卢瑟福值；粒子出射时仍在受力，所以框内值偏小。",
	   "theta(in-frame): angle already turned when the track leaves the +-22.5 box.\n"
	   "theta(asymptotic): Rutherford value at r -> infinity. The particle is still\n"
	   "being repelled as it exits, hence the in-frame value is systematically smaller."));
}

int summary_export(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n,
                   const char *path, int markdown,
                   const RunControl *ctl)
{
	FILE *fp;
	int   i;
	double de_max, dl_max;

	fp = fopen(path, "w");
	if (fp == NULL) return -1;

	if (markdown) {
		fprintf(fp, "# α 粒子散射：数值解与卢瑟福解析解对照\n\n");
		fprintf(fp, "约化单位：电荷 e、质量 相对分子质量、长度 Å、k = 1。"
		            "积分器 %s，dt = %g，t_max = %g。\n\n",
		        ctl->integrator == INTEGRATOR_RK4 ? "经典四阶 Runge-Kutta"
		                                          : "平均速度格式（一阶）",
		        ctl->dt, ctl->t_max);
		fprintf(fp, "| case | 初始位置 | v0 | E | L | r_min 数值 | r_min 解析 "
		            "| 残差 | b 渐近 | 偏心率 | 框内偏转角 | 卢瑟福偏转角 |\n");
		fprintf(fp, "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (i = 0; i < n; i++) {
			fprintf(fp, "| %d | (%.0f, %.0f) | %.1f | %.4f | %.0f | %.5f | %.5f "
			            "| %.1e | %.3f | %.3f | %.2f° | %.2f° |\n",
			        cases[i].id, cases[i].x0, cases[i].y0, cases[i].vx0,
			        reps[i].e0, reps[i].l0, reps[i].r_min, ref[i].r_min_exact,
			        fabs(reps[i].r_min - ref[i].r_min_exact),
			        ref[i].b_asym, ref[i].ecc, reps[i].theta_deg,
			        ref[i].theta_exact);
		}
		/* 取六条轨迹里的最大值：单条轨迹的漂移不能代表整体 */
		de_max = dl_max = 0.0;
		for (i = 0; i < n; i++) {
			if (reps[i].de_max > de_max) de_max = reps[i].de_max;
			if (reps[i].dl_max > dl_max) dl_max = reps[i].dl_max;
		}
		fprintf(fp, "\n守恒量：六条轨迹全程 max|dE|/E = %.1e，max|dL|/|L| = %.1e。\n",
		        de_max, dl_max);
	} else {
		fprintf(fp, "alpha scattering: numeric vs Rutherford analytic "
		            "(reduced units)\n\n");
		fprintf(fp, "%-4s %-14s %-6s %-9s %-9s %-11s %-11s %-9s %-9s %-9s %-9s\n",
		        "id", "init", "v0", "E", "L", "r_min", "r_min_ex",
		        "resid", "b_asym", "ecc", "theta_ex");
		for (i = 0; i < n; i++) {
			char init[24];
			sprintf(init, "(%.0f, %.0f)", cases[i].x0, cases[i].y0);
			fprintf(fp, "%-4d %-14s %-6.1f %-9.4f %-9.0f %-11.5f %-11.5f "
			            "%-9.1e %-9.3f %-9.3f %-9.2f\n",
			        cases[i].id, init, cases[i].vx0, reps[i].e0, reps[i].l0,
			        reps[i].r_min, ref[i].r_min_exact,
			        fabs(reps[i].r_min - ref[i].r_min_exact),
			        ref[i].b_asym, ref[i].ecc, ref[i].theta_exact);
		}
	}

	if (fclose(fp) != 0) return -1;
	return 0;
}
