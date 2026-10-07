#ifndef RUTHERFORD_H
#define RUTHERFORD_H

/* ---------------------------------------------------------------------------
 * 模块四b：卢瑟福散射解析解（用来给数值结果当"标准答案"）
 *
 * 库仑斥力是有名可解的中心力场，轨道是双曲线，于是下面这些量都能解析算出：
 *
 *   总能量     E = 1/2 m v^2 + k q1 q2 / r        （守恒，等于远场动能）
 *   角动量     L = m |r x v|                      （中心力场守恒）
 *   远场速率   v_inf = sqrt(2 E / m)
 *   渐近撞击参数 b = L / (m v_inf)
 *   轨道偏心率 eps = sqrt(1 + 2 E L^2 / (m (k q1 q2)^2))
 *   偏转角     theta = 2 arcsin(1 / eps)
 *   最近距离   r_min: 由 E = L^2/(2 m r^2) + k q1 q2 / r 解一元二次方程
 *
 * 注意 b != |y0|。粒子起始位径 r0 = 28.28 已经在相互作用区内部，
 * 它到核的垂距 20 只是"几何距离"，不是渐近撞击参数。
 * ------------------------------------------------------------------------- */

/* 有效撞击参数（初速度沿 x、核在原点时，b_geo = |y0|） */
double rutherford_geometric_b(double y0);

/* 远场速率与渐近撞击参数 */
double rutherford_v_infinity(double e_total);
double rutherford_impact_parameter(double e_total, double l);

/* 双曲线偏心率 */
double rutherford_eccentricity(double e_total, double l);

/* 渐近偏转角，单位：度 */
double rutherford_deflection_deg(double e_total, double l);

/* 最近距离 r_min，单位与长度单位一致 */
double rutherford_closest_approach(double e_total, double l);

#endif /* RUTHERFORD_H */
