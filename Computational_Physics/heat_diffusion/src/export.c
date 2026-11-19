#include <stdio.h>
#include <string.h>

#include "config.h"
#include "export.h"

int export_profiles(const char *outdir, const char *name, const Grid *g,
                    const double *u0, const double *u10, const double *u20,
                    const double *u30, const double *ucv,
                    double target, double eps, double t_conv, int periodic,
                    const char *meta)
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
    fprintf(fp, "# %s\n", meta);
    /* 参数一律 key=value，核对程序按 key 取，不靠行的顺序 */
    fprintf(fp, "# param length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(fp, "# param hot_lo=%.17g hot_hi=%.17g hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# param target=%.17g eps=%.17g t_conv=%.17g periodic=%ld\n",
            target, eps, t_conv, (long)periodic);
    fprintf(fp, "# columns: x u_t0 u_t10 u_t20 u_t30 u_converged\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%.17g %.17g %.17g %.17g %.17g %.17g\n",
                g->x[i], u0[i], u10[i], u20[i], u30[i], ucv[i]);
    }
    fclose(fp);
    return 0;
}

int export_stepdt(const char *outdir, const char *name, const Grid *g,
                  const double *ua, const double *ub, double t0,
                  double dt, double alpha, int periodic, const char *meta)
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
    fprintf(fp, "# %s\n", meta);
    /* 参数行用 key=value，核对程序按 key 取，不靠行的顺序 */
    fprintf(fp, "# param length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(fp, "# param dt=%.17g alpha=%.17g r=%.17g periodic=%ld t0=%.17g\n",
            dt, alpha, alpha * dt / (g->dx * g->dx), (long)periodic, t0);
    fprintf(fp, "# param hot_lo=%.17g hot_hi=%.17g hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# columns: x u_t u_t_plus_dt\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%.17g %.17g %.17g\n", g->x[i], ua[i], ub[i]);
    }
    fclose(fp);
    return 0;
}

int evo_open(EvoWriter *w, const char *outdir, const char *name,
             const Grid *g, const char *meta)
{
    w->fp = NULL;
    w->nn = 0;
    w->nslice = 0;
    sprintf(w->path, "%s/%s.dat", outdir, name);
    w->fp = fopen(w->path, "w");
    if (w->fp == NULL) {
        fprintf(stderr, "cannot write %s\n", w->path);
        return 1;
    }
    fprintf(w->fp, "# %s\n", meta);
    fprintf(w->fp, "# length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(w->fp, "# 每行：t 后面跟 nn 个节点温度（完整 u(x_i,t_n)）\n");
    fprintf(w->fp, "# 时间用几何级数抽稀：早期密、后期疏，切片数不超过上限。\n");
    fprintf(w->fp, "# 数值保留 10 位有效数字，画图够用；需要逐位复算的数一律从\n");
    fprintf(w->fp, "# profiles_*.dat 取，那里保留 17 位（双精度可分辨的位数）。\n");
    w->nn = g->nn;
    return 0;
}

int evo_write(EvoWriter *w, double t, const double *u)
{
    long i;

    if (w->fp == NULL) {
        return 1;
    }
    fprintf(w->fp, "%.10g", t);
    for (i = 0; i < w->nn; i++) {
        fprintf(w->fp, " %.10g", u[i]);
    }
    fprintf(w->fp, "\n");
    w->nslice++;
    return 0;
}

int evo_close(EvoWriter *w)
{
    if (w->fp != NULL) {
        fclose(w->fp);
        w->fp = NULL;
    }
    return 0;
}

int export_criterion(const char *outdir, const char *name, const char *meta,
                     const double *eps_step, const long *stop_step,
                     const double *stop_time, const double *dev_at_stop,
                     long nrows, double eps_main, double tau, double dt)
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
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# param eps_main=%.17g tau=%.17g dt=%.17g\n",
            eps_main, tau, dt);
    fprintf(fp, "# columns: eps_step stop_step(-1=未达到) stop_time "
                 "dev_at_stop dev_over_eps\n");
    for (i = 0; i < nrows; i++) {
        if (stop_step[i] > 0) {
            fprintf(fp, "%.17g %ld %.17g %.17g %.17g\n",
                    eps_step[i], stop_step[i], stop_time[i], dev_at_stop[i],
                    dev_at_stop[i] / eps_main);
        } else {
            /* 未达到：时刻与残差都没有意义，写 NA 而不是留个像样的数 */
            fprintf(fp, "%.17g %ld NA NA NA\n", eps_step[i], stop_step[i]);
        }
    }
    fclose(fp);
    return 0;
}

int export_columns(const char *outdir, const char *name, const char *meta,
                   const double *x, const double *const *cols, long ncols,
                   const char *const *col_heads, long nn)
{
    char path[512];
    FILE *fp;
    long i;
    long j;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# columns: x");
    for (j = 0; j < ncols; j++) {
        fprintf(fp, " %s", col_heads[j]);
    }
    fprintf(fp, "\n");
    for (i = 0; i < nn; i++) {
        fprintf(fp, "%.17g", x[i]);
        for (j = 0; j < ncols; j++) {
            fprintf(fp, " %.17g", cols[j][i]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}

int export_summary(const char *outdir, const char *name,
                   const double *rows, long ncols, long nrows,
                   const char *const *heads, const char *meta)
{
    char path[512];
    FILE *fp;
    long i;
    long j;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# columns:");
    for (j = 0; j < ncols; j++) {
        fprintf(fp, " %s", heads[j]);
    }
    fprintf(fp, "\n");
    for (i = 0; i < nrows; i++) {
        for (j = 0; j < ncols; j++) {
            fprintf(fp, "%.17g ", rows[i * ncols + j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}
