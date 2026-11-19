#include "histogram.h"

long hist_bin_index(double x, double lo, double width, long nbins)
{
    long idx;

    if (x < lo) {
        return -1;
    }
    idx = (long)((x - lo) / width);
    if (idx >= nbins) {
        /* 只有恰好等于最右端点时才允许归入最后一箱 */
        if (x <= lo + width * (double)nbins) {
            idx = nbins - 1;
        } else {
            return -1;
        }
    }
    return idx;
}

long hist_compute(const double *x, long n, double lo, double width,
                  long nbins, long *counts)
{
    long i;
    long outside = 0;

    for (i = 0; i < n; i++) {
        long idx = hist_bin_index(x[i], lo, width, nbins);
        if (idx < 0) {
            outside++;
        } else {
            counts[idx]++;
        }
    }
    return outside;
}

double hist_center(long i, double lo, double width)
{
    return lo + ((double)i + 0.5) * width;
}
