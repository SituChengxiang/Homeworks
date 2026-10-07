#ifndef COULOMB_H
#define COULOMB_H

#include "units.h"

/* ---------------------------------------------------------------------------
 * 模块二：库仑场（只到「力」为止）
 *
 * 给定 a 粒子的位置，回答三个问题：受力多大、势能多高、离核多远。
 * 这一层与入射粒子是什么无关：把 a 粒子换成质子，F(r) 一个字都不用改，
 * 改变的只是「力怎么变加速度」那一步，所以质量不放在这里。
 *
 *      F_vec = k q1 q2 / r^2 * r_hat = COUPLING * r_vec / r^3
 *                                        （同号电荷相斥，方向背离核）
 *      U(r)  = COUPLING / r             （无穷远为零势点，斥力势为正）
 * ------------------------------------------------------------------------- */

/* 位置 -> 库仑力矢量（约化单位，含符号与方向） */
void coulomb_force(double x, double y, double *fx, double *fy);

/* 该位置的库仑势能 U(r) = k q1 q2 / r */
double coulomb_potential(double x, double y);

/* 该位置对核的位径 r = |r_vec| */
double coulomb_radius(double x, double y);

#endif /* COULOMB_H */
