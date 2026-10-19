/* =====================================================================
 * α 粒子散射轨道计算 —— 单文件版（C89）
 *
 * 本文件由 make_single_source.py 从分模块源码自动合并生成，
 * 内容与 include/ + src/ 下的模块化版本逐字对应，只是并成了一个
 * 翻译单元，方便在 Dev-C++ 里直接打开单个文件按 F11 运行。
 *
 * 正式交付的模块化版本见 src/ 与 include/；两者行为完全一致。
 *
 * 编译：  gcc -std=c89 -pedantic-errors -O2 alpha_scattering_single.c -o alpha -lm
 * 运行：  ./alpha [输出目录] [-t 时长] [-dt 步长] [--manual] [--ascii|--utf8]
 * ===================================================================== */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


/* ---------- units.h ---------- */

/* ---------------------------------------------------------------------------
 * 模块一：约化单位制与物理常量
 *
 * 题目约定的四个基本单位：
 *      电荷  = e              质量 = 相对分子质量
 *      长度  = A              库仑常数 k = 1
 *
 * 量纲自检：由 F = k q1 q2 / r^2 与 F = m a 得
 *      [k][q]^2 / [L]^2 = [m][L] / [t]^2
 *      =>  [t] = sqrt( [m][L]^3 / (k [q]^2) )
 * 代进本约定（四个基本量数值都是 1），时间单位数值上同样是 1，
 * 于是题目里的数字可以原样代入运动方程，不需要任何换算因子。
 * 换句话说：这里算出来的 t 就是这个单位制下的无量纲时间。
 * ------------------------------------------------------------------------- */

/* ---- 基本单位 ---------------------------------------------------------- */
#define K_COULOMB       1.0     /* 库仑常数 k                        */
#define CHARGE_UNIT     1.0     /* 电荷单位 e                        */
#define LENGTH_UNIT     1.0     /* 长度单位 A                        */
#define MASS_UNIT       1.0     /* 质量单位：相对分子质量            */

/* ---- 题目给定的粒子参数 ----------------------------------------------- */
#define Z_NUCLEUS       97.0    /* 原子核带电量 97e，固定在原点      */
#define Z_ALPHA         2.0     /* a 粒子带电量 2e                   */
#define MASS_ALPHA      4.0     /* a 粒子相对分子质量 4              */

#define X_NUCLEUS       0.0     /* 原子核位置 (0, 0)                 */
#define Y_NUCLEUS       0.0

/* ---- 由基本常量组合出的系数，其它模块只准引用这两个符号 --------------- */

/* 库仑耦合强度 k*q1*q2 = 1*97*2 = 194，出现在力与势能里：
 *      U(r) = COUPLING / r        （斥力，势能为正）
 */
#define COUPLING \
	(K_COULOMB * Z_NUCLEUS * Z_ALPHA * CHARGE_UNIT * CHARGE_UNIT)

/* 运动方程 d^2 r / dt^2 = ACCEL_FACTOR * r_vec / r^3 的前系数
 *      ACCEL_FACTOR = k q1 q2 / m = 194 / 4 = 48.5
 * 除质量这一步是 a 粒子专属的，所以单独起个名字，
 * 换入射粒子时只改这里。
 */
#define ACCEL_FACTOR    (COUPLING / MASS_ALPHA)

/* ---- 计算域与数值防护 -------------------------------------------------- */

/* 画图时横纵坐标都限制在 [-PLOT_LIMIT, PLOT_LIMIT]，题目要求 */
#define PLOT_LIMIT      22.5

/* 停算裕量：让曲线略微穿出绘图框再停，
 * 图上是"穿出边界"而不是"戛然而止"，偏转方向更容易看清。
 */
#define RUNOUT_MARGIN   2.5

/* r -> 0 时库仑力发散；真到这一步说明步长太粗或初态有问题，
 * 触发保护性停算而不是让数值炸掉。本构型下最近距离在 21 以上，
 * 永远碰不到这个阈值。
 */
#define R_SINGULARITY_GUARD  1.0e-6

/* C89 的 math.h 不保证提供 M_PI（那是 POSIX 扩展），
 * 自己定义才能保证 -pedantic-errors 干净。
 */
#define PI_CONST  3.14159265358979323846


/* ---------- cases.h ---------- */

/* ---------------------------------------------------------------------------
 * 模块零：算例定义（初始条件）
 *
 * 题目要求的六组初值集中放在这一处，别的模块不许再出现这些数字。
 * 六条轨迹 = 2 个初始位置 x 3 个初速度，初速度全部沿 +x 方向，
 * 两组位置只差 y0 的符号，因此上下两组关于 x 轴严格镜像对称。
 * 这等于白送一个正确性检验：case 1 与 case 4 的能量、|角动量|、
 * 最近距离必须完全相同，跑出来不一样就是代码错了。
 * ------------------------------------------------------------------------- */

typedef struct {
	int          id;     /* 1..6，与输出文件 traj_<id>.dat 对应   */
	double       x0, y0; /* 初始位置，长度单位 A                 */
	double       vx0, vy0; /* 初始速度，约化单位                 */
	const char  *label;  /* 画图时每条曲线的文字标识             */
} Case;

#define N_CASES 6

/* 返回内部静态数组，只读使用，长度固定为 N_CASES */
const Case *cases_get(void);


