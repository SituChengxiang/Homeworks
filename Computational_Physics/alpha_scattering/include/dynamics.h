#ifndef DYNAMICS_H
#define DYNAMICS_H

#include "units.h"

/* ---------------------------------------------------------------------------
 * 模块三：动力学（由力求加速度）
 *
 *      a_vec = F_vec / m
 *
 * 这一层是「粒子」专属的：库里唯一的自由量就是质量。
 * 场模块给 F，这里给 a，积分模块只管把 a 用掉。
 * 三层各自独立，换积分方法不动物理，换入射粒子不动场。
 *
 * 关于用哪个质量：题目要求原子核固定在 (0,0)，
 * 等价于把核当成无穷重，于是不存在约化质量的修正，
 * a 粒子就用它自己的相对分子质量 4。
 * 如果哪天放开核让它反冲，正确的做法是换成约化质量
 *      mu = m_alpha * m_nucleus / (m_alpha + m_nucleus) = 4*97/101 = 3.84
 * 与 4 差约 4%，在这个能量尺度上轨迹会明显不同。
 * ------------------------------------------------------------------------- */

/* 已知力矢量，求加速度矢量：a = F / m_alpha */
void alpha_acceleration_from_force(double fx, double fy,
				   double *ax, double *ay);

/* 已知位置，直接求该点的加速度（= 查场 + 除质量，省去调用方中转） */
void alpha_acceleration(double x, double y, double *ax, double *ay);

/* a 粒子的动能 1/2 m v^2，能量检查时与势能相加 */
double alpha_kinetic_energy(double vx, double vy);

#endif /* DYNAMICS_H */
