#include <math.h>
#include "stats.h"

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