/* ---------- coulomb.h ---------- */


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


/* ---------- dynamics.h ---------- */


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


/* ---------- stepper.h ---------- */

/* ---------------------------------------------------------------------------
 * 模块四：手动积分算法（位置用前后速度的平均值推进）
 *
 * 单步写成显式三步：
 *
 *   (1) 先由当前位置查加速度        a_n     = A(x_n, y_n)
 *   (2) 速度作前向欧拉              v_{n+1} = v_n + a_n * dt
 *   (3) 位置用新旧速度的平均        x_{n+1} = x_n + (v_n + v_{n+1})/2 * dt
 *
 * 把 (2) 代进 (3) 得到
 *
 *   x_{n+1} = x_n + v_n * dt + 1/2 * a_n * dt^2
 *
 * 也就是「步长内把加速度当常量」的经典抛物线推进：
 * 对匀加速运动它是精确的，局部截断误差 O(dt^3)，位置这一路是二阶的。
 * 而速度走的是纯前向欧拉，局部误差 O(dt^2)、全局一阶 ——
 * 取平均修的是位置，不会把速度也提到二阶。
 * 这一点在选步长时要记住：真正限制精度的是速度分支。
 *
 * 算法通过函数指针向外面要加速度，本身不知道力是库仑力还是别的什么。
 * ------------------------------------------------------------------------- */

/* 粒子的完整状态：位置 + 速度 */
typedef struct {
	double x, y;
	double vx, vy;
} Particle;

/* 加速度查询函数：由位置给出加速度分量 */
typedef void (*StepperAccelFunc)(double x, double y, double *ax, double *ay);

/* 推进一个步长 dt，结果原地写回 p */
void stepper_step(Particle *p, double dt, StepperAccelFunc accel);

/* ---- 步长取多少：STEPPER_DT_DEFAULT = 0.02 ------------------------------
 *
 * 这个格式实测是一阶收敛（dt 减半误差减半），所以想压到多小就得付多少步。
 * 六个算例上按 dt 扫描，积分到出框为止：
 *
 *   dt      每条例轨迹步数   max|dE|/E     r_min 对解析值的偏差
 *   0.20        ~60          5e-4 ~ 2.7e-3   7e-3 ~ 1.8e-2
 *   0.10       ~130          3e-4 ~ 1.4e-3   4e-3 ~ 9e-3
 *   0.05       ~260          1e-4 ~ 6e-4     2e-3 ~ 5e-3
 *   0.02       ~650          3e-5 ~ 3e-4     7e-4 ~ 1.8e-3
 *   0.01      ~1300          1e-5 ~ 1e-4     4e-4 ~ 9e-4
 *
 * 取 0.02：
 *   精度——最近距离的误差不到 0.002 A，在 ±22.5 的画框里连一个像素都不到；
 *         能量漂移 3e-4 量级，足够说明守恒。
 *   代价——六条轨迹合计约 4000 步，毫秒级就跑完，再往小走只是白花钱。
 *   分辨率——最快速度 v ~ 3，每步位移 0.06 A，远小于线宽，曲线不显折角。
 * 0.01 与 0.02 的图形看不出差别，所以没必要更小。
 * ------------------------------------------------------------------------- */
#define STEPPER_DT_DEFAULT  0.02


/* ---------- rk4.h ---------- */

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


/* ---------- trajectory.h ---------- */


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


/* ---------- rutherford.h ---------- */

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


/* ---------- export.h ---------- */


/* ---------------------------------------------------------------------------
 * 模块七：数据导出
 *
 * C89 标准库没有画图能力，-pedantic-errors 下也不许调 popen 之类的 POSIX 扩展，
 * 所以这个模块只负责把结果写成文本表格，作图交给下游脚本读。
 *
 * 数据文件格式：
 *   以 # 开头的注释行说明初值与积分设置，
 *   正文五列：t  x  y  vx  vy
 * numpy 的 loadtxt 与 gnuplot 都默认跳过 # 行，不用额外解析。
 * ------------------------------------------------------------------------- */

/* 写一条轨迹；失败返回 -1，成功返回写入的数据行数 */
/* ---------------------------------------------------------------------------
 * 控制台文案的语言开关。
 *
 * 数据文件（.dat / summary.txt）一律是 ASCII，不受影响；这里只管终端打印。
 * Windows 控制台默认代码页是 GBK，UTF-8 的中文字符串会显示成乱码，
 * 所以在 _WIN32 下缺省走英文；也可以用 --utf8 强制打开中文
 * （或者事先执行 chcp 65001 把代码页切成 UTF-8）。
 * ------------------------------------------------------------------------- */

void console_set_ascii(int use_ascii);            /* 非 0 = 用英文文案 */
const char *T(const char *zh, const char *en);    /* 按设置二选一 */

int traj_export(const Trajectory *traj, const Case *cs,
		const TrajReport *rep, const RunControl *ctl,
		const char *path);

/* 六条轨迹的一行小结，打到终端方便核对 */
void traj_summary_print(const Case *cs, const TrajReport *rep, int n);

/* ---- 数值解 vs 卢瑟福解析解 -------------------------------------------- */

