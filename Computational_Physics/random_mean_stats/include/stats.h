#ifndef STATS_H
#define STATS_H

double stats_mean(const double *x, long n);

/* 样本标准差，分母取 n-1（无偏估计） */
double stats_std(const double *x, long n);

double stats_min(const double *x, long n);
double stats_max(const double *x, long n);

#endif
