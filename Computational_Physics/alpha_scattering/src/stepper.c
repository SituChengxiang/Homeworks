#include "stepper.h"

void stepper_step(Particle *p, double dt, StepperAccelFunc accel)
{
	double ax, ay;
	double vxn, vyn;

	/* (1) 旧位置处的加速度。整步只用这一个值，即认定 dt 内加速度不变。 */
	accel(p->x, p->y, &ax, &ay);

	/* (2) 速度前向欧拉 */
	vxn = p->vx + ax * dt;
	vyn = p->vy + ay * dt;

	/* (3) 位置用新旧速度的算术平均，等价于
	 *     x + vx*dt + 0.5*ax*dt^2，但写成平均形式的物理含义更直白：
	 *     这一步走过的距离 = 平均速度 x 时间。
	 */
	p->x  = p->x + 0.5 * (p->vx + vxn) * dt;
	p->y  = p->y + 0.5 * (p->vy + vyn) * dt;
	p->vx = vxn;
	p->vy = vyn;
}