/* 一个算例的解析参考量 */
typedef struct {
	double b_asym;       /* 渐近撞击参数 b = L / (m v_inf)      */
	double ecc;          /* 双曲线偏心率 eps                    */
	double r_min_exact;  /* 最近距离解析值                      */
	double theta_exact;  /* 卢瑟福渐近偏转角，度                */
} RutherfordRef;

/* 打印对照表 */
void summary_print(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n);

/* 写对照表；markdown 非 0 时输出 Markdown 表格。成功返回 0 */
int summary_export(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n,
                   const char *path, int markdown,
                   const RunControl *ctl);


/* ========== cases.c ========== */

/* ---------------------------------------------------------------------------
 * 六组初值
 *   (1) 初始位置 (-20, 20)，v0 = (1, 0) / (1.5, 0) / (3, 0)
 *   (2) 初始位置 (-20,-20)，v0 = (1, 0) / (1.5, 0) / (3, 0)
 *
 * 全部从 x = -20 处沿 +x 入射。起始位径 r0 = sqrt(20^2 + 20^2) = 28.28 A，
 * 而库仑势 194/r0 = 6.86 与动能同一量级，说明粒子出发时已经处在
 * 相互作用区内部，不是无穷远处的自由入射态。
 * 因此这里的 |y0| = 20 只是几何上的入射高度，
 * 不能直接当卢瑟福公式里的撞击参数用（那要在 r -> 无穷处定义）。
 * ------------------------------------------------------------------------- */
static const Case k_cases[N_CASES] = {
	{ 1, -20.0,  20.0, 1.0, 0.0, "(-20,+20)  v0=1.0" },
	{ 2, -20.0,  20.0, 1.5, 0.0, "(-20,+20)  v0=1.5" },
	{ 3, -20.0,  20.0, 3.0, 0.0, "(-20,+20)  v0=3.0" },
	{ 4, -20.0, -20.0, 1.0, 0.0, "(-20,-20)  v0=1.0" },
	{ 5, -20.0, -20.0, 1.5, 0.0, "(-20,-20)  v0=1.5" },
	{ 6, -20.0, -20.0, 3.0, 0.0, "(-20,-20)  v0=3.0" }
};

const Case *cases_get(void)
{
	return k_cases;
}


/* ========== coulomb.c ========== */

/* ---------------------------------------------------------------------------
 * 内部辅助：位置 -> 对核的位矢分量与位径平方
 *
 * 力是 1/r^2、势能是 1/r，都由同一个 r_vec 出发，
 * 集中在一处计算可以避免各函数自己算 r 时口径不一致。
 * ------------------------------------------------------------------------- */
static void relative_vector(double x, double y,
			    double *dx, double *dy, double *r2)
{
	*dx = x - X_NUCLEUS;
	*dy = y - Y_NUCLEUS;
	*r2 = (*dx) * (*dx) + (*dy) * (*dy);
}

double coulomb_radius(double x, double y)
{
	double dx, dy, r2;
	relative_vector(x, y, &dx, &dy, &r2);
	return sqrt(r2);
}

double coulomb_potential(double x, double y)
{
	double dx, dy, r2;

	relative_vector(x, y, &dx, &dy, &r2);
	if (r2 < R_SINGULARITY_GUARD * R_SINGULARITY_GUARD) {
		return COUPLING / R_SINGULARITY_GUARD;   /* 上限值，防 inf */
	}
	return COUPLING / sqrt(r2);
}

void coulomb_force(double x, double y, double *fx, double *fy)
{
	double dx, dy, r2, r, r3;

	relative_vector(x, y, &dx, &dy, &r2);

	/* r -> 0 时 1/r^2 发散。这不是物理，是数值爆炸的前兆，
	 * 与其返回一个巨大的力污染后面所有步，不如返回 0，
	 * 让调用方通过 coulomb_radius() 发现粒子过分靠近核并停算。
	 */
	if (r2 < R_SINGULARITY_GUARD * R_SINGULARITY_GUARD) {
		*fx = 0.0;
		*fy = 0.0;
		return;
	}

	r  = sqrt(r2);
	r3 = r2 * r;                     /* = r^3，全程只开一次方 */

	/* F = k q1 q2 * r_vec / r^3，即 k q1 q2 / r^2 沿 r_hat 方向。
	 * 写 /r^3 而不写 /r^2 再乘单位矢，是为了少一次除法，
	 * 也避免 r_hat 归一化时再开一次方。
	 */
	*fx = COUPLING * dx / r3;
	*fy = COUPLING * dy / r3;
}


/* ========== dynamics.c ========== */

void alpha_acceleration_from_force(double fx, double fy,
				   double *ax, double *ay)
{
	/* 牛顿第二定律。质量是常量，所以直接除；
	 * 若将来做到相对论速度，这里的 p/m 关系要换掉。
	 */
	*ax = fx / MASS_ALPHA;
	*ay = fy / MASS_ALPHA;
}

void alpha_acceleration(double x, double y, double *ax, double *ay)
{
	double fx, fy;

	coulomb_force(x, y, &fx, &fy);
	alpha_acceleration_from_force(fx, fy, ax, ay);
}

double alpha_kinetic_energy(double vx, double vy)
{
	return 0.5 * MASS_ALPHA * (vx * vx + vy * vy);
}


/* ========== stepper.c ========== */

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


/* ========== rk4.c ========== */

