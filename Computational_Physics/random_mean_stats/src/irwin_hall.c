#include "irwin_hall.h"

/*
 * S = X1 + X2 + X3 的密度：
 *     f_S(s) = s^2 / 2                   0 <= s <= 1
 *            = (-2s^2 + 6s - 3) / 2     1 <= s <= 2
 *            = (3 - s)^2 / 2            2 <= s <= 3
 * x = S/3，所以 f_x(x) = 3 f_S(3x)。
 */
double ih3_pdf_mean(double x)
{
    double s;

    if (x < 0.0 || x > 1.0) {
        return 0.0;
    }
    s = 3.0 * x;
    if (s <= 1.0) {
        return 1.5 * s * s;
    }
    if (s <= 2.0) {
        return 1.5 * (-2.0 * s * s + 6.0 * s - 3.0);
    }
    return 1.5 * (3.0 - s) * (3.0 - s);
}

double ih3_cdf_mean(double x)
{
    double s;

    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }
    s = 3.0 * x;
    if (s <= 1.0) {
        return s * s * s / 6.0;
    }
    if (s <= 2.0) {
        return (-2.0 * s * s * s + 9.0 * s * s - 9.0 * s + 3.0) / 6.0;
    }
    return 1.0 - (3.0 - s) * (3.0 - s) * (3.0 - s) / 6.0;
}
