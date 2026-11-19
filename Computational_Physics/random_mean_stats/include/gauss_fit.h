#ifndef GAUSS_FIT_H
#define GAUSS_FIT_H

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

#endif