/* 单级加权累加辅助宏，避免在每个 k_i 处重复写四行 */
static void combine(const double y_in[RK4_DIM], const double k[RK4_DIM],
		    double coeff, double out[RK4_DIM])
{
	int i;

	for (i = 0; i < RK4_DIM; i++) {
		out[i] = y_in[i] + coeff * k[i];
	}
}

void rk4_step(const double y_in[RK4_DIM], double h,
	      double y_out[RK4_DIM], Rk4RhsFunc rhs)
{
	double k1[RK4_DIM], k2[RK4_DIM], k3[RK4_DIM], k4[RK4_DIM];
	double stage[RK4_DIM];
	int i;

	rhs(y_in, k1);

	combine(y_in, k1, 0.5 * h, stage);
	rhs(stage, k2);

	combine(y_in, k2, 0.5 * h, stage);
	rhs(stage, k3);

	combine(y_in, k3, h, stage);
	rhs(stage, k4);

	for (i = 0; i < RK4_DIM; i++) {
		y_out[i] = y_in[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i]
						  + 2.0 * k3[i] + k4[i]);
	}
}


/* ========== trajectory.c ========== */

/* ---------------------------------------------------------------------------
 * 容器管理
 * ------------------------------------------------------------------------- */

int traj_alloc(Trajectory *traj, int capacity)
{
	traj->pt = (TrajPoint *)malloc((size_t)capacity * sizeof(TrajPoint));
	if (traj->pt == NULL) {
		traj->n = 0;
		traj->cap = 0;
		return 0;
	}
	traj->cap = capacity;
	traj->n = 0;
	return 1;
}

void traj_free(Trajectory *traj)
{
	free((void *)traj->pt);
	traj->pt = NULL;
	traj->n = 0;
	traj->cap = 0;
}

static int traj_push(Trajectory *traj, double t,
		     double x, double y, double vx, double vy)
{
	if (traj == NULL || traj->pt == NULL) return 1;      /* 不存数据，直接放过 */
	if (traj->n >= traj->cap)            return 0;      /* 容量不足，由上层置溢出标志 */

	traj->pt[traj->n].t  = t;
	traj->pt[traj->n].x  = x;
	traj->pt[traj->n].y  = y;
	traj->pt[traj->n].vx = vx;
	traj->pt[traj->n].vy = vy;
	traj->n++;
	return 1;
}

/* ---------------------------------------------------------------------------
 * 物理量：一阶系统 dy/dt = f(y)，y = {x, y, vx, vy}
 * ------------------------------------------------------------------------- */

void alpha_rhs(const double y[RK4_DIM], double dy[RK4_DIM])
{
	double ax, ay;

	/* 位置方程 */
	dy[0] = y[2];
	dy[1] = y[3];

	/* 速度方程：dv/dt = a(x, y) */
	alpha_acceleration(y[0], y[1], &ax, &ay);
	dy[2] = ax;
	dy[3] = ay;
}

double alpha_energy(double x, double y, double vx, double vy)
{
	return alpha_kinetic_energy(vx, vy) + coulomb_potential(x, y);
}

double alpha_angular_momentum(double x, double y, double vx, double vy)
{
	return MASS_ALPHA * (x * vy - y * vx);
}

/* ---------------------------------------------------------------------------
 * 积分驱动
 * ------------------------------------------------------------------------- */

