#ifndef IRWIN_HALL_H
#define IRWIN_HALL_H

/*
 * 三个独立 U(0,1) 随机数之和 S 服从 Irwin-Hall 分布；
 * x = S/3 的分布是它的缩放版本，题目里 x_i 就是这种变量。
 *
 * 这是分箱统计的标准答案：它的密度不是正态分布，
 * 而是一段分段二次曲线（0 与 1 处为零）。第 5 步的正态拟合
 * 只是近似，所以这里单独实现精确分布来验收分箱计数。
 */

/* x 的概率密度，x 在 [0,1] 之外返回 0 */
double ih3_pdf_mean(double x);

/* x 的累积分布，x<0 返回 0，x>1 返回 1 */
double ih3_cdf_mean(double x);

#endif
