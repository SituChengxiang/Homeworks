#ifndef LAGRANGE_H
#define LAGRANGE_H

/* 分箱数固定为 10，留一点余量给以后改箱数 */
#define LAGRANGE_MAX_N 32

/*
 * 由 n 个节点 (x[i], y[i]) 构造 Lagrange 插值多项式，
 * 输出按升幂排列的系数 c[0..n-1]：
 *     P(x) = c0 + c1 x + c2 x^2 + ... + c_{n-1} x^{n-1}
 * n 必须不超过 LAGRANGE_MAX_N。
 */
void lagrange_coeffs(const double *x, const double *y, int n, double *c);

/* 按升幂系数用 Horner 法求值 */
double poly_eval(const double *c, int n, double x);

/*
 * 直接用 Lagrange 基函数求和求值（O(n^2)）。
 * 它和系数展开走的是两条不同的数值路径，用来互相验证。
 */
double lagrange_eval(const double *x, const double *y, int n, double t);

#endif
