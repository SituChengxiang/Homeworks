#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include "rk4.h"
#include "stepper.h"

/* ---------------------------------------------------------------------------
 * 模块六：轨道推进
 *
 * 把 units / coulomb / dynamics / 积分器 串成一条轨迹：
 *   alpha_rhs  —— 本问题的一阶微分系统（物理写成数学形式）
 *   alpha_run  —— 从初值出发推进，按 RunControl 决定停算条件与采样密度
 *   守恒量     —— 沿途记录 E、L 的漂移，用来事后判断步长取得够不够小
 *
 * 积分器可选：手动平均速度格式（1 次力调用/步）或经典四阶 RK（4 次/步），
 * 两者都能算这个题，区别只在精度，接口一致。
 *
 * 存储用堆分配：C89 没有变长数组，几 MB 的数组放栈上会直接爆栈。
 * ------------------------------------------------------------------------- */

/* 可选的推进格式 */
typedef enum {
	INTEGRATOR_MANUAL = 0,   /* 平均速度格式，见 stepper.h   */
	INTEGRATOR_RK4    = 1    /* 经典四阶 Runge-Kutta        */
} IntegratorKind;

/* 一个采样点 */
typedef struct {
	double t;
	double x, y;
	double vx, vy;
} TrajPoint;

/* 一条轨迹的容器 */
typedef struct {
	TrajPoint *pt;   /* 采样点数组，由 traj_alloc 分配 */
	int        n;    /* 已记录点数                     */
	int        cap;  /* 数组容量                       */
} Trajectory;

/* 积分控制参数 */
typedef struct {
	double          dt;            /* 步长                                    */
	IntegratorKind  integrator;    /* 用哪种推进格式                          */
	double          t_max;         /* >0：时间上限，积分到 t_max 就停          */
	double          stop_box;      /* >0：越出 [-stop_box, stop_box]^2 即停    */
	int             record_every;  /* 每多少步存一个点；<=0 表示不存           */
} RunControl;

/* 一条轨迹的结果与验收指标 */
typedef struct {
	double e0;         /* 初始总能量 E = 1/2 m v^2 + k q1 q2 / r        */
	double e_end;      /* 末态总能量                                    */
	double de_max;     /* 全程 |dE|/E0 最大相对漂移 -> 积分精度          */
	double l0;         /* 初始角动量 L = m (x vy - y vx)                */
	double dl_max;     /* 全程 |dL|/|L0| 最大相对漂移 -> 中心力场检验    */
	double r_min;      /* 轨迹到核的最近距离                            */
	double v0;         /* 初始速率                                      */
	double t_end;      /* 实际积到的时刻                                */
	double x_end, y_end;   /* 末态位置                                  */
	double vx_end, vy_end; /* 末态速度                                  */
	double theta_deg;  /* 末态速度与初态速度的夹角（框内偏转角，度）      */
	long   steps;      /* 实际推进步数                                  */
	int    stored;     /* 实际存储点数                                  */
	int    overflow;   /* 1 = 采样点超出容量被截断                      */
	int    stopped_by; /* 0=时间上限 1=越出画框 2=容量截断 3=奇点保护     */
} TrajReport;

/* ---- 容器管理 ---------------------------------------------------------- */
int  traj_alloc(Trajectory *traj, int capacity);
void traj_free(Trajectory *traj);

/* ---- 物理量 ------------------------------------------------------------ */

/* 运动方程右端项：y = {x, y, vx, vy}，供 RK4 回调 */
void alpha_rhs(const double y[RK4_DIM], double dy[RK4_DIM]);

/* 机械能 E = 1/2 m v^2 + k q1 q2 / r */
double alpha_energy(double x, double y, double vx, double vy);

/* 对固定核的角动量 L = m (x vy - y vx)，中心力场下严格守恒 */
double alpha_angular_momentum(double x, double y, double vx, double vy);

/* ---- 积分驱动 ---------------------------------------------------------- */

/* traj 可传 NULL，表示只跑积分取诊断量、不保存数据。 */
void alpha_run(double x0, double y0, double vx0, double vy0,
	       const RunControl *ctl, Trajectory *traj, TrajReport *rep);

#endif /* TRAJECTORY_H */