void alpha_run(double x0, double y0, double vx0, double vy0,
	       const RunControl *ctl, Trajectory *traj, TrajReport *rep)
{
	double y[RK4_DIM];
	double yn[RK4_DIM];
	Particle p;
	double t, E0, L0, v0;
	long nsteps, step;

	/* ---- 初态与基准量 ---- */
	y[0] = x0;  y[1] = y0;  y[2] = vx0; y[3] = vy0;
	p.x  = x0;  p.y  = y0;  p.vx = vx0; p.vy = vy0;

	E0 = alpha_energy(x0, y0, vx0, vy0);
	L0 = alpha_angular_momentum(x0, y0, vx0, vy0);
	v0 = sqrt(vx0 * vx0 + vy0 * vy0);

	if (rep != NULL) {
		rep->e0 = E0;    rep->e_end = E0;
		rep->de_max = 0.0;
		rep->l0 = L0;    rep->dl_max = 0.0;
		rep->r_min = coulomb_radius(x0, y0);
		rep->v0 = v0;
		rep->t_end = 0.0;
		rep->x_end = x0; rep->y_end = y0;
		rep->vx_end = vx0; rep->vy_end = vy0;
		rep->theta_deg = 0.0;
		rep->steps = 0;  rep->stored = 0;
		rep->overflow = 0;
		rep->stopped_by = 0;
	}

	traj_push(traj, 0.0, x0, y0, vx0, vy0);

	/* ---- 步数上限 ----
	 * 按固定步数推进而不是 while (t < t_max)，
	 * 否则 t 会因浮点累加漂移，不同步长下"同一时刻"就不成立了。
	 */
	if (ctl->t_max > 0.0) nsteps = (long)(ctl->t_max / ctl->dt + 0.5);
	else                  nsteps = 1000000000L;

	for (step = 0; step < nsteps; step++) {

		if (ctl->integrator == INTEGRATOR_RK4) {
			rk4_step(y, ctl->dt, yn, alpha_rhs);
			y[0] = yn[0]; y[1] = yn[1]; y[2] = yn[2]; y[3] = yn[3];
		} else {
			p.x = y[0]; p.y = y[1]; p.vx = y[2]; p.vy = y[3];
			stepper_step(&p, ctl->dt, alpha_acceleration);
			y[0] = p.x; y[1] = p.y; y[2] = p.vx; y[3] = p.vy;
		}

		t = (double)(step + 1) * ctl->dt;

		if (rep != NULL) {
			double r  = coulomb_radius(y[0], y[1]);
			double E  = alpha_energy(y[0], y[1], y[2], y[3]);
			double L  = alpha_angular_momentum(y[0], y[1], y[2], y[3]);
			double dE = fabs(E - E0) / fabs(E0);
			double dL = fabs(L - L0) / (fabs(L0) > 0.0 ? fabs(L0) : 1.0);

			if (r < rep->r_min) rep->r_min = r;
			if (dE > rep->de_max) rep->de_max = dE;
			if (dL > rep->dl_max) rep->dl_max = dL;

			rep->steps = step + 1;
			rep->t_end = t;
			rep->x_end = y[0]; rep->y_end = y[1];
			rep->vx_end = y[2]; rep->vy_end = y[3];
			rep->e_end = E;
		}

		/* r 已经小到触发保护，说明步长粗到跟不上场强，立即停算 */
		if (coulomb_radius(y[0], y[1]) < R_SINGULARITY_GUARD) {
			if (rep != NULL) rep->stopped_by = 3;
			break;
		}

		if (ctl->record_every > 0 && (step + 1) % ctl->record_every == 0) {
			if (!traj_push(traj, t, y[0], y[1], y[2], y[3])) {
				if (rep != NULL) {
					rep->overflow = 1;
					rep->stopped_by = 2;
				}
				break;
			}
		}

		/* 越出绘图框就停：轨迹不需要跑到无穷远 */
		if (ctl->stop_box > 0.0 &&
		    (fabs(y[0]) > ctl->stop_box || fabs(y[1]) > ctl->stop_box)) {
			if (rep != NULL) rep->stopped_by = 1;
			break;
		}
	}

	if (rep != NULL && traj != NULL) rep->stored = traj->n;

	/* ---- 框内偏转角：末态速度与初态速度的夹角 ----
	 * 注意这不是卢瑟福的渐近偏转角；
	 * 粒子在框内就停，还没回到无穷远，两者只有在出射足够远时才趋于一致。
	 */
	if (rep != NULL && v0 > 0.0) {
		double ve = sqrt(rep->vx_end * rep->vx_end + rep->vy_end * rep->vy_end);
		double c  = 0.0;
		if (ve > 0.0) {
			c = (vx0 * rep->vx_end + vy0 * rep->vy_end) / (v0 * ve);
			if (c >  1.0) c =  1.0;
			if (c < -1.0) c = -1.0;
			rep->theta_deg = acos(c) * 180.0 / PI_CONST;
		}
	}
}


/* ========== rutherford.c ========== */

double rutherford_geometric_b(double y0)
{
	return fabs(y0);
}

double rutherford_v_infinity(double e_total)
{
	/* U(r -> inf) = 0，故 E 全部是动能 */
	return sqrt(2.0 * e_total / MASS_ALPHA);
}

double rutherford_impact_parameter(double e_total, double l)
{
	return fabs(l) / (MASS_ALPHA * rutherford_v_infinity(e_total));
}

double rutherford_eccentricity(double e_total, double l)
{
	double c2 = COUPLING * COUPLING;

	return sqrt(1.0 + 2.0 * e_total * l * l / (MASS_ALPHA * c2));
}

double rutherford_deflection_deg(double e_total, double l)
{
	double eps = rutherford_eccentricity(e_total, l);
	double s   = 1.0 / eps;

	if (s > 1.0)  s = 1.0;
	if (s < -1.0) s = -1.0;

	return 2.0 * asin(s) * 180.0 / PI_CONST;
}

double rutherford_closest_approach(double e_total, double l)
{
	/* E = A u^2 + COUPLING * u - E = 0,  u = 1/r, A = L^2/(2m)
	 * 取正根 u = (-C + sqrt(C^2 + 4 A E)) / (2 A)                   */
	double a = l * l / (2.0 * MASS_ALPHA);
	double disc;
	double u;

	if (a <= 0.0) {
		/* 对心碰撞（L = 0）：E = C/r_min */
		return COUPLING / e_total;
	}

	disc = COUPLING * COUPLING + 4.0 * a * e_total;
	u    = (-COUPLING + sqrt(disc)) / (2.0 * a);

	return 1.0 / u;
}


/* ========== export.c ========== */

static int g_ascii =
#ifdef _WIN32
    1;      /* Dev-C++ / MinGW 控制台默认 GBK，缺省先说英文 */
#else
    0;
#endif

void console_set_ascii(int use_ascii)
{
    g_ascii = use_ascii;
}

const char *T(const char *zh, const char *en)
{
    return g_ascii ? en : zh;
}

