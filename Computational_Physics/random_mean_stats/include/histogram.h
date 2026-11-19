#ifndef HISTOGRAM_H
#define HISTOGRAM_H

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

#endif
