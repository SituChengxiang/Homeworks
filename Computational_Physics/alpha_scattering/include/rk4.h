#ifndef RK4_H
#define RK4_H

/* ---------------------------------------------------------------------------
 * 模块五：四阶 Runge-Kutta 数值积分器
 *
 * 与物理无关，只负责 "给一个求导函数，把状态往前推一步"。
 * 状态向量约定为 4 维：y = {x, y, vx, vy}
 *
 * dy/dt = f(y)，经典四级格式：
 *   k1 = f(y)
 *   k2 = f(y + h/2 * k1)
 *   k3 = f(y + h/2 * k2)
 *   k4 = f(y + h   * k3)
 *   y_{n+1} = y_n + h/6 * (k1 + 2 k2 + 2 k3 + k4)
 * 局部截断误差 O(h^5)，全局 O(h^4)。
 * ------------------------------------------------------------------------- */

#define RK4_DIM 4

/* 求导回调：给定状态 y，写出导数 dy */
typedef void (*Rk4RhsFunc)(const double y[RK4_DIM], double dy[RK4_DIM]);

/* 从 y_in 走一步长 h，结果写入 y_out */
void rk4_step(const double y_in[RK4_DIM], double h,
	      double y_out[RK4_DIM], Rk4RhsFunc rhs);

#endif /* RK4_H */