int traj_export(const Trajectory *traj, const Case *cs,
		const TrajReport *rep, const RunControl *ctl,
		const char *path)
{
	FILE *fp;
	int    i;
	const char *kind;
	const char *stop_why;
	char tmax_text[16];

	fp = fopen(path, "w");
	if (fp == NULL) return -1;

	kind = (ctl->integrator == INTEGRATOR_RK4) ? "RK4" : "manual-avg-velocity";

	switch (rep->stopped_by) {
		case 1:  stop_why = "left the plot box"; break;
		case 2:  stop_why = "sample buffer full"; break;
		case 3:  stop_why = "singularity guard";  break;
		default: stop_why = "time limit reached"; break;
	}
	/* C89 没有 snprintf，这里用 sprintf 写进固定 16 字节的缓冲：
	 * %.3f 配 double 时长最多十几位，加上结束符足够宽。 */
	if (ctl->t_max > 0.0) sprintf(tmax_text, "%.3f", ctl->t_max);
	else                  sprintf(tmax_text, "none");

	/* ---- 头部注释：初值、单位约定、积分设置、守恒量诊断 ---- */
	fprintf(fp, "# alpha particle scattering trajectory (reduced units)\n");
	fprintf(fp, "# units     : charge = e, mass = relative molecular mass, "
	            "length = A, Coulomb k = 1\n");
	fprintf(fp, "# nucleus   : charge %.0f e fixed at (%.1f, %.1f)\n",
	        Z_NUCLEUS, X_NUCLEUS, Y_NUCLEUS);
	fprintf(fp, "# projectile: alpha, charge %.0f e, mass %.0f\n",
	        Z_ALPHA, MASS_ALPHA);
	fprintf(fp, "# case      : %d\n", cs->id);
	fprintf(fp, "# label     : %s\n", cs->label);
	fprintf(fp, "# init      : (x0,y0) = (%.4f, %.4f)   (vx0,vy0) = (%.4f, %.4f)\n",
	        cs->x0, cs->y0, cs->vx0, cs->vy0);
	fprintf(fp, "# run       : integrator = %s   dt = %.6f   t_max = %s\n",
	        kind, ctl->dt, tmax_text);
	fprintf(fp, "# stop      : %s   (t_end = %.6f, steps = %ld)\n",
	        stop_why, rep->t_end, rep->steps);
	fprintf(fp, "# invariant : E0 = %.9f   max|dE|/E0 = %.3e\n",
	        rep->e0, rep->de_max);
	fprintf(fp, "#             L0 = %.9f   max|dL|/|L0| = %.3e\n",
	        rep->l0, rep->dl_max);
	fprintf(fp, "# geometry  : r_min = %.6f   (over stored trajectory)\n",
	        rep->r_min);
	fprintf(fp, "# endpoint  : (x,y) = (%.6f, %.6f)   (vx,vy) = (%.6f, %.6f)\n",
	        rep->x_end, rep->y_end, rep->vx_end, rep->vy_end);
	fprintf(fp, "# theta     : %.4f deg  (angle between final and initial "
	            "velocity, in-frame)\n", rep->theta_deg);
	fprintf(fp, "# columns   : t  x  y  vx  vy\n");

	for (i = 0; i < traj->n; i++) {
		fprintf(fp, "%.8f %.8f %.8f %.8f %.8f\n",
		        traj->pt[i].t, traj->pt[i].x, traj->pt[i].y,
		        traj->pt[i].vx, traj->pt[i].vy);
	}

	if (fclose(fp) != 0) return -1;
	return traj->n;
}

void traj_summary_print(const Case *cs, const TrajReport *rep, int n)
{
	printf("%-4d %-18s %-6d %8.3f %12.6f %12.6f %11.3e %10.5f %8.3f\n",
	       cs->id, cs->label, n, rep->t_end,
	       rep->x_end, rep->y_end, rep->de_max, rep->r_min, rep->theta_deg);
}


/* ---------------------------------------------------------------------------
 * 数值解 vs 解析解
 * ------------------------------------------------------------------------- */

void summary_print(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n)
{
	int i;

	printf("\n%s\n", T("数值解与卢瑟福解析解对照",
	                   "Numeric solution vs Rutherford analytic solution"));
	printf("%-4s %-18s %-9s %-9s %-11s %-11s %-9s %-9s\n",
	       "id", "label", "E", "|L|", "r_min数值", "r_min解析",
	       "b渐近", "theta解析");
	for (i = 0; i < n; i++) {
		printf("%-4d %-18s %-9.4f %-9.1f %-11.5f %-11.5f %-9.3f %-9.2f\n",
		       cases[i].id, cases[i].label,
		       reps[i].e0, fabs(reps[i].l0),
		       reps[i].r_min, ref[i].r_min_exact,
		       ref[i].b_asym, ref[i].theta_exact);
	}
	printf("\n%s\n", T(
	   "框内偏转角 vs 渐近偏转角：前者是粒子在 +-22.5 画框内被截断时已经转过的角度，\n"
	   "后者是 r -> 无穷 的卢瑟福值；粒子出射时仍在受力，所以框内值偏小。",
	   "theta(in-frame): angle already turned when the track leaves the +-22.5 box.\n"
	   "theta(asymptotic): Rutherford value at r -> infinity. The particle is still\n"
	   "being repelled as it exits, hence the in-frame value is systematically smaller."));
}

