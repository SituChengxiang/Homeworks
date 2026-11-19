#include "util.h"

int util_is_finite(double v)
{
    if (v != v) {
        return 0;
    }
    if (v - v != 0.0) {
        return 0;
    }
    return 1;
}

int util_all_finite(const double *v, long n)
{
    long i;

    for (i = 0; i < n; i++) {
        if (!util_is_finite(v[i])) {
            return 0;
        }
    }
    return 1;
}