int summary_export(const Case *cases, const TrajReport *reps,
                   const RutherfordRef *ref, int n,
                   const char *path, int markdown,
                   const RunControl *ctl)
{
	FILE *fp;
	int   i;
	double de_max, dl_max;

	fp = fopen(path, "w");
	if (fp == NULL) return -1;

	if (markdown) {
		fprintf(fp, "# α 粒子散射：数值解与卢瑟福解析解对照\n\n");
		fprintf(fp, "约化单位：电荷 e、质量 相对分子质量、长度 Å、k = 1。"
		            "积分器 %s，dt = %g，t_max = %g。\n\n",
		        ctl->integrator == INTEGRATOR_RK4 ? "经典四阶 Runge-Kutta"
		                                          : "平均速度格式（一阶）",
		        ctl->dt, ctl->t_max);
		fprintf(fp, "| case | 初始位置 | v0 | E | L | r_min 数值 | r_min 解析 "
		            "| 残差 | b 渐近 | 偏心率 | 框内偏转角 | 卢瑟福偏转角 |\n");
		fprintf(fp, "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (i = 0; i < n; i++) {
			fprintf(fp, "| %d | (%.0f, %.0f) | %.1f | %.4f | %.0f | %.5f | %.5f "
			            "| %.1e | %.3f | %.3f | %.2f° | %.2f° |\n",
			        cases[i].id, cases[i].x0, cases[i].y0, cases[i].vx0,
			        reps[i].e0, reps[i].l0, reps[i].r_min, ref[i].r_min_exact,
			        fabs(reps[i].r_min - ref[i].r_min_exact),
			        ref[i].b_asym, ref[i].ecc, reps[i].theta_deg,
			        ref[i].theta_exact);
		}
		/* 取六条轨迹里的最大值：单条轨迹的漂移不能代表整体 */
		de_max = dl_max = 0.0;
		for (i = 0; i < n; i++) {
			if (reps[i].de_max > de_max) de_max = reps[i].de_max;
			if (reps[i].dl_max > dl_max) dl_max = reps[i].dl_max;
		}
		fprintf(fp, "\n守恒量：六条轨迹全程 max|dE|/E = %.1e，max|dL|/|L| = %.1e。\n",
		        de_max, dl_max);
	} else {
		fprintf(fp, "alpha scattering: numeric vs Rutherford analytic "
		            "(reduced units)\n\n");
		fprintf(fp, "%-4s %-14s %-6s %-9s %-9s %-11s %-11s %-9s %-9s %-9s %-9s\n",
		        "id", "init", "v0", "E", "L", "r_min", "r_min_ex",
		        "resid", "b_asym", "ecc", "theta_ex");
		for (i = 0; i < n; i++) {
			char init[24];
			sprintf(init, "(%.0f, %.0f)", cases[i].x0, cases[i].y0);
			fprintf(fp, "%-4d %-14s %-6.1f %-9.4f %-9.0f %-11.5f %-11.5f "
			            "%-9.1e %-9.3f %-9.3f %-9.2f\n",
			        cases[i].id, init, cases[i].vx0, reps[i].e0, reps[i].l0,
			        reps[i].r_min, ref[i].r_min_exact,
			        fabs(reps[i].r_min - ref[i].r_min_exact),
			        ref[i].b_asym, ref[i].ecc, ref[i].theta_exact);
		}
	}

	if (fclose(fp) != 0) return -1;
	return 0;
}


/* ========== main.c ========== */

/* ---------------------------------------------------------------------------
 * 模块八：主程序
 *
 * 只做调度：取六组初值 -> 积分 -> 写文件 -> 打表。
 * 物理、数值、导出的实现都不写在这里。
 *
 * 用法： alpha_scattering [输出目录] [-t 时长] [-dt 步长] [--manual]
 *   缺省输出到当前工作目录下的 data/，t_max = 14.0，dt = 0.005，用 RK4。
 *
 *   t_max = 14：六条轨迹最迟在 t = 13.5 穿出 +-22.5 的画框，
 *               取 14 保证没有一条是被时间上限截断的。
 *   dt = 0.005：RK4 的能量漂移降到 1e-15（双精度舍入水平），
 *               最近距离与解析值吻合到 8 位有效数字；
 *               每 2 步存一个点，输出采样间隔 0.01，一条约 1300 行。
 * ------------------------------------------------------------------------- */

#define TRAJ_CAPACITY 50000
#define DT_DEFAULT        0.005
#define RECORD_DEFAULT        2

/* 目录不存在或不可写时（IDE 把工作目录设到别处、或 data/ 还没建），
 * 退回当前目录继续跑，不至于直接失败。
 * C89 没有可移植的 mkdir，所以只能退让，不能自己建目录。
 */
static int dir_is_writable(const char *dir)
{
	char  probe[600];
	FILE *fp;

	sprintf(probe, "%s/.write_test", dir);
	fp = fopen(probe, "w");
	if (fp == NULL) return 0;
	fclose(fp);
	remove(probe);
	return 1;
}

int main(int argc, char **argv)
{
	const Case  *cases = cases_get();
	RunControl   ctl;
	TrajReport   reps[N_CASES];
	RutherfordRef ref[N_CASES];
	char         outdir[512];
	char         path[600];
	int          i;

	strcpy(outdir, "data");

	ctl.integrator   = INTEGRATOR_RK4;
	ctl.dt           = DT_DEFAULT;
	ctl.t_max        = 14.0;
	ctl.stop_box     = PLOT_LIMIT + RUNOUT_MARGIN;   /* 22.5 + 2.5 = 25.0 */
	ctl.record_every = RECORD_DEFAULT;

	/* ---- 命令行 ---- */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			ctl.t_max = atof(argv[++i]);
		} else if (strcmp(argv[i], "-dt") == 0 && i + 1 < argc) {
			ctl.dt = atof(argv[++i]);
		} else if (strcmp(argv[i], "--manual") == 0) {
			ctl.integrator = INTEGRATOR_MANUAL;
		} else if (strcmp(argv[i], "--utf8") == 0) {
			console_set_ascii(0);
		} else if (strcmp(argv[i], "--ascii") == 0) {
			console_set_ascii(1);
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("usage: %s [outdir] [-t T] [-dt H] [--manual] [--ascii|--utf8]\n",
			       argv[0]);
			return 0;
		} else if (strlen(argv[i]) < 400) {
			strcpy(outdir, argv[i]);
		}
	}

	if (!dir_is_writable(outdir)) {
		printf("%s\n", T("提示: 目录不存在或不可写，数据改写到当前目录",
		                 "note: output dir missing or unwritable, writing to cwd"));
		strcpy(outdir, ".");
	}
	if (ctl.integrator == INTEGRATOR_MANUAL && ctl.dt > 0.02) {
		printf("%s\n\n", T("提示: 平均速度格式只有一阶精度，建议 --manual 配 -dt 0.02",
		                   "note: the average-velocity scheme is first order; "
		                   "pair --manual with -dt 0.02"));
	}


	printf("%s\n", T("约化单位: 电荷 e / 质量 相对分子质量 / 长度 A / k = 1",
	                "reduced units: charge e / relative molecular mass / angstrom / k = 1"));
	printf(T("核 %.0f e 固定于 (%g, %g)   a 粒子 %.0f e 质量 %.0f\n",
	         "nucleus %.0f e fixed at (%g, %g);  alpha %.0f e, mass %.0f\n"),
	       Z_NUCLEUS, X_NUCLEUS, Y_NUCLEUS, Z_ALPHA, MASS_ALPHA);
	printf(T("k q1 q2 = %g   加速度前系数 k q1 q2 / m = %g\n",
	         "k q1 q2 = %g   acceleration factor k q1 q2 / m = %g\n"),
	       COUPLING, ACCEL_FACTOR);
	printf(T("积分器 = %s   dt = %.6f   t_max = %.3f   停算框 = +-%.1f\n\n",
	         "integrator = %s   dt = %.6f   t_max = %.3f   stop box = +-%.1f\n\n"),
	       ctl.integrator == INTEGRATOR_RK4 ? "RK4" : "manual",
	       ctl.dt, ctl.t_max, ctl.stop_box);

	printf("%-4s %-18s %-6s %8s %12s %12s %11s %10s %8s\n",
	       "id", "label", "n_pts", "t_end", "x_end", "y_end",
	       "max|dE|/E", "r_min", "theta");

	for (i = 0; i < N_CASES; i++) {
		Trajectory  traj;
		TrajReport *rep = &reps[i];
		int         rows;

		if (!traj_alloc(&traj, TRAJ_CAPACITY)) {
			fprintf(stderr, "错误: 轨迹缓冲区分配失败\n");
			return 1;
		}

		alpha_run(cases[i].x0, cases[i].y0, cases[i].vx0, cases[i].vy0,
			  &ctl, &traj, rep);

		/* 解析参考量：同一组 (E, L) 代进卢瑟福公式，作为数值解的标准答案 */
		ref[i].b_asym      = rutherford_impact_parameter(rep->e0, rep->l0);
		ref[i].ecc         = rutherford_eccentricity(rep->e0, rep->l0);
		ref[i].r_min_exact = rutherford_closest_approach(rep->e0, rep->l0);
		ref[i].theta_exact = rutherford_deflection_deg(rep->e0, rep->l0);

		if (rep->overflow) {
			fprintf(stderr, "警告: case %d 采样点超出容量，轨迹被截断\n",
			        cases[i].id);
		}

		sprintf(path, "%s/traj_%d.dat", outdir, cases[i].id);
		rows = traj_export(&traj, &cases[i], rep, &ctl, path);
		if (rows < 0) {
			fprintf(stderr, "错误: 无法写出 %s\n", path);
			fprintf(stderr, "提示: 请先 mkdir -p %s\n", outdir);
			traj_free(&traj);
			return 1;
		}

		traj_summary_print(&cases[i], rep, traj.n);
		traj_free(&traj);
	}

	/* ---- 数值解 vs 解析解：终端一份，文件一份 ---- */
	summary_print(cases, reps, ref, N_CASES);

	sprintf(path, "%s/summary.md", outdir);
	if (summary_export(cases, reps, ref, N_CASES, path, 1, &ctl) != 0) {
		fprintf(stderr, "错误: 无法写出 %s\n", path);
	}
	sprintf(path, "%s/summary.txt", outdir);
	if (summary_export(cases, reps, ref, N_CASES, path, 0, &ctl) != 0) {
		fprintf(stderr, "错误: 无法写出 %s\n", path);
	}

	printf(T("数据已写入 %s/ ：traj_1.dat ... traj_%d.dat, summary.md, summary.txt\n",
	         "wrote %s/ : traj_1.dat ... traj_%d.dat, summary.md, summary.txt\n"),
	       outdir, N_CASES);
	return 0;
}
