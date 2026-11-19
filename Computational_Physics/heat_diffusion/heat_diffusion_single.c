/* ======================================================================
 * 一维热传导：两种边界条件下的铁棍温度演化 —— 单文件版（C89）
 *
 * 本文件由 make_single_source.py 从 include/ + src/ 自动合并生成，
 * 与模块化版本（正式交付）内容对应，只是并成一个翻译单元，
 * 方便只拷一个 .c 到别的机器上编译运行。
 *
 * 生成物，不要手改；改 src/ 再重跑本脚本。
 *
 * 编译：  gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 \
 *             heat_diffusion_single.c -o hd -lm
 * 运行：  ./hd [输出目录] [--verify-only]
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <float.h>


/* ---------- config.h ---------- */

/*
 * 题目给定的量，全部集中在这里。别的模块只许引用这些符号，
 * 不许再写死 0.1 / 0.01 / 100 这些数字。
 *
 * 一根各向同性铁棍，长 l = 0.1 m；中间长 0.01 m 的一段初温 100 摄氏度，
 * 其余 0 摄氏度。各向同性意味着热扩散率是一个标量 alpha，方程退化成
 * 一维的 du/dt = alpha * d2u/dx2。
 */

/* ---- 几何与初始温度 ---- */
#define ROD_LENGTH 0.1   /* 棍长 l，m */
#define HOT_WIDTH  0.01  /* 中间高温段长度，m */
#define HOT_TEMP   100.0 /* 高温段初温，摄氏度 */
#define COLD_TEMP  0.0   /* 其余部分初温，摄氏度 */

/* 高温段居中，于是左右端点是 (l -+ w)/2 = 0.045 / 0.055 */
#define HOT_LO ((ROD_LENGTH - HOT_WIDTH) / 2.0)
#define HOT_HI ((ROD_LENGTH + HOT_WIDTH) / 2.0)

/*
 * ---- 铁的热物性（室温附近）----
 * 题目只说"铁棍子"，没给 alpha，必须自己交代取值。
 * 采用手册表值 alpha = 23 mm^2/s = 2.3e-5 m^2/s；
 * 下面三个量用来交叉核对：k/(rho*cp) = 2.28e-5，与表值差约 1%。
 * 来源与核对见 README 第 2 节。
 */
#define IRON_ALPHA 2.3e-5 /* 采用的热扩散率，m^2/s */
#define IRON_K     80.2   /* 导热率，W/(m*K) */
#define IRON_RHO   7870.0 /* 密度，kg/m^3 */
#define IRON_CP    447.0  /* 比热容，J/(kg*K) */

/* ---- 离散参数（题目要求 (a)：Dx = 0.1 cm，自设 Dt）---- */
#define DEFAULT_DX 0.001 /* (a)(d) 用的细网格，m */
#define COARSE_DX  0.002 /* (e) 用的粗网格，m */
#define DEFAULT_DT 0.01  /* 自设的时间步长，s */

/* FTCS 显式格式的稳定性上限是 r = alpha*dt/dx^2 <= 1/2 */
#define FTCS_R_MAX 0.5

/* ---- 题目 (d)(e) 要求画曲线的时刻，s ---- */
#define SNAP_COUNT 3
#define SNAP_T1 10.0
#define SNAP_T2 20.0
#define SNAP_T3 30.0

/*
 * ---- 收敛条件（题目要求 (a)：自设）----
 * 主判据：与终态的最大偏差 max|u_i - u_inf| <= CONV_EPS。
 *   u_inf 由边界条件决定：两端钉 0 度 -> 0；周期边界 -> 初值平均值 10 度。
 * 副判据：单步最大变化 max|u_i^{n+1} - u_i^n| <= CONV_STEP_EPS。
 *   两个判据不等价，第 5 步会实测它们差多少。
 */
#define CONV_EPS      1e-6  /* 摄氏度 */
#define CONV_STEP_EPS 1e-9  /* 摄氏度/步 */
#define CONV_MAX_STEPS 20000000L /* 保险丝：不收敛就报错退出，不静默截断 */

/*
 * 时空演化文件的时间抽稀上限。完整 u(x_i,t_n) 是英文版要求的交付物，
 * 但 Dirichlet 收敛要七万多步，逐step存会写出几十 MB。这里按预测步数
 * 反推输出间隔，把切片数压在这个上限以内（早期变化快，仍然够用）。
 */
#define EXPORT_MAX_SLICES 800

/* 浮点比较用的几何容差：只用来判断"节点是否落在区间端点上" */
#define GEOM_TOL 1e-12

/* C89 不保证有 M_PI */
#define HD_PI 3.14159265358979323846


/* ---------- util.h ---------- */

/*
 * 跨模块共用的小工具。
 *
 * 之所以有这一个模块：`value_is_finite` 原先在 solver.c 和 crank.c 里
 * 各写了一份同名 static 函数。分开编译相安无事，但 make_single_source.py
 * 把两者并成一个翻译单元时就是重定义、直接编不过。单文件版不只是个演示
 * 品，它由 test/run.sh 逐字节比对模块版的输出，所以它同时也是
 * 「静态作用域有没有被滥用」的检查器。
 */

/*
 * C89 没有 isfinite / NAN（都是 C99 的）。用两条只依赖 IEEE 算术的判据：
 *   v != v        -> NaN
 *   v - v != 0    -> +-inf（inf - inf 是 NaN）；有限值必然满足 v-v == 0
 * 不开 -ffast-math，编译器不会把 v-v 优化成 0。
 */
int util_is_finite(double v);

/* 整个数组是否都有限 */
int util_all_finite(const double *v, long n);


/* ---------- material.h ---------- */

/*
 * 材料层：只负责把"铁"翻译成方程里的系数，以及由系数派生出
 * 离散参数必须遵守的界限。不涉及网格，也不涉及时间推进。
 */

/* 采用的热扩散率 alpha，m^2/s */
double material_alpha(void);

/* 由 k/(rho*cp) 独立算出的 alpha，用来交叉核对表值 */
double material_alpha_from_krc(void);

/* (表值 - k/rho/cp) / (k/rho/cp)，相对偏差 */
double material_alpha_rel_dev(void);

/* Fourier 数 r = alpha*dt/dx^2，显式格式的稳定性就看它 */
double material_fourier_number(double alpha, double dx, double dt);

/* FTCS 稳定的最大时间步长 dx^2/(2*alpha)，s */
double material_dt_max_ftcs(double alpha, double dx);

/*
 * 扩散时间尺度：最低阶模态衰减到 1/e 所需的时间。
 * Dirichlet（两端 0 度）最低模是 sin(pi x/l)，tau = l^2/(pi^2 alpha)；
 * 周期边界最低非零模是 cos(2 pi x/l)，tau = l^2/(4 pi^2 alpha)。
 * 周期边界的 tau 是 Dirichlet 的 1/4，所以它收敛快得多。
 */
double material_tau_dirichlet(double alpha, double length);
double material_tau_periodic(double alpha, double length);


/* ---------- grid.h ---------- */

/*
 * 网格与初始温度场。
 *
 * 节点 x_i = i*dx，i = 0..n，n = l/dx。周期边界下 x_n 与 x_0 是同一个
 * 物理点，数组里仍然存 n+1 个值方便画图，但守恒量只对 i = 0..n-1 求和。
 *
 * 初值用有限体积口径：节点 i 代表它自己的控制体，温度取初始分布在该
 * 控制体上的平均值。内部节点的控制体宽 dx，Dirichlet 两个端节点各宽
 * dx/2（半单元），周期边界每个节点都是整单元 dx。这样总热量
 * sum(u_i * w_i) 与积分 int u0 dx 严格相等，与 dx 取多少无关。
 */

typedef enum {
    BC_DIRICHLET = 0, /* 两端接触 0 度理想导热体 */
    BC_PERIODIC  = 1  /* 首尾相接 */
} BoundaryKind;

typedef struct {
    long n;        /* 单元数 */
    long nn;       /* 节点数 = n+1 */
    double dx;     /* 实际步长 = length/n，保证网格正好铺满 */
    double length;
    double *x;     /* 节点坐标，长度 nn */
    double *u;     /* 节点温度，长度 nn */
} Grid;

/* 成功返回 0；dx 非法或内存不足返回非 0。g 会被清零后填充。 */
int grid_init(Grid *g, double length, double dx);
void grid_free(Grid *g);

/* 节点 i 的控制体宽度：Dirichlet 端点是 dx/2，其余（含周期）是 dx */
double grid_cell_width(const Grid *g, long i, int periodic);

/*
 * 以 x=xc 为中心、宽 dx 的控制体与高温段 [hot_lo,hot_hi] 的重叠长度。
 * periodic 非 0 时把高温段按 +-length 平移后再求重叠，处理跨界单元。
 */
double grid_cell_overlap(double xc, double dx, double hot_lo, double hot_hi,
                         double length, int periodic);

/* 有限体积初值：u_i = cold + (hot-cold)*overlap_i/w_i */
void grid_initial_fv(Grid *g, double hot_lo, double hot_hi,
                     double hot_temp, double cold_temp, int periodic);

/*
 * 负向对照：直接把落在闭区间 [hot_lo,hot_hi] 里的节点设成 hot_temp。
 * 这是最容易想到的写法，也是错的——dx=0.1cm 时 0.045 和 0.055 恰好都是
 * 节点，闭区间里有 11 个节点却只有 10 个单元，总热量多算 10%。
 *
 * 端点判断必须带 GEOM_TOL，实测：HOT_LO=(0.1-0.01)/2 算出来是
 * 0.045000000000000005274，而 x[45]=45*(0.1/100) 是 0.044999999999999998335，
 * 不带容差时 x[45]>=HOT_LO 为假，于是只数到 10 个节点、热量碰巧等于 1.0，
 * 但左半单元被丢掉，初值不再关于中点对称。两种写法都错，错法不同。
 */
void grid_initial_naive(Grid *g, double hot_lo, double hot_hi,
                        double hot_temp, double cold_temp);

/* 总热量 int u dx：Dirichlet 用梯形权重（端点 dx/2） */
double grid_heat_dirichlet(const Grid *g);

/* 总热量：周期边界对 i=0..n-1 求和，每个节点权重 dx */
double grid_heat_periodic(const Grid *g);

/* 周期边界的平均温度 = 热量 / l，它是整个演化过程的守恒量 */
double grid_mean_periodic(const Grid *g);

/* 关于中点的对称性偏差 max|u_i - u_(n-i)|，初值应接近 0 */
double grid_symmetry_defect(const Grid *g);

/* 统计温度等于 value（容差 tol）的节点个数 */
long grid_count_equal(const Grid *g, double value, double tol);

/* 施加边界条件：Dirichlet 把两端钉成 value；周期把 u_n 同步成 u_0 */
void grid_apply_bc(Grid *g, BoundaryKind kind, double value);


/* ---------- solver.h ---------- */


/*
 * 显式 FTCS（forward time, centered space）推进器。
 *
 *   u_i^{n+1} = u_i^n + r (u_{i+1}^n - 2 u_i^n + u_{i-1}^n),
 *   r = alpha dt / dx^2
 *
 * 改写成凸组合就看清楚稳定性从哪来：
 *   u_i^{n+1} = r u_{i-1}^n + (1-2r) u_i^n + r u_{i+1}^n
 * 三个权重非负且和为 1 当且仅当 r <= 1/2，于是新值夹在旧值的
 * 最小最大之间——离散极值原理成立，格式稳定。r > 1/2 时中间权重
 * 变负，最高频模态的放大因子 1-4r 越过 -1，振荡发散。
 *
 * 推进器借用 Grid，不拥有它；自己只持有一块等长的 scratch，
 * 因为显式格式绝不能原地更新（右边的 u_{i-1} 会被本步的新值污染）。
 */
typedef struct {
    Grid *g;             /* 借用的网格与温度场 */
    BoundaryKind bc;
    double bc_value;     /* Dirichlet 两端钉住的温度 */
    double alpha;
    double dt;           /* 名义时间步长 */
    double r;            /* 名义 Fourier 数 alpha*dt/dx^2 */
    double r_last;       /* 最近一步实际用的 r（末步可能缩短） */
    double time;         /* 当前物理时刻 */
    long steps;          /* 已走步数 */
    double *work;        /* scratch，长度 g->nn */
} Ftcs;

/* 成功返回 0。dt<=0 或内存不足返回非 0。不做稳定性拦截——
   r>1/2 也要能跑，负向对照靠它。 */
int ftcs_init(Ftcs *s, Grid *g, BoundaryKind bc, double bc_value,
              double alpha, double dt);
void ftcs_free(Ftcs *s);

/* 走一个名义步长 dt */
void ftcs_step(Ftcs *s);

/*
 * 走到 target_time。剩余时间不足一个 dt 时，末步按剩余时间缩短
 * （r 变小，仍然稳定），这样任意 dt 都能精确落在 10/20/30 s 上。
 * 返回 0 正常；返回 1 表示中途出现非有限值（发散）。
 */
int ftcs_advance(Ftcs *s, double target_time);

/* 温度场里是否有 NaN / inf */
int ftcs_all_finite(const Ftcs *s);

/*
 * 波数为 q 的离散模态的放大因子。
 * 把 u_i = sin(q x_i) 或 cos(q x_i) 代入差分算子：
 *   u_{i+1} - 2u_i + u_{i-1} = -4 sin^2(q dx/2) u_i
 * 所以 G = 1 - 4 r sin^2(q dx / 2)。
 * Dirichlet 第 k 模 q = k pi / l；周期第 k 模 q = 2 k pi / l。
 * 对应的连续解每步衰减 exp(-alpha q^2 dt)，两者之差就是离散误差。
 */
double ftcs_growth_factor(double r, double q, double dx);


/* ---------- crank.h ---------- */


/*
 * Crank-Nicolson（隐式，二阶时间）。把方程写成
 *   (I - (r/2) L) u^{n+1} = (I + (r/2) L) u^n,   r = alpha dt / dx^2
 * 左右两边在 t_{n+1/2} 处都是二阶精度，所以整个格式是 O(dt^2) + O(dx^2)。
 *
 * 左端矩阵是三对角的：
 *   对角 1+r，上下对角 -r/2，周期边界再加两个角元 -r/2。
 * 它严格对角占优（|1+r| > r），Thomas 追赶法不需要选主元就稳定。
 *
 * 周期边界的矩阵是"循环三对角"，用 Sherman-Morrison 降回三对角：
 *   取 u = [alpha, 0,...,0, beta]^T, v = [1, 0,...,0, 1]^T，
 *   A' = A - u v^T 就把两个角元抹掉了，代价是对角首尾各减去 alpha / beta
 *   （我们这里 alpha = beta = -r/2，于是首尾对角变成 1 + 1.5r，更占优）。
 *   解两次 A'（一次右端是 rhs，一次右端是 u），再按
 *     x = A'^{-1}b - (v . A'^{-1}b)/(1 + v . A'^{-1}u) * A'^{-1}u
 *   合成。这是 Numerical Recipes 的 cyclic 做法，O(m) 复杂度。
 *
 * 稳定性：第 k 模的放大因子
 *   G = (1 - 2 r s^2) / (1 + 2 r s^2),  s = sin(q dx / 2)
 * 对任何 r 都满足 |G| <= 1，所以无条件稳定。但注意 r -> inf 时最高频模
 * 的 G -> -1：它不衰减，只是每步翻一次符号。也就是说 CN 无条件稳定，
 * 却无条件"不单调"——大步长会留下不衰减的锯齿。这是 CN 的真实代价，
 * 第 4 步会把它量出来。
 */
typedef struct {
    Grid *g;             /* 借用 */
    BoundaryKind bc;
    double bc_value;
    double alpha;
    double dt;
    double r;            /* 名义 Fourier 数 */
    double r_last;
    double time;
    long steps;
    long m;              /* 未知数个数：Dirichlet = n-1，周期 = n */
    double *diag;        /* 长度 m */
    double *sub;         /* 长度 m，sub[0] 不用 */
    double *sup;         /* 长度 m，sup[m-1] 不用 */
    double *rhs;         /* 长度 m */
    double *x;           /* 长度 m */
    double *cp;          /* Thomas 工作区 */
    double *dp;          /* Thomas 工作区 */
    double *dg;          /* Sherman-Morrison 修正后的对角 */
    double *uvec;        /* SM 的 u */
    double *w;           /* A'^{-1} rhs */
    double *y;           /* A'^{-1} u */
} Crank;

int crank_init(Crank *s, Grid *g, BoundaryKind bc, double bc_value,
               double alpha, double dt);
void crank_free(Crank *s);

void crank_step(Crank *s);
int crank_advance(Crank *s, double target_time);
int crank_all_finite(const Crank *s);

/* CN 的离散模态放大因子 G = (1-2r s^2)/(1+2r s^2) */
double crank_growth_factor(double r, double q, double dx);


/* ---------- analytic.h ---------- */

/*
 * 分离变量法给出的解析解，作为数值解的标尺。
 *
 * 初始条件是方波 u0(x) = 100 于 [l/2-w/2, l/2+w/2]，其余 0。
 * 它在区间端点有跳跃，所以 Fourier 系数按 1/k 衰减——这既是
 * 第 0 节 Gibbs 现象的来源，也意味着 t=0 时级数收敛得很慢。
 * 只要 t>0，exp(-alpha q_k^2 t) 就把高频压死，项数需求骤降。
 *
 * (1) Dirichlet：两端钉 0 度，本征函数 sin(k pi x/l)
 *       u(x,t) = sum_k b_k sin(k pi x/l) exp(-alpha (k pi/l)^2 t)
 *       b_k = (2/l) int_0^l u0 sin(k pi x/l) dx
 *           = (400/(k pi)) sin(k pi/2) sin(k pi w/(2l))
 *     偶数项恒为 0（初值关于中点对称，sin 关于中点是反对称的）。
 *     终态 u_inf = 0，热量从两端漏光。
 *
 * (2) 周期：本征函数 {1, cos(2k pi x/l), sin(2k pi x/l)}
 *       u(x,t) = 10 + sum_k A_k cos(2k pi x/l) exp(-alpha (2k pi/l)^2 t)
 *       A_k = (200/(k pi)) (-1)^k sin(k pi w/l)
 *     正弦系数全为 0（初值关于 x=l/2 对称，sin 项关于它是反对称的）。
 *     零模 = 平均值 = 100*w/l = 10 度，它不随时间变——这就是终态。
 *
 * 两套系数都不许写死数字，全部由 config.h 的 l / w / 100 / 0 推出来。
 */

/* 第 k 模的波数：Dirichlet 取 k*pi/l，周期取 2*k*pi/l */
double analytic_wavenumber(long k, int periodic);

/* 衰减率 alpha * q_k^2，单位 1/s；它的倒数就是该模的衰减时间 */
double analytic_decay_rate(long k, int periodic);

/* Fourier 系数（解析式） */
double analytic_b_dirichlet(long k);
double analytic_a_periodic(long k);

/* 周期边界的零模，即平均温度 = (HOT_TEMP - COLD_TEMP) * HOT_WIDTH / l + COLD_TEMP */
double analytic_mean_periodic(void);

/*
 * 级数求值。nmax <= 0 表示按 tol 自动定项数。
 * 返回该时刻该位置的温度（摄氏度）。
 */
double analytic_u(double x, double t, long nmax, int periodic);

/*
 * 自动定项数：用 |系数| 的上界乘时间因子，找出仍然大于 tol 的最大 k，
 * 再加余量。t<=0 时指数因子不起作用，级数只按 1/k 收敛，此时
 * 返回一个上限项数并由调用方自行说明收敛性（见 Gibbs 检查）。
 */
long analytic_nmax(double t, double tol, int periodic);

/*
 * 独立核对用的数值投影：对 u0 的支撑区间 [HOT_LO, HOT_HI] 做复合 Simpson
 * 积分算 (2/l) int u0 * 本征函数 dx。被积函数在这个区间里是光滑的正弦/
 * 余弦，Simpson 能达到很高精度，正好用来验上面那两个解析系数式。
 * npts 必须是偶数。
 */
double analytic_project_dirichlet(long k, long npts);
double analytic_project_periodic(long k, long npts);


/* ---------- converge.h ---------- */


/*
 * 收敛判据。题目要求"自设 Dt 以及收敛条件"，这里给两条，并且实测它们的差别
 * ——因为第二条是很多人会误用的那一条。
 *
 * 主判据（采用）：max_i |u_i - u_inf| <= eps
 *   u_inf 由边界条件决定，不是猜的：
 *     Dirichlet 两端钉 0 度，热量从两端漏光 -> u_inf = 0；
 *     周期边界热量无处可去，平均值守恒 -> u_inf = 初值平均（10 度）。
 *   它直接回答"离物理终态还差多少"，与 dt 无关。
 *
 * 副判据（对照）：max_i |u_i^{n+1} - u_i^n| <= eps_step
 *   这是"逐 step 变化量"判据，回答的是"还动不动"，不是"到没到"。
 *   单指数衰减 u - u_inf ~ A exp(-t/tau) 时，单步变化量约等于
 *      (u - u_inf) * dt / tau
 *   于是副判据在残差 eps_step * tau / dt 处就停手，比主判据松 tau/dt 倍。
 *   本例 tau_Dirichlet / dt = 44.05 / 0.01 = 4405，差三个数量级。
 *   衰减越慢（tau 越大）这个漏洞越严重，而铁棍恰好就是慢的那一类。
 */
typedef struct {
    double target;      /* u_inf */
    double eps;         /* 主判据阈值，摄氏度 */
    double max_dev;     /* 当前 max|u - u_inf| */
    double max_jump;    /* 最近一步的 max|u^{n+1} - u^n| */
    long steps;         /* 已观察的步数 */
    int converged;      /* 主判据是否已满足 */
} Converge;

/* 终态温度：Dirichlet 取边界值 0，周期取网格平均（守恒量） */
double converge_target(BoundaryKind bc, const Grid *g);

/* 场对终态的最大偏离 / 相邻两步的最大变化 */
double converge_max_dev(const double *u, long nn, double target);
double converge_max_jump(const double *u_prev, const double *u_now, long nn);

void converge_init(Converge *c, double target, double eps);

/* 每走一步调用一次；steps 自增，并判断主判据 */
void converge_observe(Converge *c, const double *u_prev, const double *u_now,
                      long nn);

/*
 * 主判据达标时刻的单指数预测：t = tau * ln(amp / eps)。
 * amp 是最低阶模的初始幅度（Dirichlet 用 |b_1|，周期用 |A_1|），
 * tau 用对应的最低阶模衰减时间。多模解在后期被最低阶模主导，
 * 所以这个预测应当和实测接近——差多少本身就是个检查。
 */
double converge_predict_time(double alpha, double length, double amp,
                             int periodic, double eps);

/*
 * 从逐 step 变化量的历史里，找"从这一步起永久低于阈值"的第一个步数。
 * 直接取首次穿越是不行的：初期前沿还在陡降，max_jump 会先冲高再回落，
 * 中途可能短暂跌破阈值又弹回去。找不到返回 -1。
 */
long converge_first_permanent(const double *hist, long n, double threshold);


/* ---------- export.h ---------- */



/*
 * 数据导出。所有数字都用 %.17g 写，读回来必须和内存里逐位相同，
 * 这样第 6 步的"代入式核对"才是在核对真实交付的数，而不是另一份近似值。
 */

/*
 * 剖面文件：一行一个节点，列依次是
 *   x  u(0)  u(10s)  u(20s)  u(30s)  u(收敛后)
 * 这正是题目 (d)(e) 要画的四条曲线加初值。meta 是写进注释头的说明行。
 */
int export_profiles(const char *outdir, const char *name, const Grid *g,
                    const double *u0, const double *u10, const double *u20,
                    const double *u30, const double *ucv,
                    double target, double eps, double t_conv, int periodic,
                    const char *meta);

/*
 * 相隔正好一个 dt 的两条剖面。注释头带上 length/dx/n/nn/dt/alpha/r/periodic/t0，
 * 让核对程序只读这一个文件就能把差分方程
 *   u^{n+1}_i - u^n_i = r (u^n_{i+1} - 2 u^n_i + u^n_{i-1})
 * 的残差代出来，不用去猜参数。
 */
int export_stepdt(const char *outdir, const char *name, const Grid *g,
                  const double *ua, const double *ub, double t0,
                  double dt, double alpha, int periodic, const char *meta);

/* 演化文件的流式写入器：一行一个时刻，先写 t 再写整条温度剖面 */
typedef struct {
    FILE *fp;
    long nn;
    long nslice;
    char path[512];
} EvoWriter;

int evo_open(EvoWriter *w, const char *outdir, const char *name,
             const Grid *g, const char *meta);
int evo_write(EvoWriter *w, double t, const double *u);
int evo_close(EvoWriter *w);

/*
 * 判据扫描文件：列依次是
 *   eps_step  停手步数  停手时刻  停手时的残差 max|u-u_inf|  残差/eps
 * 用来把"副判据比主判据松多少"这件事交给读者复核。
 */
int export_criterion(const char *outdir, const char *name, const char *meta,
                     const double *eps_step, const long *stop_step,
                     const double *stop_time, const double *dev_at_stop,
                     long nrows, double eps_main, double tau, double dt);

/*
 * 通用多列导出：第一列固定是 x，后面跟 ncols 个剖面。
 * 附图（稳定性对照、CN 过冲、误差分解）用它写，避免每种图开一个函数。
 */
int export_columns(const char *outdir, const char *name, const char *meta,
                   const double *x, const double *const *cols, long ncols,
                   const char *const *col_heads, long nn);

/* 汇总：每个 (dx, 边界) 组合的关键数字，报告表格直接引它 */
int export_summary(const char *outdir, const char *name,
                   const double *rows, long ncols, long nrows,
                   const char *const *heads, const char *meta);


/* ---------- verify.h ---------- */

/*
 * 代入式核对层：把已经写进 data 目录下的 .dat 文件 的数字读回来，代进各自的定义式，
 * 看等式成不成立。这是最强的一层证据——它核对的就是报告里印出来的那批数，
 * 任何人拿计算器都能照同一批数复算，不依赖第二份实现。
 *
 * 返回失败项数；调用方据此决定退出码。
 */
long verify_run(const char *outdir);

/* 核对层跑过的总项数（verify_run 之后才有意义） */
long verify_checks_run(void);


/* ========== util.c ========== */

int util_is_finite(double v)
{
    if (v != v) {
        return 0;
    }
    if (v - v != 0.0) {
        return 0;
    }
    return 1;
}

int util_all_finite(const double *v, long n)
{
    long i;

    for (i = 0; i < n; i++) {
        if (!util_is_finite(v[i])) {
            return 0;
        }
    }
    return 1;
}


/* ========== material.c ========== */

double material_alpha(void)
{
    return IRON_ALPHA;
}

double material_alpha_from_krc(void)
{
    return IRON_K / (IRON_RHO * IRON_CP);
}

double material_alpha_rel_dev(void)
{
    double ref;

    ref = material_alpha_from_krc();
    return (material_alpha() - ref) / ref;
}

double material_fourier_number(double alpha, double dx, double dt)
{
    return alpha * dt / (dx * dx);
}

double material_dt_max_ftcs(double alpha, double dx)
{
    return dx * dx / (2.0 * alpha);
}

double material_tau_dirichlet(double alpha, double length)
{
    return length * length / (HD_PI * HD_PI * alpha);
}

double material_tau_periodic(double alpha, double length)
{
    return length * length / (4.0 * HD_PI * HD_PI * alpha);
}


/* ========== grid.c ========== */


static double overlap_of(double a_lo, double a_hi, double b_lo, double b_hi)
{
    double lo;
    double hi;

    lo = (a_lo > b_lo) ? a_lo : b_lo;
    hi = (a_hi < b_hi) ? a_hi : b_hi;
    return (hi > lo) ? hi - lo : 0.0;
}

int grid_init(Grid *g, double length, double dx)
{
    long n;
    long i;

    g->n = 0;
    g->nn = 0;
    g->dx = 0.0;
    g->length = 0.0;
    g->x = NULL;
    g->u = NULL;

    if (length <= 0.0 || dx <= 0.0 || dx > length) {
        return 1;
    }

    n = (long)(length / dx + 0.5);
    if (n < 2) {
        return 2;
    }

    g->x = (double *)malloc((size_t)(n + 1) * sizeof(double));
    g->u = (double *)malloc((size_t)(n + 1) * sizeof(double));
    if (g->x == NULL || g->u == NULL) {
        grid_free(g);
        return 3;
    }

    g->n = n;
    g->nn = n + 1;
    g->length = length;
    g->dx = length / (double)n; /* 用实际步长，避免 l/dx 不整除时铺不满 */

    for (i = 0; i <= n; i++) {
        g->x[i] = (double)i * g->dx;
        g->u[i] = 0.0;
    }
    g->x[n] = length; /* 端点取精确值，不让累加误差漂出去 */
    return 0;
}

void grid_free(Grid *g)
{
    if (g->x != NULL) {
        free(g->x);
        g->x = NULL;
    }
    if (g->u != NULL) {
        free(g->u);
        g->u = NULL;
    }
    g->n = 0;
    g->nn = 0;
}

double grid_cell_width(const Grid *g, long i, int periodic)
{
    if (!periodic && (i == 0 || i == g->n)) {
        return g->dx / 2.0;
    }
    return g->dx;
}

double grid_cell_overlap(double xc, double dx, double hot_lo, double hot_hi,
                         double length, int periodic)
{
    double c_lo;
    double c_hi;
    double sum;
    int shift;

    c_lo = xc - dx / 2.0;
    c_hi = xc + dx / 2.0;

    if (!periodic) {
        if (c_lo < 0.0) {
            c_lo = 0.0;
        }
        if (c_hi > length) {
            c_hi = length;
        }
        return overlap_of(c_lo, c_hi, hot_lo, hot_hi);
    }

    /* 周期：高温段在圆上等价于它自己加上 +-length 的平移 */
    sum = 0.0;
    for (shift = -1; shift <= 1; shift++) {
        sum += overlap_of(c_lo, c_hi,
                          hot_lo + (double)shift * length,
                          hot_hi + (double)shift * length);
    }
    return sum;
}

void grid_initial_fv(Grid *g, double hot_lo, double hot_hi,
                     double hot_temp, double cold_temp, int periodic)
{
    long i;
    double w;
    double ov;

    for (i = 0; i < g->nn; i++) {
        w = grid_cell_width(g, i, periodic);
        ov = grid_cell_overlap(g->x[i], g->dx, hot_lo, hot_hi,
                               g->length, periodic);
        if (ov > w) { /* 浮点保护：重叠不可能超过控制体本身 */
            ov = w;
        }
        g->u[i] = cold_temp + (hot_temp - cold_temp) * (ov / w);
    }
    if (periodic) {
        g->u[g->n] = g->u[0];
    }
}

void grid_initial_naive(Grid *g, double hot_lo, double hot_hi,
                        double hot_temp, double cold_temp)
{
    long i;

    for (i = 0; i < g->nn; i++) {
        if (g->x[i] >= hot_lo - GEOM_TOL && g->x[i] <= hot_hi + GEOM_TOL) {
            g->u[i] = hot_temp;
        } else {
            g->u[i] = cold_temp;
        }
    }
}

double grid_heat_dirichlet(const Grid *g)
{
    long i;
    double sum;

    sum = 0.0;
    for (i = 0; i < g->nn; i++) {
        sum += g->u[i] * grid_cell_width(g, i, 0);
    }
    return sum;
}

double grid_heat_periodic(const Grid *g)
{
    long i;
    double sum;

    sum = 0.0;
    for (i = 0; i < g->n; i++) { /* 不含 i=n，它与 i=0 重复 */
        sum += g->u[i] * g->dx;
    }
    return sum;
}

double grid_mean_periodic(const Grid *g)
{
    return grid_heat_periodic(g) / g->length;
}

double grid_symmetry_defect(const Grid *g)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < g->nn; i++) {
        d = fabs(g->u[i] - g->u[g->n - i]);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

long grid_count_equal(const Grid *g, double value, double tol)
{
    long i;
    long count;

    count = 0;
    for (i = 0; i < g->nn; i++) {
        if (fabs(g->u[i] - value) <= tol) {
            count++;
        }
    }
    return count;
}

void grid_apply_bc(Grid *g, BoundaryKind kind, double value)
{
    if (kind == BC_DIRICHLET) {
        g->u[0] = value;
        g->u[g->n] = value;
    } else {
        g->u[g->n] = g->u[0];
    }
}


/* ========== solver.c ========== */


/* 时刻比较的相对容差：吸收 3000 次 time += dt 的累加误差 */
#define TIME_EPS_REL 1e-12

int ftcs_init(Ftcs *s, Grid *g, BoundaryKind bc, double bc_value,
              double alpha, double dt)
{
    s->g = g;
    s->bc = bc;
    s->bc_value = bc_value;
    s->alpha = alpha;
    s->dt = dt;
    s->r = 0.0;
    s->r_last = 0.0;
    s->time = 0.0;
    s->steps = 0;
    s->work = NULL;

    if (g == NULL || g->u == NULL || dt <= 0.0 || alpha < 0.0) {
        return 1;
    }
    s->work = (double *)malloc((size_t)g->nn * sizeof(double));
    if (s->work == NULL) {
        return 2;
    }
    s->r = alpha * dt / (g->dx * g->dx);
    s->r_last = s->r;
    grid_apply_bc(g, bc, bc_value);
    return 0;
}

void ftcs_free(Ftcs *s)
{
    if (s->work != NULL) {
        free(s->work);
        s->work = NULL;
    }
}

/* 用指定步长走一步；r 随步长重算，末步缩短时依然满足稳定性 */
static void ftcs_step_dt(Ftcs *s, double dt)
{
    Grid *g;
    double *u;
    double *w;
    double r;
    long i;
    long im;
    long ip;
    long n;

    g = s->g;
    u = g->u;
    w = s->work;
    n = g->n;
    r = s->alpha * dt / (g->dx * g->dx);
    s->r_last = r;

    if (s->bc == BC_DIRICHLET) {
        w[0] = s->bc_value;
        w[n] = s->bc_value;
        for (i = 1; i < n; i++) {
            w[i] = u[i] + r * (u[i + 1] - 2.0 * u[i] + u[i - 1]);
        }
    } else {
        /* 周期：只有 i=0..n-1 是独立未知量，i=n 与 i=0 是同一点 */
        for (i = 0; i < n; i++) {
            im = (i == 0) ? n - 1 : i - 1;
            ip = (i == n - 1) ? 0 : i + 1;
            w[i] = u[i] + r * (u[ip] - 2.0 * u[i] + u[im]);
        }
        w[n] = w[0];
    }

    memcpy(u, w, (size_t)g->nn * sizeof(double));
    s->time += dt;
    s->steps++;
}

void ftcs_step(Ftcs *s)
{
    ftcs_step_dt(s, s->dt);
}

int ftcs_all_finite(const Ftcs *s)
{
    return util_all_finite(s->g->u, s->g->nn);
}

int ftcs_advance(Ftcs *s, double target_time)
{
    double remain;
    double eps;
    double scale;

    if (target_time < s->time) {
        return 2; /* 不许倒着走 */
    }
    scale = (target_time > 1.0) ? target_time : 1.0;
    eps = TIME_EPS_REL * scale;

    while (target_time - s->time > eps) {
        remain = target_time - s->time;
        if (remain < s->dt) {
            ftcs_step_dt(s, remain);
        } else {
            ftcs_step_dt(s, s->dt);
        }
        if (!ftcs_all_finite(s)) {
            return 1;
        }
        if (s->steps >= CONV_MAX_STEPS) {
            return 3;
        }
    }
    s->time = target_time; /* 吸掉累加误差，让快照时刻精确 */
    return 0;
}

double ftcs_growth_factor(double r, double q, double dx)
{
    double s;

    s = sin(q * dx / 2.0);
    return 1.0 - 4.0 * r * s * s;
}


/* ========== crank.c ========== */


#define TIME_EPS_REL 1e-12

/*
 * 三对角方程组 a*sub, d*diag, c*sup 的 Thomas 解法。
 * sub[0] 与 sup[m-1] 不参与运算。矩阵严格对角占优，无需选主元。
 * cp / dp 是长度为 m 的工作区。
 */
static void thomas(const double *sub, const double *diag, const double *sup,
                   const double *d, long m, double *x,
                   double *cp, double *dp)
{
    double den;
    long i;

    if (m <= 0) {
        return;
    }
    cp[0] = sup[0] / diag[0];
    dp[0] = d[0] / diag[0];
    for (i = 1; i < m; i++) {
        den = diag[i] - sub[i] * cp[i - 1];
        cp[i] = (i < m - 1) ? sup[i] / den : 0.0;
        dp[i] = (d[i] - sub[i] * dp[i - 1]) / den;
    }
    x[m - 1] = dp[m - 1];
    for (i = m - 2; i >= 0; i--) {
        x[i] = dp[i] - cp[i] * x[i + 1];
    }
}

/*
 * 循环三对角求解：Sherman-Morrison 把两个角元吸收掉。
 * 角元取 s->sup[m-1] 与 s->sub[0] 存放（这两个位置在纯三对角里本来不用，
 * 正好借来存角元，避免再开数组）。
 */
static void cyclic_solve(Crank *s, const double *d, double *x)
{
    long m;
    long i;
    double al;
    double be;
    double vb;
    double uy;
    double k;

    m = s->m;
    al = s->sup[m - 1]; /* A[0][m-1] */
    be = s->sub[0];     /* A[m-1][0] */

    memcpy(s->dg, s->diag, (size_t)m * sizeof(double));
    s->dg[0] -= al;
    s->dg[m - 1] -= be;

    /* u = [al, 0, ..., 0, be] */
    memset(s->uvec, 0, (size_t)m * sizeof(double));
    s->uvec[0] = al;
    s->uvec[m - 1] = be;

    /* 纯三对角部分：sub[0] / sup[m-1] 必须置零才不会被 thomas 误用 */
    s->sub[0] = 0.0;
    s->sup[m - 1] = 0.0;

    thomas(s->sub, s->dg, s->sup, d, m, s->w, s->cp, s->dp);
    thomas(s->sub, s->dg, s->sup, s->uvec, m, s->y, s->cp, s->dp);

    vb = s->w[0] + s->w[m - 1];
    uy = s->y[0] + s->y[m - 1];
    k = vb / (1.0 + uy);
    for (i = 0; i < m; i++) {
        x[i] = s->w[i] - k * s->y[i];
    }

    /* 把角元放回去，下一步还要用 */
    s->sub[0] = be;
    s->sup[m - 1] = al;
}

int crank_init(Crank *s, Grid *g, BoundaryKind bc, double bc_value,
               double alpha, double dt)
{
    long m;
    size_t sz;

    memset(s, 0, sizeof(Crank));
    s->g = g;
    s->bc = bc;
    s->bc_value = bc_value;
    s->alpha = alpha;
    s->dt = dt;

    if (g == NULL || g->u == NULL || dt <= 0.0 || alpha < 0.0) {
        return 1;
    }
    if (bc == BC_DIRICHLET) {
        m = g->n - 1; /* 未知量 i = 1..n-1 */
    } else {
        m = g->n;     /* 未知量 i = 0..n-1，i=n 与 i=0 同一点 */
    }
    if (m < 1) {
        return 2;
    }
    s->m = m;

    sz = (size_t)m * sizeof(double);
    s->diag = (double *)malloc(sz);
    s->sub = (double *)malloc(sz);
    s->sup = (double *)malloc(sz);
    s->rhs = (double *)malloc(sz);
    s->x = (double *)malloc(sz);
    s->cp = (double *)malloc(sz);
    s->dp = (double *)malloc(sz);
    s->dg = (double *)malloc(sz);
    s->uvec = (double *)malloc(sz);
    s->w = (double *)malloc(sz);
    s->y = (double *)malloc(sz);
    if (s->diag == NULL || s->sub == NULL || s->sup == NULL
        || s->rhs == NULL || s->x == NULL || s->cp == NULL || s->dp == NULL
        || s->dg == NULL || s->uvec == NULL || s->w == NULL || s->y == NULL) {
        crank_free(s);
        return 3;
    }

    s->r = alpha * dt / (g->dx * g->dx);
    s->r_last = s->r;
    grid_apply_bc(g, bc, bc_value);
    return 0;
}

void crank_free(Crank *s)
{
    double *p;

    p = s->diag;      s->diag = NULL;      free(p);
    p = s->sub;       s->sub = NULL;       free(p);
    p = s->sup;       s->sup = NULL;       free(p);
    p = s->rhs;       s->rhs = NULL;       free(p);
    p = s->x;         s->x = NULL;         free(p);
    p = s->cp;        s->cp = NULL;        free(p);
    p = s->dp;        s->dp = NULL;        free(p);
    p = s->dg;        s->dg = NULL;        free(p);
    p = s->uvec;      s->uvec = NULL;      free(p);
    p = s->w;         s->w = NULL;         free(p);
    p = s->y;         s->y = NULL;         free(p);
    s->m = 0;
}

/* 按步长 dt 组装矩阵与右端并求解 */
static void crank_step_dt(Crank *s, double dt)
{
    Grid *g;
    double *u;
    double r;
    double half;
    long n;
    long m;
    long i;
    long p;

    g = s->g;
    u = g->u;
    n = g->n;
    m = s->m;
    r = s->alpha * dt / (g->dx * g->dx);
    half = 0.5 * r;
    s->r_last = r;

    for (i = 0; i < m; i++) {
        s->diag[i] = 1.0 + r;
        s->sub[i] = -half;
        s->sup[i] = -half;
    }

    if (s->bc == BC_DIRICHLET) {
        /* 未知量 u[1..n-1]；u[0]、u[n] 是已知边界值，只进右端 */
        for (p = 0; p < m; p++) {
            i = p + 1;
            s->rhs[p] = half * u[i - 1] + (1.0 - r) * u[i] + half * u[i + 1];
        }
        thomas(s->sub, s->diag, s->sup, s->rhs, m, s->x, s->cp, s->dp);
        for (p = 0; p < m; p++) {
            u[p + 1] = s->x[p];
        }
        u[0] = s->bc_value;
        u[n] = s->bc_value;
    } else {
        /* 周期：i = 0..n-1，邻点按模 n 回绕 */
        for (p = 0; p < m; p++) {
            i = p;
            s->rhs[p] = half * u[(i == 0 ? n - 1 : i - 1)]
                        + (1.0 - r) * u[i]
                        + half * u[(i == n - 1 ? 0 : i + 1)];
        }
        /* 角元存进本来不用的位置，cyclic_solve 会取出来 */
        s->sup[m - 1] = -half;
        s->sub[0] = -half;
        cyclic_solve(s, s->rhs, s->x);
        for (p = 0; p < m; p++) {
            u[p] = s->x[p];
        }
        u[n] = u[0];
    }

    s->time += dt;
    s->steps++;
}

void crank_step(Crank *s)
{
    crank_step_dt(s, s->dt);
}

int crank_all_finite(const Crank *s)
{
    return util_all_finite(s->g->u, s->g->nn);
}

int crank_advance(Crank *s, double target_time)
{
    double remain;
    double eps;
    double scale;

    if (target_time < s->time) {
        return 2;
    }
    scale = (target_time > 1.0) ? target_time : 1.0;
    eps = TIME_EPS_REL * scale;

    while (target_time - s->time > eps) {
        remain = target_time - s->time;
        if (remain < s->dt) {
            crank_step_dt(s, remain);
        } else {
            crank_step_dt(s, s->dt);
        }
        if (!crank_all_finite(s)) {
            return 1;
        }
        if (s->steps >= CONV_MAX_STEPS) {
            return 3;
        }
    }
    s->time = target_time;
    return 0;
}

double crank_growth_factor(double r, double q, double dx)
{
    double s;

    s = sin(q * dx / 2.0);
    s *= s;
    return (1.0 - 2.0 * r * s) / (1.0 + 2.0 * r * s);
}


/* ========== analytic.c ========== */


/* 级数项数硬上限：t=0 时按 1/k 收敛，靠这个上限兜住 */
#define ANALYTIC_KMAX 200000L

double analytic_wavenumber(long k, int periodic)
{
    double q;

    q = HD_PI * (double)k / ROD_LENGTH;
    return periodic ? 2.0 * q : q;
}

double analytic_decay_rate(long k, int periodic)
{
    double q;

    q = analytic_wavenumber(k, periodic);
    return IRON_ALPHA * q * q;
}

double analytic_b_dirichlet(long k)
{
    double q;

    q = analytic_wavenumber(k, 0); /* k pi / l */
    /* (2/l) * HOT_TEMP * int_lo^hi sin(q x) dx
       = (2 HOT_TEMP/(l q)) [cos(q lo) - cos(q hi)] */
    return (2.0 * HOT_TEMP / (ROD_LENGTH * q))
           * (cos(q * HOT_LO) - cos(q * HOT_HI));
}

double analytic_a_periodic(long k)
{
    double q;

    q = analytic_wavenumber(k, 1); /* 2 k pi / l，不是 k pi / l */
    /* (2/l) * HOT_TEMP * int_lo^hi cos(q x) dx
       = (2 HOT_TEMP/(l q)) [sin(q hi) - sin(q lo)]
       系数与波数必须用同一个 q，写错任何一个都会让偶数 k 恰好抵消、
       读数看起来只是"错位一格"。 */
    return (2.0 * HOT_TEMP / (ROD_LENGTH * q))
           * (sin(q * HOT_HI) - sin(q * HOT_LO));
}

double analytic_mean_periodic(void)
{
    return ((HOT_TEMP - COLD_TEMP) * HOT_WIDTH + COLD_TEMP * ROD_LENGTH)
           / ROD_LENGTH;
}

long analytic_nmax(double t, double tol, int periodic)
{
    double rate1;
    double amp;
    double term;
    double bound;
    long k;
    long last;

    if (t <= 0.0) {
        return ANALYTIC_KMAX; /* 没有指数压制，只能给上限 */
    }
    rate1 = analytic_decay_rate(1, periodic);
    bound = 2.0 * HOT_TEMP / HD_PI; /* |系数| <= 2*HOT_TEMP/(k*pi) */

    last = 1;
    for (k = 1; k <= ANALYTIC_KMAX; k++) {
        amp = bound / (double)k;
        term = amp * exp(-rate1 * (double)k * (double)k * t);
        if (term >= tol) {
            last = k;
        } else if (rate1 * (double)k * (double)k * t > 100.0) {
            break; /* 指数已经彻底压死，后面不会再翻上来 */
        }
    }
    if (last + 2 < ANALYTIC_KMAX) {
        last += 2; /* 余量 */
    }
    return last;
}

double analytic_u(double x, double t, long nmax, int periodic)
{
    double sum;
    double q;
    long k;

    if (nmax <= 0) {
        nmax = analytic_nmax(t, 1e-12, periodic);
    }
    if (nmax > ANALYTIC_KMAX) {
        nmax = ANALYTIC_KMAX;
    }

    sum = periodic ? analytic_mean_periodic() : 0.0;
    for (k = 1; k <= nmax; k++) {
        q = analytic_wavenumber(k, periodic);
        if (periodic) {
            sum += analytic_a_periodic(k) * cos(q * x)
                   * exp(-q * q * IRON_ALPHA * t);
        } else {
            sum += analytic_b_dirichlet(k) * sin(q * x)
                   * exp(-q * q * IRON_ALPHA * t);
        }
    }
    return sum;
}

double analytic_project_dirichlet(long k, long npts)
{
    double h;
    double s;
    double q;
    long j;

    q = analytic_wavenumber(k, 0);
    h = (HOT_HI - HOT_LO) / (double)npts;
    s = sin(q * HOT_LO) + sin(q * HOT_HI);
    for (j = 1; j < npts; j++) {
        s += ((j % 2) == 0 ? 2.0 : 4.0) * sin(q * (HOT_LO + (double)j * h));
    }
    s *= h / 3.0;
    return (2.0 / ROD_LENGTH) * HOT_TEMP * s;
}

double analytic_project_periodic(long k, long npts)
{
    double h;
    double s;
    double q;
    long j;

    q = analytic_wavenumber(k, 1);
    h = (HOT_HI - HOT_LO) / (double)npts;
    s = cos(q * HOT_LO) + cos(q * HOT_HI);
    for (j = 1; j < npts; j++) {
        s += ((j % 2) == 0 ? 2.0 : 4.0) * cos(q * (HOT_LO + (double)j * h));
    }
    s *= h / 3.0;
    return (2.0 / ROD_LENGTH) * HOT_TEMP * s;
}


/* ========== converge.c ========== */


double converge_target(BoundaryKind bc, const Grid *g)
{
    if (bc == BC_DIRICHLET) {
        return COLD_TEMP; /* 两端钉 0 度，最终处处 0 */
    }
    return grid_mean_periodic(g); /* 守恒的平均值，就是终态 */
}

double converge_max_dev(const double *u, long nn, double target)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(u[i] - target);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

double converge_max_jump(const double *u_prev, const double *u_now, long nn)
{
    long i;
    double d;
    double worst;

    worst = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(u_now[i] - u_prev[i]);
        if (d > worst) {
            worst = d;
        }
    }
    return worst;
}

void converge_init(Converge *c, double target, double eps)
{
    c->target = target;
    c->eps = eps;
    c->max_dev = 0.0;
    c->max_jump = 0.0;
    c->steps = 0;
    c->converged = 0;
}

void converge_observe(Converge *c, const double *u_prev, const double *u_now,
                      long nn)
{
    c->max_dev = converge_max_dev(u_now, nn, c->target);
    c->max_jump = converge_max_jump(u_prev, u_now, nn);
    c->steps++;
    if (c->max_dev <= c->eps) {
        c->converged = 1;
    }
}

double converge_predict_time(double alpha, double length, double amp,
                             int periodic, double eps)
{
    double tau;

    if (amp <= eps || alpha <= 0.0) {
        return 0.0;
    }
    tau = periodic ? material_tau_periodic(alpha, length)
                   : material_tau_dirichlet(alpha, length);
    return tau * log(amp / eps);
}

/*
 * hist[i] 是"第 i+1 步之后"的 max_jump（0 基数组，1 基步数）。
 * 返回最小的步数 S，使得从 S 步起每一步都满足 hist <= threshold；
 * 若整个历史都满足则 S = 1；若连最后一步都不满足则返回 -1。
 *
 * 必须这样从后往前扫，不能取"首次穿越"：初期陡峭前沿还在快速下降，
 * max_jump 会先冲高再单调回落，中途可能短暂跌破阈值又弹回去。
 */
long converge_first_permanent(const double *hist, long n, double threshold)
{
    long i;

    if (n <= 0) {
        return -1;
    }
    i = n - 1;
    while (i >= 0 && hist[i] <= threshold) {
        i--;
    }
    if (i < 0) {
        return 1; /* 从头就满足 */
    }
    if (i + 1 >= n) {
        return -1; /* 最后一步仍超阈值 */
    }
    return i + 2; /* 下标 i+1 对应步数 i+2 */
}


/* ========== export.c ========== */


int export_profiles(const char *outdir, const char *name, const Grid *g,
                    const double *u0, const double *u10, const double *u20,
                    const double *u30, const double *ucv,
                    double target, double eps, double t_conv, int periodic,
                    const char *meta)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    /* 参数一律 key=value，核对程序按 key 取，不靠行的顺序 */
    fprintf(fp, "# param length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(fp, "# param hot_lo=%.17g hot_hi=%.17g hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# param target=%.17g eps=%.17g t_conv=%.17g periodic=%ld\n",
            target, eps, t_conv, (long)periodic);
    fprintf(fp, "# columns: x u_t0 u_t10 u_t20 u_t30 u_converged\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%.17g %.17g %.17g %.17g %.17g %.17g\n",
                g->x[i], u0[i], u10[i], u20[i], u30[i], ucv[i]);
    }
    fclose(fp);
    return 0;
}

int export_stepdt(const char *outdir, const char *name, const Grid *g,
                  const double *ua, const double *ub, double t0,
                  double dt, double alpha, int periodic, const char *meta)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    /* 参数行用 key=value，核对程序按 key 取，不靠行的顺序 */
    fprintf(fp, "# param length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(fp, "# param dt=%.17g alpha=%.17g r=%.17g periodic=%ld t0=%.17g\n",
            dt, alpha, alpha * dt / (g->dx * g->dx), (long)periodic, t0);
    fprintf(fp, "# param hot_lo=%.17g hot_hi=%.17g hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# columns: x u_t u_t_plus_dt\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%.17g %.17g %.17g\n", g->x[i], ua[i], ub[i]);
    }
    fclose(fp);
    return 0;
}

int evo_open(EvoWriter *w, const char *outdir, const char *name,
             const Grid *g, const char *meta)
{
    w->fp = NULL;
    w->nn = 0;
    w->nslice = 0;
    sprintf(w->path, "%s/%s.dat", outdir, name);
    w->fp = fopen(w->path, "w");
    if (w->fp == NULL) {
        fprintf(stderr, "cannot write %s\n", w->path);
        return 1;
    }
    fprintf(w->fp, "# %s\n", meta);
    fprintf(w->fp, "# length=%.17g dx=%.17g n=%ld nn=%ld\n",
            g->length, g->dx, g->n, g->nn);
    fprintf(w->fp, "# 每行：t 后面跟 nn 个节点温度（完整 u(x_i,t_n)）\n");
    fprintf(w->fp, "# 时间用几何级数抽稀：早期密、后期疏，切片数不超过上限。\n");
    fprintf(w->fp, "# 数值保留 10 位有效数字，画图够用；需要逐位复算的数一律从\n");
    fprintf(w->fp, "# profiles_*.dat 取，那里保留 17 位（双精度可分辨的位数）。\n");
    w->nn = g->nn;
    return 0;
}

int evo_write(EvoWriter *w, double t, const double *u)
{
    long i;

    if (w->fp == NULL) {
        return 1;
    }
    fprintf(w->fp, "%.10g", t);
    for (i = 0; i < w->nn; i++) {
        fprintf(w->fp, " %.10g", u[i]);
    }
    fprintf(w->fp, "\n");
    w->nslice++;
    return 0;
}

int evo_close(EvoWriter *w)
{
    if (w->fp != NULL) {
        fclose(w->fp);
        w->fp = NULL;
    }
    return 0;
}

int export_criterion(const char *outdir, const char *name, const char *meta,
                     const double *eps_step, const long *stop_step,
                     const double *stop_time, const double *dev_at_stop,
                     long nrows, double eps_main, double tau, double dt)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# param eps_main=%.17g tau=%.17g dt=%.17g\n",
            eps_main, tau, dt);
    fprintf(fp, "# columns: eps_step stop_step(-1=未达到) stop_time "
                 "dev_at_stop dev_over_eps\n");
    for (i = 0; i < nrows; i++) {
        if (stop_step[i] > 0) {
            fprintf(fp, "%.17g %ld %.17g %.17g %.17g\n",
                    eps_step[i], stop_step[i], stop_time[i], dev_at_stop[i],
                    dev_at_stop[i] / eps_main);
        } else {
            /* 未达到：时刻与残差都没有意义，写 NA 而不是留个像样的数 */
            fprintf(fp, "%.17g %ld NA NA NA\n", eps_step[i], stop_step[i]);
        }
    }
    fclose(fp);
    return 0;
}

int export_columns(const char *outdir, const char *name, const char *meta,
                   const double *x, const double *const *cols, long ncols,
                   const char *const *col_heads, long nn)
{
    char path[512];
    FILE *fp;
    long i;
    long j;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# columns: x");
    for (j = 0; j < ncols; j++) {
        fprintf(fp, " %s", col_heads[j]);
    }
    fprintf(fp, "\n");
    for (i = 0; i < nn; i++) {
        fprintf(fp, "%.17g", x[i]);
        for (j = 0; j < ncols; j++) {
            fprintf(fp, " %.17g", cols[j][i]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}

int export_summary(const char *outdir, const char *name,
                   const double *rows, long ncols, long nrows,
                   const char *const *heads, const char *meta)
{
    char path[512];
    FILE *fp;
    long i;
    long j;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# %s\n", meta);
    fprintf(fp, "# columns:");
    for (j = 0; j < ncols; j++) {
        fprintf(fp, " %s", heads[j]);
    }
    fprintf(fp, "\n");
    for (i = 0; i < nrows; i++) {
        for (j = 0; j < ncols; j++) {
            fprintf(fp, "%.17g ", rows[i * ncols + j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return 0;
}


/* ========== verify.c ========== */


#define V_NNCAP   1024
#define V_LINEBUF 65536
#define V_PROF_COLS 6
#define V_SD_COLS   3
#define V_CR_COLS   5
#define V_SU_COLS   16

/* C89 没有 NAN 宏：NA 用这个不可能自然出现的哨兵值表示 */
#define V_NA (-1.23456789012345e300)

static long v_checks_run = 0;
static long v_checks_failed = 0;

static void vcheck(const char *id, const char *what, int ok, const char *detail)
{
    v_checks_run++;
    if (!ok) {
        v_checks_failed++;
    }
    printf("  [%s] %-4s %-54s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/* ---------- 文件读取 ---------- */

/*
 * 读下一个数据行（跳过 # 注释）。NA 解析成 V_NA 哨兵。
 * 返回列数；0 = 到文件末尾；-1 = 格式或容量问题。
 */
static long dat_next(FILE *fp, double *row, long cap)
{
    char line[V_LINEBUF];
    char *p;
    char *end;
    long cols;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '#') {
            continue;
        }
        cols = 0;
        p = line;
        while (*p != '\0') {
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (*p == '\0' || *p == '\n') {
                break;
            }
            if (cols >= cap) {
                return -1;
            }
            if (*p == 'N') {
                while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
                    p++;
                }
                row[cols++] = V_NA;
                continue;
            }
            row[cols] = strtod(p, &end);
            if (end == p) {
                return -1;
            }
            cols++;
            p = end;
        }
        if (cols > 0) {
            return cols;
        }
    }
    return 0;
}

/* 整个小文件读进 buf（按行展平），返回行数 */
static long dat_load(const char *outdir, const char *name, double *buf,
                     long cap, long *ncol)
{
    char path[512];
    FILE *fp;
    long n = 0;
    long cols;
    long first = -1;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }
    while (n < cap) {
        cols = dat_next(fp, buf + n, cap - n);
        if (cols <= 0) {
            break;
        }
        if (first < 0) {
            first = cols;
        } else if (cols != first) {
            fclose(fp);
            return -2;
        }
        n += cols;
    }
    fclose(fp);
    if (first < 0) {
        return -3;
    }
    *ncol = first;
    if (n % first != 0) {
        return -4;
    }
    return n / first;
}

/* 只取第一个和最后一个数据行（演化文件太大，不能整个读进来） */
static int dat_first_last(const char *outdir, const char *name,
                          double *first, double *last, long cap, long *ncol)
{
    char path[512];
    FILE *fp;
    double *tmp;
    long cols;
    long seen = 0;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }
    tmp = (double *)malloc((size_t)cap * sizeof(double));
    if (tmp == NULL) {
        fclose(fp);
        return 2;
    }
    while ((cols = dat_next(fp, tmp, cap)) > 0) {
        if (seen == 0) {
            memcpy(first, tmp, (size_t)cols * sizeof(double));
            *ncol = cols;
        }
        memcpy(last, tmp, (size_t)cols * sizeof(double));
        seen++;
    }
    free(tmp);
    fclose(fp);
    return (seen > 0) ? 0 : 3;
}

/* 从注释头按 key 取一个数；key 前面必须有一个空格，避免 n= 命中 nn= */
static int dat_param(const char *outdir, const char *name, const char *key,
                     double *out)
{
    char path[512];
    char line[V_LINEBUF];
    char search[64];
    char *p;
    char *end;
    FILE *fp;

    sprintf(path, "%s/%s.dat", outdir, name);
    sprintf(search, " %s=", key);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return 1;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] != '#') {
            break; /* 参数都在注释头 */
        }
        p = strstr(line, search);
        if (p != NULL) {
            p += strlen(search);
            *out = strtod(p, &end);
            if (end != p) {
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return 2;
}

/*
 * 两个数的相对差。演化文件只存 10 位有效数字，往返误差的量级是
 * 5e-10 x |值|：周期收敛值约 10 度，绝对差就有 5e-9，用绝对容差会误判。
 * 所以跨文件比对一律按相对差，量级小于 1 的值按 1 兜底。
 */
static double rel_diff(double a, double b)
{
    double m = fabs(a);

    if (fabs(b) > m) {
        m = fabs(b);
    }
    if (m < 1.0) {
        m = 1.0;
    }
    return fabs(a - b) / m;
}

/* ---------- 有限体积定义式（自己按定义重算，不调用 grid.c） ---------- */

static double seg_overlap(double a, double b, double lo, double hi)
{
    double l = (a > lo) ? a : lo;
    double r = (b < hi) ? b : hi;
    return (r > l) ? r - l : 0.0;
}

static double fv_overlap(double x, double dx, double lo, double hi,
                         double len, int periodic)
{
    double a = x - dx / 2.0;
    double b = x + dx / 2.0;

    if (!periodic) {
        if (a < 0.0) {
            a = 0.0;
        }
        if (b > len) {
            b = len;
        }
        return seg_overlap(a, b, lo, hi);
    }
    return seg_overlap(a, b, lo, hi)
           + seg_overlap(a, b, lo - len, hi - len)
           + seg_overlap(a, b, lo + len, hi + len);
}

static double fv_width(double x, double dx, double len, int periodic)
{
    double a;
    double b;

    if (periodic) {
        return dx;
    }
    a = x - dx / 2.0;
    b = x + dx / 2.0;
    if (a < 0.0) {
        a = 0.0;
    }
    if (b > len) {
        b = len;
    }
    return b - a;
}

/* ---------- 单个组合 ---------- */

static void verify_case(const char *outdir, const char *dxtag,
                        const char *bctag, int periodic)
{
    static double prof[(long)V_NNCAP * V_PROF_COLS];
    static double sd[(long)V_NNCAP * V_SD_COLS];
    static double cr[(long)V_NNCAP * V_CR_COLS];
    static double ev1[V_NNCAP];
    static double evN[V_NNCAP];
    char name[128];
    double len, dx, nf, nnf, hlo, hhi, htemp, ctemp;
    double target, eps, tconv, perflag;
    double sdt, salpha, sr;
    double ctau, cdt, ceps;
    long n;
    long nn;
    long nrows;
    long ncol;
    long i;
    long c;
    double pred;
    double worst;
    double d;
    double heat0;
    double colmax[V_PROF_COLS];
    double colmin[V_PROF_COLS];
    double dev;
    double jump;
    double bal_pred;
    int ok;
    char buf[256];

    printf("\n  ---- %s / %s ----\n", dxtag, bctag);

    sprintf(name, "profiles_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, prof, (long)V_NNCAP * V_PROF_COLS, &ncol);
    if (nrows < 0 || ncol != V_PROF_COLS) {
        vcheck("V0", "剖面文件可读且列数为 6", 0, name);
        return;
    }
    dat_param(outdir, name, "length", &len);
    dat_param(outdir, name, "dx", &dx);
    dat_param(outdir, name, "n", &nf);
    dat_param(outdir, name, "nn", &nnf);
    dat_param(outdir, name, "hot_lo", &hlo);
    dat_param(outdir, name, "hot_hi", &hhi);
    dat_param(outdir, name, "hot_temp", &htemp);
    dat_param(outdir, name, "cold_temp", &ctemp);
    dat_param(outdir, name, "target", &target);
    dat_param(outdir, name, "eps", &eps);
    dat_param(outdir, name, "t_conv", &tconv);
    dat_param(outdir, name, "periodic", &perflag);
    n = (long)nf;
    nn = (long)nnf;

    if (nrows != nn) {
        sprintf(buf, "行数 %ld vs nn=%ld", nrows, nn);
        vcheck("V0", "剖面行数等于 nn", 0, buf);
        return;
    }

    /* V1 初值列 = 有限体积定义式 */
    worst = 0.0;
    for (i = 0; i < nn; i++) {
        double x = prof[i * V_PROF_COLS + 0];
        double w = fv_width(x, dx, len, periodic);
        double ov = fv_overlap(x, dx, hlo, hhi, len, periodic);
        pred = ctemp + (htemp - ctemp) * (ov / w);
        d = fabs(prof[i * V_PROF_COLS + 1] - pred);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "max|u_t0 - 定义式| = %.3e", worst);
    vcheck("V1", "初值列 = 控制体重叠定义式", worst <= 1e-12, buf);

    /* V2 初值热量 = (hot-cold)*宽度 */
    heat0 = 0.0;
    for (i = 0; i < nn; i++) {
        double x = prof[i * V_PROF_COLS + 0];
        if (periodic && i == n) {
            break; /* i=n 与 i=0 是同一点，不重复计入 */
        }
        heat0 += prof[i * V_PROF_COLS + 1] * fv_width(x, dx, len, periodic);
    }
    pred = (htemp - ctemp) * (hhi - hlo);
    sprintf(buf, "sum u_t0 w = %.15f，(100-0)*0.01 = %.15f，差 %.3e",
            heat0, pred, fabs(heat0 - pred));
    vcheck("V2", "初值总热量 = (hot-cold) x 高温段长",
           fabs(heat0 - pred) <= 1e-12 * pred, buf);

    /* V3 边界条件逐列成立 */
    worst = 0.0;
    for (c = 1; c < V_PROF_COLS; c++) {
        if (periodic) {
            d = fabs(prof[0 * V_PROF_COLS + c] - prof[n * V_PROF_COLS + c]);
        } else {
            d = fabs(prof[0 * V_PROF_COLS + c] - ctemp);
            if (fabs(prof[n * V_PROF_COLS + c] - ctemp) > d) {
                d = fabs(prof[n * V_PROF_COLS + c] - ctemp);
            }
        }
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "五条剖面全部满足，最大偏差 %.3e", worst);
    vcheck("V3", periodic ? "周期：u(0,t) 与 u(l,t) 是同一点"
                          : "Dirichlet：两端恒等于 cold_temp",
           worst == 0.0, buf);

    /* V4 极值原理（r<=1/2 时凸组合的推论），V5 最大值沿时间单调不增 */
    for (c = 1; c < V_PROF_COLS; c++) {
        colmax[c] = prof[0 * V_PROF_COLS + c];
        colmin[c] = colmax[c];
        for (i = 1; i < nn; i++) {
            if (prof[i * V_PROF_COLS + c] > colmax[c]) {
                colmax[c] = prof[i * V_PROF_COLS + c];
            }
            if (prof[i * V_PROF_COLS + c] < colmin[c]) {
                colmin[c] = prof[i * V_PROF_COLS + c];
            }
        }
    }
    ok = 1;
    for (c = 2; c < V_PROF_COLS; c++) {
        if (colmax[c] > colmax[1] + 1e-12 || colmin[c] < colmin[1] - 1e-12) {
            ok = 0;
        }
    }
    sprintf(buf, "初值范围 [%.6f, %.6f]，后续剖面均未越出", colmin[1], colmax[1]);
    vcheck("V4", "极值原理：max 不增、min 不减", ok, buf);

    ok = 1;
    for (c = 2; c < V_PROF_COLS; c++) {
        if (colmax[c] > colmax[c - 1] + 1e-12) {
            ok = 0;
        }
    }
    sprintf(buf, "max u: %.6f -> %.6f -> %.6f -> %.6f -> %.3e",
            colmax[1], colmax[2], colmax[3], colmax[4], colmax[5]);
    vcheck("V5", "max u 沿 0/10/20/30/收敛 单调不增", ok, buf);

    /* V6 对称性（关于棍中点） */
    worst = 0.0;
    for (c = 1; c < V_PROF_COLS; c++) {
        for (i = 0; i < nn; i++) {
            d = fabs(prof[i * V_PROF_COLS + c]
                     - prof[(n - i) * V_PROF_COLS + c]);
            if (d > worst) {
                worst = d;
            }
        }
    }
    sprintf(buf, "五条剖面 max|u_i - u_(n-i)| = %.3e", worst);
    vcheck("V6", "关于中点对称", worst <= 1e-9, buf);

    /* V7 收敛判据自洽 */
    dev = 0.0;
    for (i = 0; i < nn; i++) {
        d = fabs(prof[i * V_PROF_COLS + 5] - target);
        if (d > dev) {
            dev = d;
        }
    }
    sprintf(buf, "max|u_conv - target| = %.4e，eps = %.1e", dev, eps);
    vcheck("V7", "收敛列满足主判据且没有多跑",
           dev <= eps && dev > eps / 1000.0, buf);

    /* V8 终态取值 */
    pred = periodic ? heat0 / len : ctemp;
    sprintf(buf, "target = %.12f，定义式给出 %.12f", target, pred);
    vcheck("V8", periodic ? "周期终态 = 初值热量 / l = 10 度"
                          : "Dirichlet 终态 = 边界温度 0 度",
           fabs(target - pred) <= 1e-9, buf);

    /* V9 FTCS 差分方程本身 */
    sprintf(name, "stepdt_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, sd, (long)V_NNCAP * V_SD_COLS, &ncol);
    if (nrows < 0 || ncol != V_SD_COLS || nrows != nn) {
        vcheck("V9", "stepdt 文件可读且行数等于 nn", 0, name);
    } else {
        dat_param(outdir, name, "dt", &sdt);
        dat_param(outdir, name, "alpha", &salpha);
        dat_param(outdir, name, "r", &sr);
        jump = 0.0;
        /* 只有独立未知量才满足递推：Dirichlet 是 i=1..n-1，
           周期是 i=0..n-1（i=n 是 i=0 的副本，代进去会越界读一列） */
        for (i = periodic ? 0 : 1; i < n; i++) {
            long im;
            long ip;
            double res;

            if (periodic) {
                im = (i == 0) ? n - 1 : i - 1;
                ip = (i == n - 1) ? 0 : i + 1;
            } else {
                im = i - 1;
                ip = i + 1;
            }
            res = sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1]
                  - sr * (sd[ip * V_SD_COLS + 1] - 2.0 * sd[i * V_SD_COLS + 1]
                          + sd[im * V_SD_COLS + 1]);
            if (fabs(res) > jump) {
                jump = fabs(res);
            }
        }
        sprintf(buf, "max 残差 = %.3e（r=%.17g 取自文件头）", jump, sr);
        vcheck("V9", "导出数字逐个满足 FTCS 差分方程", jump <= 1e-14, buf);

        /* V10 离散热量收支 */
        if (periodic) {
            double s = 0.0;
            for (i = 0; i < n; i++) {
                s += sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1];
            }
            sprintf(buf, "sum(u^{n+1}-u^n) = %.3e", s);
            vcheck("V10", "周期：离散步热量严格守恒", fabs(s) <= 1e-12, buf);
        } else {
            double s = 0.0;
            for (i = 1; i < n; i++) {
                s += sd[i * V_SD_COLS + 2] - sd[i * V_SD_COLS + 1];
            }
            bal_pred = -sr * (sd[1 * V_SD_COLS + 1]
                              + sd[(n - 1) * V_SD_COLS + 1]);
            sprintf(buf, "内部和 = %.12f，-r(u_1+u_{n-1}) = %.12f，差 %.3e",
                    s, bal_pred, fabs(s - bal_pred));
            vcheck("V10", "Dirichlet：离散步热量收支 = 两端净流出",
                   fabs(s - bal_pred) <= 1e-12, buf);
        }
    }

    /* V11 副判据的松紧 */
    sprintf(name, "criterion_%s_%s", dxtag, bctag);
    nrows = dat_load(outdir, name, cr, (long)V_NNCAP * V_CR_COLS, &ncol);
    if (nrows < 0 || ncol != V_CR_COLS) {
        vcheck("V11", "criterion 文件可读", 0, name);
    } else {
        dat_param(outdir, name, "tau", &ctau);
        dat_param(outdir, name, "dt", &cdt);
        dat_param(outdir, name, "eps_main", &ceps);
        worst = 0.0;
        ok = 0;
        for (i = 0; i < nrows; i++) {
            double es = cr[i * V_CR_COLS + 0];
            double st = cr[i * V_CR_COLS + 1];
            double dv = cr[i * V_CR_COLS + 3];
            if (st < 0.0 || st == V_NA) {
                continue; /* NA / 未达到 */
            }
            pred = es * (ctau / cdt);
            d = fabs(dv / pred - 1.0);
            if (d > worst) {
                worst = d;
            }
            ok++;
        }
        sprintf(buf, "%d 档达到，实测/预测 最大相对差 %.4f%%",
                ok, 100.0 * worst);
        vcheck("V11", "副判据停手残差 = eps_step x tau/dt",
               ok >= 3 && worst <= 0.05, buf);
    }

    /* V12 演化文件首末行与剖面一致 */
    sprintf(name, "evolution_%s_%s", dxtag, bctag);
    if (dat_first_last(outdir, name, ev1, evN, V_NNCAP, &ncol) != 0
        || ncol != nn + 1) {
        sprintf(buf, "读到 %ld 列，应为 nn+1 = %ld（多一列 t）", ncol, nn + 1);
        vcheck("V12", "演化文件可读且列数等于 nn+1", 0, buf);
    } else {
        worst = 0.0;
        for (i = 0; i < nn; i++) {
            d = rel_diff(ev1[i + 1], prof[i * V_PROF_COLS + 1]);
            if (d > worst) {
                worst = d;
            }
        }
        sprintf(buf, "首行 t=%.10g 与 u_t0 列 max 相对差 %.3e", ev1[0], worst);
        vcheck("V12", "演化首行 = 初值剖面", worst <= 1e-9, buf);

        worst = 0.0;
        for (i = 0; i < nn; i++) {
            d = rel_diff(evN[i + 1], prof[i * V_PROF_COLS + 5]);
            if (d > worst) {
                worst = d;
            }
        }
        sprintf(buf, "末行 t=%.10g（t_conv=%.10g）与收敛列 max 相对差 %.3e",
                evN[0], tconv, worst);
        vcheck("V13", "演化末行 = 收敛剖面", worst <= 1e-9, buf);
    }
}

/* ---------- 跨组合 ---------- */

static void verify_global(const char *outdir)
{
    static double su[(long)V_NNCAP * V_SU_COLS];
    long nrows;
    long ncol;
    long i;
    double worst;
    double d;
    double rel;
    double eps;
    char buf[256];

    printf("\n  ---- 跨组合（summary.dat）----\n");
    nrows = dat_load(outdir, "summary", su, (long)V_NNCAP * V_SU_COLS, &ncol);
    if (nrows < 0 || ncol != V_SU_COLS) {
        vcheck("V14", "summary 可读且列数为 16", 0, "summary.dat");
        return;
    }
    dat_param(outdir, "profiles_fine_dirichlet", "eps", &eps);

    /* V14 每行 t_pred 的定义式 */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        double tau = su[i * V_SU_COLS + 5];
        double amp = su[i * V_SU_COLS + 4];
        double t_pred = su[i * V_SU_COLS + 6];
        double p = tau * log(amp / eps);

        d = fabs(t_pred / p - 1.0);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "四行 t_pred vs tau*ln(amp/eps) 最大相对差 %.3e", worst);
    vcheck("V14", "预测收敛时刻满足它自己的定义式", worst <= 1e-12, buf);

    /* V15 实测 t_conv 与预测 t_pred 一致 */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        rel = fabs(su[i * V_SU_COLS + 7] / su[i * V_SU_COLS + 6] - 1.0);
        if (rel > worst) {
            worst = rel;
        }
    }
    sprintf(buf, "最大相对差 %.4f%%", 100.0 * worst);
    vcheck("V15", "实测收敛时刻与单指数预测一致", worst <= 0.05, buf);

    /* V16 步数与时刻自洽：steps_conv * dt = t_conv */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        d = fabs(su[i * V_SU_COLS + 8] * su[i * V_SU_COLS + 2]
                 - su[i * V_SU_COLS + 7]);
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "max |steps*dt - t_conv| = %.3e", worst);
    vcheck("V16", "步数、dt、收敛时刻三者自洽", worst <= 1e-9, buf);

    /* V17 停手残差不超过 eps */
    worst = 0.0;
    for (i = 0; i < nrows; i++) {
        d = su[i * V_SU_COLS + 9];
        if (d > worst) {
            worst = d;
        }
    }
    sprintf(buf, "四组合最大停手残差 %.4e，eps %.1e", worst, eps);
    vcheck("V17", "所有组合的停手残差都不超 eps", worst <= eps, buf);
}

long verify_checks_run(void)
{
    return v_checks_run;
}

long verify_run(const char *outdir)
{
    printf("\n== 代入式核对：读回 data/*.dat 自己验定义式 ==\n");
    verify_case(outdir, "fine", "dirichlet", 0);
    verify_case(outdir, "fine", "periodic", 1);
    verify_case(outdir, "coarse", "dirichlet", 0);
    verify_case(outdir, "coarse", "periodic", 1);
    verify_global(outdir);
    printf("\n== 代入式核对：%ld 项，%ld 项失败 ==\n",
           v_checks_run, v_checks_failed);
    return v_checks_failed;
}


/* ========== main.c ========== */


/* 本步的检查计数器：任何一项 FAIL 都让程序非零退出 */
static long checks_run = 0;
static long checks_failed = 0;

static void check(const char *id, const char *what, int ok, const char *detail)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
    }
    printf("  [%s] %-4s %-46s %s\n", ok ? "PASS" : "FAIL", id, what, detail);
}

/*
 * 附图数据用的上限与缓存。分解表在 report_error_decomposition 里填、
 * 由 export_demo_data 写出：两个函数之间不传参，只靠这几个静态量。
 */
#define V_DEMO_NN 256
static double demo_dx[3];
static double demo_fmt[3];
static double demo_rep[3];
static double demo_tot[3];
static long demo_n = 0;

static void report_material(void)
{
    double a;
    double arc;
    double dev;
    char buf[160];

    a = material_alpha();
    arc = material_alpha_from_krc();
    dev = material_alpha_rel_dev();

    printf("== 材料：各向同性铁 ==\n");
    printf("  采用 alpha            = %.10e m^2/s (= %.4f mm^2/s)\n",
           a, a * 1e6);
    printf("  k/(rho*cp)            = %.10e m^2/s\n", arc);
    printf("    k = %.1f W/(m*K), rho = %.1f kg/m^3, cp = %.1f J/(kg*K)\n",
           IRON_K, IRON_RHO, IRON_CP);
    printf("  相对偏差              = %+.4f%%\n", 100.0 * dev);
    sprintf(buf, "表值与 k/(rho*cp) 差 %+.3f%%", 100.0 * dev);
    check("M1", "alpha 表值与 k/(rho*cp) 一致（2% 内）", fabs(dev) < 0.02, buf);

    printf("  扩散时间 tau（最低模衰减到 1/e）:\n");
    printf("    Dirichlet  l^2/(pi^2 alpha)   = %.6f s\n",
           material_tau_dirichlet(a, ROD_LENGTH));
    printf("    周期       l^2/(4 pi^2 alpha) = %.6f s\n",
           material_tau_periodic(a, ROD_LENGTH));
    printf("    比值 = %.1f（周期边界收敛快 4 倍）\n",
           material_tau_dirichlet(a, ROD_LENGTH)
               / material_tau_periodic(a, ROD_LENGTH));
    printf("  题目要画的时刻 10/20/30 s 相当于 Dirichlet 的 %.3f/%.3f/%.3f tau\n",
           SNAP_T1 / material_tau_dirichlet(a, ROD_LENGTH),
           SNAP_T2 / material_tau_dirichlet(a, ROD_LENGTH),
           SNAP_T3 / material_tau_dirichlet(a, ROD_LENGTH));
}

static int export_initial(const Grid *g, const double *naive, int periodic,
                          const char *outdir, const char *name)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/%s.dat", outdir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    fprintf(fp, "# initial temperature field, finite-volume vs naive\n");
    fprintf(fp, "# length=%.17g dx=%.17g n=%ld periodic=%d\n",
            g->length, g->dx, g->n, periodic);
    fprintf(fp, "# hot=[%.17g,%.17g] hot_temp=%.17g cold_temp=%.17g\n",
            HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    fprintf(fp, "# i x_i u_fv u_naive cell_width\n");
    for (i = 0; i < g->nn; i++) {
        fprintf(fp, "%ld %.17g %.17g %.17g %.17g\n",
                i, g->x[i], g->u[i], naive[i],
                grid_cell_width(g, i, periodic));
    }
    fclose(fp);
    return 0;
}

static int report_grid(double dx_req, const char *tag, const char *name,
                       const char *outdir)
{
    Grid g_dir;   /* Dirichlet 口径的有限体积初值 */
    Grid g_per;   /* 周期口径的有限体积初值（端节点控制体跨界回绕） */
    Grid g_naive; /* 负向对照：闭区间打点 */
    double *naive;
    double exact_heat;
    double heat_d;
    double heat_p;
    double mean_p;
    double sym;
    double r;
    double dtmax;
    double naive_heat;
    double umin;
    double umax;
    long i;
    long n_hot;
    long n_hot_naive;
    int rc;
    char buf[192];

    printf("\n== 网格与初值：%s（Dx = %.4f m = %.1f cm）==\n",
           tag, dx_req, dx_req * 100.0);

    /* 先清零：短路求值下后面的 grid_init 可能根本没被调用，
       而错误分支要对三个 Grid 都调 grid_free */
    memset(&g_dir, 0, sizeof(Grid));
    memset(&g_per, 0, sizeof(Grid));
    memset(&g_naive, 0, sizeof(Grid));

    if (grid_init(&g_dir, ROD_LENGTH, dx_req) != 0
            || grid_init(&g_per, ROD_LENGTH, dx_req) != 0
            || grid_init(&g_naive, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        grid_free(&g_dir);
        grid_free(&g_per);
        grid_free(&g_naive);
        return 1;
    }
    naive = (double *)malloc((size_t)g_dir.nn * sizeof(double));
    if (naive == NULL) {
        grid_free(&g_dir);
        grid_free(&g_per);
        grid_free(&g_naive);
        return 1;
    }

    printf("  n = %ld 单元, %ld 节点, 实际 dx = %.17g m\n",
           g_dir.n, g_dir.nn, g_dir.dx);
    printf("  高温段 [%.4f, %.4f] m，居中\n", HOT_LO, HOT_HI);

    /* 三份初值各自独立存放，互不覆盖 */
    grid_initial_fv(&g_dir, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    grid_initial_fv(&g_per, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 1);
    grid_initial_naive(&g_naive, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP);
    for (i = 0; i < g_dir.nn; i++) {
        naive[i] = g_naive.u[i];
    }

    exact_heat = (HOT_TEMP - COLD_TEMP) * HOT_WIDTH + COLD_TEMP * ROD_LENGTH;
    heat_d = grid_heat_dirichlet(&g_dir);
    heat_p = grid_heat_periodic(&g_per);
    mean_p = grid_mean_periodic(&g_per);
    sym = grid_symmetry_defect(&g_dir);
    naive_heat = grid_heat_dirichlet(&g_naive);

    printf("  精确 int u0 dx        = %.12f 摄氏度*m\n", exact_heat);
    printf("  FV 热量（梯形权重）   = %.12f\n", heat_d);
    printf("  FV 热量（周期求和）   = %.12f\n", heat_p);
    printf("  FV 周期平均温度       = %.12f 摄氏度\n", mean_p);
    printf("  FV 对称性偏差         = %.3e\n", sym);
    printf("  naive 闭区间打点热量  = %.12f（偏差 %+.4f%%）\n",
           naive_heat, 100.0 * (naive_heat - exact_heat) / exact_heat);

    umin = g_dir.u[0];
    umax = g_dir.u[0];
    for (i = 1; i < g_dir.nn; i++) {
        if (g_dir.u[i] < umin) {
            umin = g_dir.u[i];
        }
        if (g_dir.u[i] > umax) {
            umax = g_dir.u[i];
        }
    }
    n_hot = grid_count_equal(&g_dir, HOT_TEMP, 1e-9);
    n_hot_naive = grid_count_equal(&g_naive, HOT_TEMP, 1e-9);
    printf("  FV 取值范围 [%.6f, %.6f]，恰好 100 度的节点 %ld 个\n",
           umin, umax, n_hot);
    printf("  naive 恰好 100 度的节点 %ld 个\n", n_hot_naive);

    printf("  非零节点（x, u_FV, u_naive）:\n   ");
    for (i = 0; i < g_dir.nn; i++) {
        if (g_dir.u[i] != COLD_TEMP || naive[i] != COLD_TEMP) {
            printf(" (%.4f,%.1f,%.1f)", g_dir.x[i], g_dir.u[i], naive[i]);
        }
    }
    printf("\n");

    dtmax = material_dt_max_ftcs(material_alpha(), g_dir.dx);
    r = material_fourier_number(material_alpha(), g_dir.dx, DEFAULT_DT);
    printf("  FTCS 稳定上限 dt_max  = dx^2/(2 alpha) = %.10f s\n", dtmax);
    printf("  自设 dt = %.4f s -> r = alpha*dt/dx^2 = %.10f\n", DEFAULT_DT, r);
    printf("  到 30 s 需要 %.0f 步\n", SNAP_T3 / DEFAULT_DT);

    /* ---- 本步验收 ---- */
    sprintf(buf, "n*dx=%.17g vs l=%.17g",
            (double)g_dir.n * g_dir.dx, ROD_LENGTH);
    check("G1", "网格正好铺满棍长",
          fabs((double)g_dir.n * g_dir.dx - ROD_LENGTH) < 1e-15, buf);

    sprintf(buf, "%.12f vs %.12f", heat_d, exact_heat);
    check("G2", "FV 初值热量 = int u0 dx（Dirichlet 权重）",
          fabs(heat_d - exact_heat) <= 1e-12 * exact_heat, buf);

    sprintf(buf, "%.12f vs %.12f", heat_p, exact_heat);
    check("G3", "FV 初值热量 = int u0 dx（周期求和）",
          fabs(heat_p - exact_heat) <= 1e-12 * exact_heat, buf);

    sprintf(buf, "mean=%.12f, 目标 %.1f", mean_p, exact_heat / ROD_LENGTH);
    check("G4", "周期平均温度 = 10 摄氏度（终态值）",
          fabs(mean_p - exact_heat / ROD_LENGTH) <= 1e-12, buf);

    sprintf(buf, "max|u_i-u_(n-i)| = %.3e", sym);
    check("G5", "初值关于棍中点对称", sym <= 1e-12, buf);

    sprintf(buf, "[%.6f, %.6f] 落在 [0,100] 内", umin, umax);
    check("G6", "初值不越界（离散极值原理的起点）",
          umin >= COLD_TEMP - 1e-12 && umax <= HOT_TEMP + 1e-12, buf);

    sprintf(buf, "r=%.8f <= 0.5, dt_max=%.8f s", r, dtmax);
    check("G7", "自设 dt 满足 FTCS 稳定性 r<=1/2", r <= FTCS_R_MAX, buf);

    sprintf(buf, "naive 热量 %.6f, 偏差 %+.4f%%", naive_heat,
            100.0 * (naive_heat - exact_heat) / exact_heat);
    printf("  [NOTE] %-4s %-46s %s\n", "G8",
           "naive 闭区间打点的热量偏差（负向对照，不是判据）", buf);

    rc = export_initial(&g_dir, naive, 0, outdir, name);

    free(naive);
    grid_free(&g_dir);
    grid_free(&g_per);
    grid_free(&g_naive);
    return rc;
}

/* ================= 步骤 2：显式 FTCS 推进 ================= */

static double field_max(const Grid *g)
{
    long i;
    double m;

    m = g->u[0];
    for (i = 1; i < g->nn; i++) {
        if (g->u[i] > m) {
            m = g->u[i];
        }
    }
    return m;
}

static double field_min(const Grid *g)
{
    long i;
    double m;

    m = g->u[0];
    for (i = 1; i < g->nn; i++) {
        if (g->u[i] < m) {
            m = g->u[i];
        }
    }
    return m;
}

static double field_max_abs(const Grid *g)
{
    long i;
    double m;
    double a;

    m = 0.0;
    for (i = 0; i < g->nn; i++) {
        a = fabs(g->u[i]);
        if (a > m) {
            m = a;
        }
    }
    return m;
}

/*
 * 逐步推进到 30 s，每一步都盯着四个不变量/单调量：
 *   周期：平均温度守恒（差分离散拉普拉斯在圆上求和为 0）
 *   Dirichlet：总热量单调流失（两端是热汇）
 *   两者：最大值不增、最小值不减（r<=1/2 时是凸组合，离散极值原理）
 *   两者：关于中点的对称性保持
 */
static void report_evolution(double dx_req, const char *tag,
                             BoundaryKind bc, const char *bctag)
{
    Grid g;
    Ftcs s;
    double mean0;
    double heat_prev;
    double heat_now;
    double max_prev;
    double min_prev;
    double max_now;
    double min_now;
    double drift;
    double sym;
    double worst_drift;
    double worst_sym;
    double terr;
    double tbound;
    long viol_max;
    long viol_min;
    long viol_heat;
    long step;
    long total_steps;
    long snap1;
    long snap2;
    long snap3;
    int periodic;
    int finite_ok;
    char buf[192];

    periodic = (bc == BC_PERIODIC);
    printf("\n== FTCS 演化：%s + %s（dt = %.4f s, r = %.6f）==\n",
           tag, bctag, DEFAULT_DT,
           material_fourier_number(material_alpha(), dx_req, DEFAULT_DT));

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        checks_run++;
        checks_failed++;
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), DEFAULT_DT) != 0) {
        fprintf(stderr, "ftcs_init failed\n");
        grid_free(&g);
        checks_run++;
        checks_failed++;
        return;
    }

    mean0 = periodic ? grid_mean_periodic(&g) : 0.0;
    heat_prev = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
    max_prev = field_max(&g);
    min_prev = field_min(&g);
    worst_drift = 0.0;
    worst_sym = grid_symmetry_defect(&g);
    viol_max = 0;
    viol_min = 0;
    viol_heat = 0;
    finite_ok = 1;

    total_steps = (long)(SNAP_T3 / DEFAULT_DT + 0.5);
    snap1 = (long)(SNAP_T1 / DEFAULT_DT + 0.5);
    snap2 = (long)(SNAP_T2 / DEFAULT_DT + 0.5);
    snap3 = total_steps;

    printf("  %8s %14s %14s %12s %12s\n",
           "t/s", periodic ? "周期均值" : "总热量", "对称性偏差",
           "max u", "min u");
    printf("  %8.2f %14.10f %14.3e %12.6f %12.6f\n",
           0.0, heat_prev, worst_sym, max_prev, min_prev);

    for (step = 1; step <= total_steps; step++) {
        ftcs_step(&s);
        if (!ftcs_all_finite(&s)) {
            finite_ok = 0;
            break;
        }
        max_now = field_max(&g);
        min_now = field_min(&g);
        heat_now = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
        sym = grid_symmetry_defect(&g);

        if (max_now > max_prev + 1e-10) {
            viol_max++;
        }
        if (min_now < min_prev - 1e-10) {
            viol_min++;
        }
        if (!periodic && heat_now > heat_prev + 1e-12) {
            viol_heat++;
        }
        if (periodic) {
            drift = fabs(heat_now / g.length - mean0);
            if (drift > worst_drift) {
                worst_drift = drift;
            }
        }
        if (sym > worst_sym) {
            worst_sym = sym;
        }

        if (step == snap1 || step == snap2 || step == snap3) {
            printf("  %8.2f %14.10f %14.3e %12.6f %12.6f\n",
                   s.time, heat_now, sym, max_now, min_now);
        }
        max_prev = max_now;
        min_prev = min_now;
        heat_prev = heat_now;
    }

    printf("  走完 %ld 步，t = %.10f s，末步 r = %.10f\n",
           s.steps, s.time, s.r_last);

    sprintf(buf, "全程有限 = %d", finite_ok);
    check("E1", "推进过程不出现 NaN/inf", finite_ok, buf);

    terr = fabs(s.time - SNAP_T3);
    /* 这里是手动 ftcs_step 逐步走，没有 ftcs_advance 的末刻吸附，
       所以 3000 次 time += dt 的舍入误差原样留着。顺序求和的误差
       上界约 n*u*T（u = DBL_EPSILON），留 4 倍余量当判据，
       而不是随手拍一个 1e-12。 */
    tbound = 4.0 * (double)total_steps * DBL_EPSILON * SNAP_T3;
    sprintf(buf, "步数 %ld/%ld，|t-30|=%.3e（求和上界 %.3e）",
            s.steps, total_steps, terr, tbound);
    check("E2", "步数精确、时刻误差在舍入上界内",
          s.steps == total_steps && terr <= tbound, buf);

    if (periodic) {
        sprintf(buf, "max|mean(t)-mean(0)| = %.3e（%ld 步）",
              worst_drift, s.steps);
        check("E3", "周期边界平均温度守恒", worst_drift <= 1e-10, buf);
    } else {
        sprintf(buf, "热量上升的次数 = %ld", viol_heat);
        check("E3", "Dirichlet 总热量单调不增", viol_heat == 0, buf);
    }

    sprintf(buf, "最大值上升 %ld 次，最小值下降 %ld 次", viol_max, viol_min);
    check("E4", "离散极值原理：max 不增、min 不减",
          viol_max == 0 && viol_min == 0, buf);

    sprintf(buf, "全程 max|u_i-u_(n-i)| = %.3e", worst_sym);
    check("E5", "对称性在演化中保持", worst_sym <= 1e-9, buf);

    ftcs_free(&s);
    grid_free(&g);
}

/*
 * 稳定性负向对照：把 r 依次取 0.23 / 0.5 / 0.575 / 0.69。
 * r<=0.5 必须稳，r>0.5 必须发散，而且发散速率要等于最高频模态的
 * 理论放大因子 |1-4r sin^2(q dx/2)|——光"炸了"不够，要炸得对。
 */
static void report_stability(double dx_req, const char *tag)
{
    Grid g;
    Ftcs s;
    double targets[4];
    double dt;
    double r;
    double qmax;
    double gtheory;
    double hist[401];
    double measured;
    double endmax;
    long i;
    long j;
    long step;
    long blown;
    int periodic;
    int finite_ok;
    char buf[192];

    printf("\n== 稳定性对照：%s（Dirichlet，最高模 k = n-1）==\n", tag);
    targets[0] = material_fourier_number(material_alpha(), dx_req, DEFAULT_DT);
    targets[1] = 0.5;
    targets[2] = 0.575;
    targets[3] = 0.69;
    periodic = 0;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        return;
    }
    qmax = (double)(g.n - 1) * HD_PI / ROD_LENGTH;
    printf("  最高模波数 q = (n-1)pi/l = %.6f 1/m，q*dx/2 = %.8f rad\n",
           qmax, qmax * g.dx / 2.0);
    printf("  %8s %12s %10s %14s %14s %12s\n",
           "r", "dt/s", "步数", "末态 max|u|", "理论|G|", "实测每步");

    for (i = 0; i < 4; i++) {
        r = targets[i];
        dt = r * dx_req * dx_req / material_alpha();
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt) != 0) {
            fprintf(stderr, "ftcs_init failed\n");
            continue;
        }
        gtheory = fabs(ftcs_growth_factor(r, qmax, g.dx));
        finite_ok = 1;
        blown = -1;

        if (r <= FTCS_R_MAX) {
            /* 稳定分支：推到 30 s，看是否始终有界 */
            if (ftcs_advance(&s, SNAP_T3) != 0) {
                finite_ok = 0;
            }
            endmax = field_max_abs(&g);
            printf("  %8.5f %12.8f %10ld %14.6e %14.8f %12s\n",
                   r, dt, s.steps, endmax, gtheory, "-");
            sprintf(buf, "r=%.5f: 30 s 后 max|u|=%.6f，有限=%d",
                    r, endmax, finite_ok);
            check("S1", "r<=1/2 稳定且不超过初值 100 度",
                  finite_ok && endmax <= HOT_TEMP + 1e-9, buf);
        } else {
            /* 发散分支 a：物理初值，记录它第几步炸穿 1e12。
               200 步内 r=0.575/0.69 都还不到 inf（约 1e22 / 1e49）。 */
            endmax = 0.0;
            for (step = 0; step <= 200; step++) {
                if (step > 0) {
                    ftcs_step(&s);
                }
                hist[step] = field_max_abs(&g);
                endmax = hist[step];
                if (!ftcs_all_finite(&s)) {
                    finite_ok = 0;
                    blown = step;
                    break;
                }
                if (hist[step] > 1e12 && blown < 0) {
                    blown = step;
                }
            }
            ftcs_free(&s);

            /*
             * 发散分支 b：换成精确特征向量 sin(qmax x)。
             * 物理初值是一堆模态的叠加，k=98 与 k=99 的 |G| 只差 0.13%，
             * 干涉会让 max|u| 的增长率偏离 |G_99| 约 1%（实测 1.2875 vs
             * 1.2994）。纯模态下 u_i^m = G^m sin(qmax x_i) 严格成立，
             * 比值就能对到舍入量级，判据也才敢收紧到 1e-9。
             */
            for (j = 0; j < g.nn; j++) {
                g.u[j] = sin(qmax * g.x[j]);
            }
            if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                          material_alpha(), dt) != 0) {
                fprintf(stderr, "ftcs_init failed\n");
                continue;
            }
            for (step = 0; step <= 200; step++) {
                if (step > 0) {
                    ftcs_step(&s);
                }
                hist[step] = field_max_abs(&g);
            }
            measured = pow(hist[200] / hist[100], 1.0 / 100.0);
            printf("  %8.5f %12.8f %10ld %14.6e %14.8f %12.8f\n",
                   r, dt, s.steps, endmax, gtheory, measured);
            sprintf(buf, "r=%.3f: 第 %ld 步越过 1e12；纯模态每步 %.10f vs 理论 %.10f",
                    r, blown, measured, gtheory);
            check("S2", "r>1/2 发散，纯模态速率严格等于 |G|",
                  blown > 0 && finite_ok == 1
                      && fabs(measured / gtheory - 1.0) < 1e-9, buf);
        }
        ftcs_free(&s);
    }
    grid_free(&g);
}

/*
 * 离散模态的精确核对：sin(q x_i) / cos(q x_i) 是差分拉普拉斯的
 * 精确特征向量，所以走 m 步后参考节点的比值必须等于 G^m，
 * 其中 G = 1-4r sin^2(q dx/2)。这是针对"离散格式"的解析结果，
 * 不依赖 PDE 的解，能独立抓住推进公式写错。
 * 同时列出连续解的 exp(-alpha q^2 t)，两者之差就是离散误差。
 */
static void report_mode(double dx_req, BoundaryKind bc,
                        const char *bctag, long k, long msteps)
{
    Grid g;
    Ftcs s;
    double q;
    double u0ref;
    double uref;
    double measured;
    double gfac;
    double gcont;
    double theory;
    double exact;
    long i;
    long ref;
    long mcap;
    char buf[192];

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        fprintf(stderr, "grid_init failed\n");
        return;
    }
    q = (bc == BC_DIRICHLET) ? (double)k * HD_PI / ROD_LENGTH
                             : 2.0 * (double)k * HD_PI / ROD_LENGTH;
    for (i = 0; i < g.nn; i++) {
        g.u[i] = (bc == BC_DIRICHLET) ? sin(q * g.x[i]) : cos(q * g.x[i]);
    }
    ref = g.n / 2;
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), DEFAULT_DT) != 0) {
        fprintf(stderr, "ftcs_init failed\n");
        grid_free(&g);
        return;
    }
    u0ref = g.u[ref];
    gfac = ftcs_growth_factor(s.r, q, g.dx);
    gcont = exp(-material_alpha() * q * q * DEFAULT_DT);

    /*
     * 强阻尼模态不能硬走 1000 步：k=99 时 G=0.0802，G^1000 约 1e-1096，
     * double 直接下溢成 0，而舍入误差注入的伪低频模还停在 1e-16，
     * 于是"实测/理论"变成 0/0。按 |G|^m >= 1e-6 反推步数上限，
     * 让真信号始终比伪模高 10 个数量级。
     */
    if (fabs(gfac) < 1.0 && fabs(gfac) > 0.0) {
        mcap = (long)(log(1e-6) / log(fabs(gfac)));
        if (mcap < 1) {
            mcap = 1;
        }
        if (msteps > mcap) {
            msteps = mcap;
        }
    }
    for (i = 0; i < msteps; i++) {
        ftcs_step(&s);
    }
    uref = g.u[ref];
    measured = uref / u0ref;
    theory = pow(gfac, (double)msteps);
    exact = pow(gcont, (double)msteps);

    printf("  %-10s k=%-4ld q=%10.4f 1/m  走 %ld 步 (%.3f s)\n",
           bctag, k, q, msteps, (double)msteps * DEFAULT_DT);
    printf("      每步放大因子：离散 G = %+.10f，连续 exp(-alpha q^2 dt) = %.10f\n",
           gfac, gcont);
    printf("      离散/连续 = %.12f（%+.4e %%）<- 这就是空间离散误差\n",
           gfac / gcont, 100.0 * (gfac / gcont - 1.0));
    printf("      实测 u/u0 = %.12e   理论 G^m = %.12e   连续解 = %.12e\n",
           measured, theory, exact);
    printf("      实测 vs 理论 相对差 = %.3e\n", fabs(measured / theory - 1.0));

    sprintf(buf, "k=%ld: 实测/理论-1 = %.3e（走 %ld 步）",
            k, fabs(measured / theory - 1.0), msteps);
    check("D1", "单模衰减等于离散特征值 G^m",
          theory > 0.0 && fabs(measured / theory - 1.0) < 1e-9, buf);

    ftcs_free(&s);
    grid_free(&g);
}

/* ================= 步骤 3：Fourier 级数解析解 ================= */

/* 把 FTCS 推到 t，返回与解析解的逐点最大误差；rms_out 可传 NULL */
static double ftcs_max_error(double dx_req, double dt, double t,
                             BoundaryKind bc, double *rms_out)
{
    Grid g;
    Ftcs s;
    double err;
    double worst;
    double sumsq;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
        grid_free(&g);
        return -1.0;
    }
    if (ftcs_advance(&s, t) != 0) {
        grid_free(&g);
        return -1.0;
    }

    worst = 0.0;
    sumsq = 0.0;
    for (i = 0; i < g.nn; i++) {
        err = g.u[i] - analytic_u(g.x[i], t, 0, periodic);
        if (fabs(err) > worst) {
            worst = fabs(err);
        }
        sumsq += err * err;
    }
    if (rms_out != NULL) {
        *rms_out = sqrt(sumsq / (double)g.nn);
    }
    ftcs_free(&s);
    grid_free(&g);
    return worst;
}

static void report_analytic_coeffs(void)
{
    long k;
    double ba;
    double bn;
    double pa;
    double pn;
    double rel;
    double worst_dir;
    double worst_per;
    double mean;
    char buf[192];

    printf("\n== 解析解的 Fourier 系数：解析式 vs 复合 Simpson 数值投影 ==\n");
    printf("  %4s %18s %18s %12s   %18s %18s %12s\n",
           "k", "b_k 解析", "b_k Simpson", "相对差",
           "A_k 解析", "A_k Simpson", "相对差");

    worst_dir = 0.0;
    worst_per = 0.0;
    for (k = 1; k <= 8; k++) {
        ba = analytic_b_dirichlet(k);
        bn = analytic_project_dirichlet(k, 2000);
        pa = analytic_a_periodic(k);
        pn = analytic_project_periodic(k, 2000);
        /* 恒零系数没法比相对值：|解析式| 低于 1e-10 时改比绝对差 */
        rel = (fabs(ba) > 1e-10) ? fabs(bn / ba - 1.0) : fabs(bn - ba);
        if (rel > worst_dir) {
            worst_dir = rel;
        }
        rel = (fabs(pa) > 1e-10) ? fabs(pn / pa - 1.0) : fabs(pn - pa);
        if (rel > worst_per) {
            worst_per = rel;
        }
        printf("  %4ld %18.12f %18.12f %12.2e   %18.12f %18.12f %12.2e\n",
               k, ba, bn, rel, pa, pn, rel);
    }

    sprintf(buf, "Dirichlet 最大偏差 %.3e", worst_dir);
    check("A1", "解析系数 = Simpson 数值投影（正弦基）", worst_dir < 1e-10, buf);
    sprintf(buf, "周期最大偏差 %.3e", worst_per);
    check("A2", "解析系数 = Simpson 数值投影（余弦基）", worst_per < 1e-10, buf);

    /* 对称性决定的恒零系数：初值关于中点对称 */
    printf("  对称性导致的恒零系数：b_2 = %.3e, b_4 = %.3e, b_6 = %.3e, b_8 = %.3e\n",
           analytic_b_dirichlet(2), analytic_b_dirichlet(4),
           analytic_b_dirichlet(6), analytic_b_dirichlet(8));
    sprintf(buf, "max 偶数项 |b_k| = %.3e", fabs(analytic_b_dirichlet(8)));
    check("A3", "Dirichlet 偶数项恒为 0（初值关于中点对称）",
          fabs(analytic_b_dirichlet(2)) < 1e-12
              && fabs(analytic_b_dirichlet(4)) < 1e-12
              && fabs(analytic_b_dirichlet(6)) < 1e-12
              && fabs(analytic_b_dirichlet(8)) < 1e-12, buf);

    mean = analytic_mean_periodic();
    sprintf(buf, "零模 = %.12f，期望 (100*0.01)/0.1 = 10", mean);
    check("A4", "周期零模 = 平均温度 = 终态值", fabs(mean - 10.0) < 1e-12, buf);
}

/* t=0 的截断级数（不含时间因子），用来单独考察级数本身的收敛性 */
static double series_t0(double x, long K)
{
    long k;
    double sum;

    sum = 0.0;
    for (k = 1; k <= K; k++) {
        sum += analytic_b_dirichlet(k) * sin(analytic_wavenumber(k, 0) * x);
    }
    return sum;
}

/*
 * t=0 截断级数在跳跃点右侧的峰值。
 *
 * 关键：Gibbs 峰到间断点的距离按 l/K 收缩（K=1e5 时只剩 1e-6 量级），
 * 固定步长扫描会把峰整个漏掉。实测用固定 2e-6 步长扫 K=2e5，
 * 读出"峰顶 99.998、过冲 -0.002%"——那是采样伪影，不是物理。
 * 所以窗口取 8*l/K、点数固定 800，步长随 K 自适应。
 */
static double series_t0_peak(long K, long np)
{
    double win;
    double step;
    double v;
    double worst;
    long j;

    win = 8.0 * ROD_LENGTH / (double)K;
    step = win / (double)np;
    worst = series_t0(HOT_LO, K);
    for (j = 1; j < np; j++) {
        v = series_t0(HOT_LO + (double)j * step, K);
        if (v > worst) {
            worst = v;
        }
    }
    return worst;
}

static void report_analytic_series(void)
{
    double n_auto;
    double u_auto;
    double u_double;
    double s1;
    double s2;
    double ext;
    double ideal;
    double m1;
    double m2;
    double e1;
    double e2;
    double mid;
    long K1;
    long K2;
    char buf[192];

    printf("\n== 级数收敛性与 t=0 的 Gibbs 过冲 ==\n");

    /* A5：自动项数够不够——翻倍再比 */
    n_auto = (double)analytic_nmax(SNAP_T1, 1e-12, 0);
    u_auto = analytic_u(ROD_LENGTH / 2.0, SNAP_T1, (long)n_auto, 0);
    u_double = analytic_u(ROD_LENGTH / 2.0, SNAP_T1, (long)(2.0 * n_auto), 0);
    printf("  t=10 s 自动项数 = %.0f；x=l/2 处 %.0f 项 = %.14f，%.0f 项 = %.14f\n",
           n_auto, n_auto, u_auto, 2.0 * n_auto, u_double);
    sprintf(buf, "项数翻倍差 %.3e", fabs(u_auto - u_double));
    check("A5", "级数项数足够（翻倍后不变）",
          fabs(u_auto - u_double) < 1e-12, buf);

    /*
     * A6：Gibbs 过冲。截断级数的峰值 S(K) = S_inf + c/K + O(1/K^2)，
     * 取两个 K 做 Richardson 外推把 1/K 项消掉，再和理论值比。
     * 直接拿单个 K 的读数卡容差，等于把级数截断误差算进物理常数。
     */
    K1 = 10000;
    K2 = 40000;
    s1 = 100.0 * (series_t0_peak(K1, 800) / HOT_TEMP - 1.0);
    s2 = 100.0 * (series_t0_peak(K2, 800) / HOT_TEMP - 1.0);
    ext = (s1 * (double)K1 - s2 * (double)K2) / ((double)K1 - (double)K2);
    /* Si(pi)/pi - 1/2 = 0.089489872236084，由 sin(t)/t 数值积分独立算得 */
    ideal = 8.9489872236084;
    printf("  过冲 K=%-6ld: %.6f%%   K=%-6ld: %.6f%%   1/K 外推: %.6f%%   理论: %.6f%%\n",
           K1, s1, K2, s2, ext, ideal);
    printf("  两档读数之差 %.6f 个百分点；外推后与理论差 %.6f 个百分点\n",
           s1 - s2, ext - ideal);
    sprintf(buf, "外推 %.6f%% vs 理论 %.6f%%", ext, ideal);
    check("A6", "t=0 过冲收敛到 Wilbraham–Gibbs 常数",
          fabs(ext - ideal) < 0.0005, buf);

    /* A7：跳跃点本身收敛到跳变中值，且误差严格按 1/K 缩小 */
    mid = (HOT_TEMP + COLD_TEMP) / 2.0;
    m1 = series_t0(HOT_LO, K1);
    m2 = series_t0(HOT_LO, K2);
    e1 = fabs(m1 - mid);
    e2 = fabs(m2 - mid);
    printf("  t=0, x=0.045（跳跃处）：K=%ld 读 %.6f，K=%ld 读 %.6f，跳变中值 %.1f\n",
           K1, m1, K2, m2, mid);
    printf("  误差 %.4e -> %.4e，比值 %.4f（1/K 律应为 %.1f）\n",
           e1, e2, e1 / e2, (double)K2 / (double)K1);
    sprintf(buf, "K=%ld 读 %.6f，误差比 %.4f vs 期望 %.1f",
            K2, m2, e1 / e2, (double)K2 / (double)K1);
    check("A7", "跳跃点收敛到跳变中值，误差按 1/K",
          e2 < 0.01 && fabs(e1 / e2 - (double)K2 / (double)K1) < 0.05, buf);

    /* A8：终态 */
    printf("  t=10000 s：Dirichlet u(l/2) = %.3e（终态 0），周期 u(l/2) = %.12f（终态 10）\n",
           analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0),
           analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1));
    sprintf(buf, "|u_D(1e4)| = %.3e，|u_P(1e4)-10| = %.3e",
            fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0)),
            fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1) - 10.0));
    check("A8", "解析解的终态：Dirichlet 归 0、周期归 10",
          fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 0)) < 1e-12
              && fabs(analytic_u(ROD_LENGTH / 2.0, 10000.0, 0, 1) - 10.0) < 1e-12, buf);
}

static void report_analytic_vs_ftcs(void)
{
    double dxs[2];
    const char *names[2];
    double snaps[3];
    long i;
    long j;
    double err;
    double rms;
    BoundaryKind bc;

    dxs[0] = DEFAULT_DX;
    dxs[1] = COARSE_DX;
    names[0] = "dx=0.1cm";
    names[1] = "dx=0.2cm";
    snaps[0] = SNAP_T1;
    snaps[1] = SNAP_T2;
    snaps[2] = SNAP_T3;

    printf("\n== FTCS 数值解 vs 解析解（dt = %.4f s，逐点最大误差 / RMS，摄氏度）==\n",
           DEFAULT_DT);
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 3; j++) {
            bc = BC_DIRICHLET;
            err = ftcs_max_error(dxs[i], DEFAULT_DT, snaps[j], bc, &rms);
            printf("  %-9s Dirichlet t=%5.1f s  max|err| = %11.5e   rms = %11.5e\n",
                   names[i], snaps[j], err, rms);
            bc = BC_PERIODIC;
            err = ftcs_max_error(dxs[i], DEFAULT_DT, snaps[j], bc, &rms);
            printf("  %-9s 周期      t=%5.1f s  max|err| = %11.5e   rms = %11.5e\n",
                   names[i], snaps[j], err, rms);
        }
    }
    printf("  误差量级 1e-3 ~ 1e-2：这是格式误差，不是 bug——\n");
    printf("  解析解是连续 PDE 的精确解，FTCS 只有一阶时间 + 二阶空间精度。\n");
}

/*
 * 纯单模初值下 FTCS 与精确 PDE 解的最大误差。
 * 初值本身就是差分算子的特征函数，所以没有"初值表示误差"，
 * 读数只剩时间离散 + 空间离散两项，收敛阶才读得干净。
 */
static double ftcs_mode_max_error(double dx_req, double dt, double t,
                                  BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs s;
    double q;
    double ref;
    double err;
    double worst;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    q = analytic_wavenumber(modek, periodic);
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt) != 0) {
        grid_free(&g);
        return -1.0;
    }
    if (ftcs_advance(&s, t) != 0) {
        grid_free(&g);
        return -1.0;
    }
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        ref = exp(-material_alpha() * q * q * t)
              * (periodic ? cos(q * g.x[i]) : sin(q * g.x[i]));
        err = g.u[i] - ref;
        if (fabs(err) > worst) {
            worst = fabs(err);
        }
    }
    ftcs_free(&s);
    grid_free(&g);
    return worst;
}

/*
 * 同一 dx、两种 dt（dt 与 dt/2）的两个数值解之差。
 *
 * 为什么要这样测：FTCS 的稳定性把 dt 和 dx^2 绑在一起，于是
 *   时间误差 / 空间误差 = 12 alpha dt / dx^2 = 12 r <= 6
 * 与模态、网格都无关。也就是说在固定 dx 下，时间误差最多只有空间
 * 误差的 6 倍，细化 dt 必然撞上空间地板，"与精确解比"读不出干净阶数。
 * 两个同网格解相减，空间误差完全相同、自动抵消，剩下的就是纯时间误差。
 */
static double ftcs_time_diff(double dx_req, double dt, double t,
                             BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs s;
    double q;
    double *ua;
    double worst;
    double err;
    long i;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    q = analytic_wavenumber(modek, periodic);

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    ua = (double *)malloc((size_t)g.nn * sizeof(double));
    if (ua == NULL) {
        grid_free(&g);
        return -1.0;
    }

    /* 第一次：dt */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt) != 0
        || ftcs_advance(&s, t) != 0) {
        free(ua);
        grid_free(&g);
        return -1.0;
    }
    memcpy(ua, g.u, (size_t)g.nn * sizeof(double));
    ftcs_free(&s);

    /* 第二次：dt/2，同一网格、同一初值 */
    for (i = 0; i < g.nn; i++) {
        g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
    }
    if (ftcs_init(&s, &g, bc, 0.0, material_alpha(), dt / 2.0) != 0
        || ftcs_advance(&s, t) != 0) {
        free(ua);
        grid_free(&g);
        return -1.0;
    }
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        err = fabs(g.u[i] - ua[i]);
        if (err > worst) {
            worst = err;
        }
    }
    ftcs_free(&s);
    free(ua);
    grid_free(&g);
    return worst;
}

/*
 * 离散 FV 初值当作分段常数函数时的精确正弦系数：
 *   FV(x) = u_i 于控制体 [x_i-dx/2, x_i+dx/2] ∩ [0,l]
 *   b_k^FV = (2/l) sum_i u_i [cos(q a_i) - cos(q c_i)] / q
 *
 * 有了它就能把"数值 vs 解析"的总误差拆成两块：
 *   格式误差   = FTCS(FV初值) - 精确演化(FV初值)   同一份初值，只差格式
 *   表示误差   = 精确演化(FV初值) - 精确演化(方波)  同一套解析演化，只差初值
 * 两者相加正好是总误差。
 */
static double fv_coeff_dirichlet(const Grid *g, long k)
{
    double q;
    double a;
    double b;
    double sum;
    long i;

    q = analytic_wavenumber(k, 0);
    sum = 0.0;
    for (i = 0; i < g->nn; i++) {
        a = g->x[i] - g->dx / 2.0;
        b = g->x[i] + g->dx / 2.0;
        if (a < 0.0) {
            a = 0.0;
        }
        if (b > g->length) {
            b = g->length;
        }
        sum += g->u[i] * (cos(q * a) - cos(q * b)) / q;
    }
    return (2.0 / ROD_LENGTH) * sum;
}

/* 用给定系数集在节点上求 t 时刻的精确演化值 */
static void evolve_coeffs(const Grid *g, const double *b, long nb, double t,
                          double *out)
{
    long i;
    long k;
    double q;
    double v;

    for (i = 0; i < g->nn; i++) {
        v = 0.0;
        for (k = 1; k <= nb; k++) {
            q = analytic_wavenumber(k, 0);
            v += b[k - 1] * sin(q * g->x[i]) * exp(-q * q * IRON_ALPHA * t);
        }
        out[i] = v;
    }
}

#define DECOMP_KMAX 64
#define DECOMP_NMAX 512

static void report_error_decomposition(void)
{
    static double bp[DECOMP_KMAX];
    double bf[DECOMP_KMAX];
    double a_ex[DECOMP_NMAX];
    double a_fv[DECOMP_NMAX];
    Grid g;
    Ftcs s;
    double dts[4];
    double dxs[3];
    double fmt;
    double rep;
    double tot;
    double fmt_max;
    double rep_max;
    double tot_max;
    double rep_at_aligned;
    double dt_i;
    double last_sign;
    double first_sign;
    int sign_changed;
    long i;
    long j;
    long k;
    long nb;
    long worst_i;
    int rc;
    char buf[192];

    printf("\n== 总误差的分解：格式误差 vs 初值表示误差（Dirichlet, t = %.0f s）==\n",
           SNAP_T1);

    nb = DECOMP_KMAX;
    for (k = 1; k <= nb; k++) {
        bp[k - 1] = analytic_b_dirichlet(k);
    }

    printf("  %8s %8s %14s %14s %14s %10s\n",
           "dx", "dt", "格式误差max", "表示误差max", "总误差max", "表示占比");
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    rep_at_aligned = -1.0;
    for (i = 0; i < 3; i++) {
        /* dt 随 dx^2 缩，保持 r = 0.23：直接用 DEFAULT_DT 的话粗网格
           稳定、细网格反而 r>1/2 发散，三档之间根本没有可比性 */
        dt_i = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT)
               * dxs[i] * dxs[i] / material_alpha();
        memset(&g, 0, sizeof(Grid));
        if (grid_init(&g, ROD_LENGTH, dxs[i]) != 0) {
            return;
        }
        if (g.nn > DECOMP_NMAX) {
            printf("      dx=%.4f 节点数 %ld 超出分解数组，跳过\n", dxs[i], g.nn);
            grid_free(&g);
            continue;
        }
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        for (k = 1; k <= nb; k++) {
            bf[k - 1] = fv_coeff_dirichlet(&g, k);
        }
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt_i) != 0) {
            grid_free(&g);
            return;
        }
        rc = ftcs_advance(&s, SNAP_T1);
        if (rc != 0) {
            printf("      dx=%.4f 推进失败 rc=%d\n", dxs[i], rc);
            ftcs_free(&s);
            grid_free(&g);
            continue;
        }
        evolve_coeffs(&g, bp, nb, SNAP_T1, a_ex);
        evolve_coeffs(&g, bf, nb, SNAP_T1, a_fv);

        tot_max = 0.0;
        fmt_max = 0.0;
        rep_max = 0.0;
        worst_i = 0;
        for (j = 0; j < g.nn; j++) {
            tot = fabs(g.u[j] - a_ex[j]);
            if (tot > tot_max) {
                tot_max = tot;
                worst_i = j;
            }
            fmt = fabs(g.u[j] - a_fv[j]);
            if (fmt > fmt_max) {
                fmt_max = fmt;
            }
            rep = fabs(a_fv[j] - a_ex[j]);
            if (rep > rep_max) {
                rep_max = rep;
            }
        }
        printf("  %8.4f %8.5f %14.4e %14.4e %14.4e %9.1f%%\n",
               dxs[i], dt_i, fmt_max, rep_max, tot_max,
               100.0 * rep_max / (rep_max + fmt_max));
        if (i == 0) {
            rep_at_aligned = rep_max;
        }
        if (i < 3) {
            demo_dx[i] = dxs[i];
            demo_fmt[i] = fmt_max;
            demo_rep[i] = rep_max;
            demo_tot[i] = tot_max;
            demo_n = i + 1;
        }
        ftcs_free(&s);
        grid_free(&g);
    }
    printf("  dx=0.002 那一档表示误差恒等于 0：高温段边界 0.045/0.055 正好落在单元\n");
    printf("  边界上，分段常数的 FV 重构与精确方波是同一个函数，系数逐位相同。\n");
    printf("  dx=0.001 时边界落在节点中心，重构多出两个 50 度的半格，表示误差比\n");
    printf("  格式误差大一个量级——总误差由初值表示主导，而不是由格式主导。\n");

    sprintf(buf, "dx=0.002 表示误差 = %.3e", rep_at_aligned);
    check("A13", "边界与单元对齐时初值表示误差恒为 0",
          rep_at_aligned >= 0.0 && rep_at_aligned < 1e-10, buf);

    /* 格式误差随 dt 变号：时间项过阻尼（+）、空间项欠阻尼（-），两者反号 */
    printf("\n  dx = 0.001 m 固定，扫 dt 看格式误差变号：\n");
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    for (k = 1; k <= nb; k++) {
        bf[k - 1] = fv_coeff_dirichlet(&g, k);
    }
    evolve_coeffs(&g, bp, nb, SNAP_T1, a_ex);
    evolve_coeffs(&g, bf, nb, SNAP_T1, a_fv);

    rep = 0.0;
    worst_i = 0;
    for (j = 0; j < g.nn; j++) {
        if (fabs(a_fv[j] - a_ex[j]) > fabs(rep)) {
            rep = a_fv[j] - a_ex[j];
            worst_i = j;
        }
    }
    printf("      表示误差在 x=%.4f 处 = %+.6e（与 dt 无关）\n",
           g.x[worst_i], rep);
    printf("      %8s %8s %16s %16s\n", "dt", "r", "该点格式误差", "该点总误差");
    first_sign = 0.0;
    last_sign = 0.0;
    sign_changed = 0;
    for (i = 0; i < 4; i++) {
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        if (ftcs_init(&s, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dts[i]) != 0) {
            continue;
        }
        if (ftcs_advance(&s, SNAP_T1) != 0) {
            printf("      dt=%.5f 推进失败（可能 r>1/2）\n", dts[i]);
            ftcs_free(&s);
            continue;
        }
        fmt = g.u[worst_i] - a_fv[worst_i];
        printf("      %8.5f %8.4f %+16.6e %+16.6e   %s\n",
               dts[i], s.r, fmt, g.u[worst_i] - a_ex[worst_i],
               (fmt * rep > 0.0) ? "与表示误差同号" : "与表示误差反号(抵消)");
        if (fmt != 0.0) {
            if (first_sign == 0.0) {
                first_sign = fmt;
            }
            if (last_sign != 0.0 && fmt * last_sign < 0.0) {
                sign_changed = 1;
            }
            last_sign = fmt;
        }
        ftcs_free(&s);
    }
    grid_free(&g);
    printf("      格式误差在 dt 扫描中%s变号：时间项与空间项反号，所以固定 dx\n",
           sign_changed ? "" : "没有");
    printf("      细化 dt 时总误差不是干净地按 2 倍下降，而是先穿过一个\n");
    printf("      \"格式误差本身几乎为零\"的点（见上面收敛阶 (4) 的 2.30/2.85/1.20）。\n");

    sprintf(buf, "变号 = %d，首末 %+.3e -> %+.3e",
            sign_changed, first_sign, last_sign);
    check("A14", "格式误差随 dt 变号（时间项与空间项反号）",
          sign_changed == 1, buf);
}

static void report_convergence_order(void)
{
    double dts[4];
    double dxs[3];
    double errs[4];
    double e0;
    double e1;
    double e2;
    double r_fixed;
    double dx_fine;
    double p0;
    double p1;
    double p2;
    long i;
    char buf[192];

    printf("\n== 收敛阶 ==\n");
    printf("  方波初值同时带着三种误差：时间离散 O(dt)、空间离散 O(dx^2)、\n");
    printf("  以及\"离散初值 vs 精确方波\"的表示误差。三者混在一起，比值不干净。\n");
    printf("  所以先用纯单模初值把格式阶数读干净，再回到方波如实报告。\n");

    /* ---- (1) 时间一阶：同网格两种 dt 相减，空间误差自动抵消 ---- */
    dx_fine = DEFAULT_DX;
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    printf("\n  (1) 纯模 Dirichlet k=3，dx = %.4f m 固定，同网格 dt 与 dt/2 相减：\n",
           dx_fine);
    printf("      时间误差/空间误差 = 12r；dt=%.4f 时 r=%.4f，只差 %.2f 倍，\n",
           DEFAULT_DT, material_fourier_number(material_alpha(), dx_fine, DEFAULT_DT),
           12.0 * material_fourier_number(material_alpha(), dx_fine, DEFAULT_DT));
    printf("      所以\"与精确解比\"读不出时间阶，必须同网格相减。\n");
    printf("  %10s %10s %16s\n", "dt/s", "r", "|u(dt)-u(dt/2)|");
    for (i = 0; i < 4; i++) {
        errs[i] = ftcs_time_diff(dx_fine, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %10.5f %10.5f %16.6e", dts[i],
               material_fourier_number(material_alpha(), dx_fine, dts[i]),
               errs[i]);
        if (i > 0) {
            printf("   相邻比 %.4f", errs[i - 1] / errs[i]);
        }
        printf("\n");
    }
    sprintf(buf, "相邻比 %.4f / %.4f / %.4f",
            errs[0] / errs[1], errs[1] / errs[2], errs[2] / errs[3]);
    check("A9", "时间一阶收敛（dt 减半差值减半）",
          fabs(errs[0] / errs[1] - 2.0) < 0.02
              && fabs(errs[1] / errs[2] - 2.0) < 0.02
              && fabs(errs[2] / errs[3] - 2.0) < 0.02, buf);

    /* ---- (2) 空间二阶：r 固定，dt = r dx^2/alpha 随 dx^2 一起缩 ---- */
    r_fixed = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT);
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    printf("\n  (2) 纯模 Dirichlet k=3，r 固定 = %.4f，dx 减半：\n", r_fixed);
    e0 = ftcs_mode_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    e1 = ftcs_mode_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    e2 = ftcs_mode_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                             SNAP_T1, BC_DIRICHLET, 3);
    printf("      dx=0.0020 err=%.6e | dx=0.0010 err=%.6e | dx=0.0005 err=%.6e\n",
           e0, e1, e2);
    printf("      误差比 %.4f 与 %.4f（二阶应为 4.000）\n", e0 / e1, e1 / e2);
    sprintf(buf, "误差比 %.4f / %.4f", e0 / e1, e1 / e2);
    check("A10", "纯模下空间二阶收敛（dx 减半误差降 4 倍）",
          fabs(e0 / e1 - 4.0) < 0.2 && fabs(e1 / e2 - 4.0) < 0.2, buf);

    /* ---- (3) 周期边界同样跑 ---- */
    e0 = ftcs_mode_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    e1 = ftcs_mode_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    e2 = ftcs_mode_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                             SNAP_T1, BC_PERIODIC, 1);
    printf("  (3) 纯模 周期 k=1，r 固定：err=%.6e / %.6e / %.6e，误差比 %.4f 与 %.4f\n",
           e0, e1, e2, e0 / e1, e1 / e2);
    sprintf(buf, "周期误差比 %.4f", e1 / e2);
    check("A11", "周期边界同样二阶", fabs(e1 / e2 - 4.0) < 0.2, buf);

    /* ---- (4) 物理方波：如实报告，比值不干净是三种误差混在一起 ---- */
    printf("\n  (4) 物理方波初值（dx = 0.001 m），dt 减半：\n");
    for (i = 0; i < 4; i++) {
        errs[i] = ftcs_max_error(DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, NULL);
        printf("      dt=%.5f err=%.6e%s\n", dts[i], errs[i],
               i > 0 ? "" : "");
        if (i > 0) {
            printf("               相邻比 %.4f", errs[i - 1] / errs[i]);
            if (i == 3) {
                printf("  <- 见上面的误差分解：格式误差在此已与表示误差反号抵消");
            }
        }
        printf("\n");
    }
    printf("  (5) 物理方波初值，r 固定 dx 减半：\n");
    p0 = ftcs_max_error(dxs[0], r_fixed * dxs[0] * dxs[0] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    p1 = ftcs_max_error(dxs[1], r_fixed * dxs[1] * dxs[1] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    p2 = ftcs_max_error(dxs[2], r_fixed * dxs[2] * dxs[2] / material_alpha(),
                        SNAP_T1, BC_DIRICHLET, NULL);
    printf("      dx=0.0020 err=%.6e | dx=0.0010 err=%.6e | dx=0.0005 err=%.6e\n",
           p0, p1, p2);
    printf("      误差比 %.4f 与 %.4f\n", p0 / p1, p1 / p2);
    printf("      dx=0.002 那一档反而更小，这不是二阶失效——见上面的误差分解：\n");
    printf("      该档高温段边界正好落在单元边界上，初值表示误差恒等于 0，\n");
    printf("      总误差只剩格式误差；而 dx=0.001 的总误差由表示误差主导。\n");
    printf("      纯模的 (2) 里没有初值误差，同一组 dx 才给出干净的 4.00。\n");

    /* 方波误差随 t 单调下降：活着的高频模越来越少 */
    e0 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_DIRICHLET, NULL);
    e1 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T2, BC_DIRICHLET, NULL);
    e2 = ftcs_max_error(DEFAULT_DX, DEFAULT_DT, SNAP_T3, BC_DIRICHLET, NULL);
    printf("  (6) 方波误差随时间下降：t=10 %.4e -> 20 %.4e -> 30 %.4e\n",
           e0, e1, e2);
    sprintf(buf, "%.3e -> %.3e -> %.3e", e0, e1, e2);
    check("A12", "方波误差随 t 单调下降（高频模先死掉）",
          e0 > e1 && e1 > e2, buf);
}

/* ================= 步骤 4：Crank-Nicolson 对照 ================= */

/*
 * 两个格式共用的误差测量。which: 0 = FTCS，1 = Crank-Nicolson。
 * modek <= 0 用物理方波初值；modek > 0 用纯单模初值（去掉初值表示误差）。
 * 返回负数表示推进失败（例如 FTCS 的 r > 1/2）。
 */
static double scheme_error(int which, double dx_req, double dt, double t,
                           BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    double q;
    double ref;
    double err;
    double worst;
    long i;
    int periodic;
    int rc;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    if (modek <= 0) {
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    } else {
        q = analytic_wavenumber(modek, periodic);
        for (i = 0; i < g.nn; i++) {
            g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
        }
    }

    if (which == 0) {
        if (ftcs_init(&fs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1.0;
        }
        rc = ftcs_advance(&fs, t);
    } else {
        if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
            grid_free(&g);
            return -1.0;
        }
        rc = crank_advance(&cs, t);
    }
    if (rc != 0) {
        if (which == 0) {
            ftcs_free(&fs);
        } else {
            crank_free(&cs);
        }
        grid_free(&g);
        return -1.0;
    }

    q = (modek > 0) ? analytic_wavenumber(modek, periodic) : 0.0;
    worst = 0.0;
    for (i = 0; i < g.nn; i++) {
        if (modek <= 0) {
            ref = analytic_u(g.x[i], t, 0, periodic);
        } else {
            ref = exp(-material_alpha() * q * q * t)
                  * (periodic ? cos(q * g.x[i]) : sin(q * g.x[i]));
        }
        err = fabs(g.u[i] - ref);
        if (err > worst) {
            worst = err;
        }
    }
    if (which == 0) {
        ftcs_free(&fs);
    } else {
        crank_free(&cs);
    }
    grid_free(&g);
    return worst;
}

/*
 * 同一 dx、dt 与 dt/2 两个数值解之差。
 * 空间离散误差与初值表示误差在两边完全相同，相减自动抵消，
 * 读数就是纯时间误差，才能干净地判时间收敛阶。
 */
static double scheme_time_diff(int which, double dx_req, double dt, double t,
                               BoundaryKind bc, long modek)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    double q;
    double *ua;
    double worst;
    double err;
    double step_dt;
    long i;
    int pass;
    int periodic;

    periodic = (bc == BC_PERIODIC);
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return -1.0;
    }
    ua = (double *)malloc((size_t)g.nn * sizeof(double));
    if (ua == NULL) {
        grid_free(&g);
        return -1.0;
    }
    worst = -1.0;

    for (pass = 0; pass < 2; pass++) {
        step_dt = (pass == 0) ? dt : dt / 2.0;

        if (modek <= 0) {
            grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
        } else {
            q = analytic_wavenumber(modek, periodic);
            for (i = 0; i < g.nn; i++) {
                g.u[i] = periodic ? cos(q * g.x[i]) : sin(q * g.x[i]);
            }
        }

        if (which == 0) {
            if (ftcs_init(&fs, &g, bc, COLD_TEMP, material_alpha(), step_dt) != 0
                || ftcs_advance(&fs, t) != 0) {
                free(ua);
                grid_free(&g);
                return -1.0;
            }
            ftcs_free(&fs);
        } else {
            if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(),
                           step_dt) != 0
                || crank_advance(&cs, t) != 0) {
                free(ua);
                grid_free(&g);
                return -1.0;
            }
            crank_free(&cs);
        }

        if (pass == 0) {
            memcpy(ua, g.u, (size_t)g.nn * sizeof(double));
        } else {
            worst = 0.0;
            for (i = 0; i < g.nn; i++) {
                err = fabs(g.u[i] - ua[i]);
                if (err > worst) {
                    worst = err;
                }
            }
        }
    }
    free(ua);
    grid_free(&g);
    return worst;
}

/* CN 推到 t 后的最大最小值。返回 0 表示成功跑完 */
static int crank_extremes(double dx_req, double dt, double t,
                          BoundaryKind bc, double *pmax, double *pmin)
{
    Grid g;
    Crank cs;
    int periodic;
    int rc;

    periodic = (bc == BC_PERIODIC);
    *pmax = 0.0;
    *pmin = 0.0;
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, dx_req) != 0) {
        return 1;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    if (crank_init(&cs, &g, bc, COLD_TEMP, material_alpha(), dt) != 0) {
        grid_free(&g);
        return 2;
    }
    rc = crank_advance(&cs, t);
    if (rc == 0 && !crank_all_finite(&cs)) {
        rc = 3;
    }
    if (rc == 0) {
        *pmax = field_max(&g);
        *pmin = field_min(&g);
    }
    crank_free(&cs);
    grid_free(&g);
    return rc;
}

static void report_crank_mode(void)
{
    Grid g;
    Crank cs;
    double q;
    double gfac;
    double gcont;
    double u0ref;
    double measured;
    double theory;
    double msteps;
    double mcap;
    long c;
    long i;
    long ref;
    long ks[3];
    int pers[3];
    const char *labels[3];
    char buf[192];

    printf("\n== CN 的离散特征值：G = (1 - 2r s^2)/(1 + 2r s^2) ==\n");
    ks[0] = 1;  pers[0] = 0; labels[0] = "Dirichlet k=1 ";
    ks[1] = 99; pers[1] = 0; labels[1] = "Dirichlet k=99";
    ks[2] = 1;  pers[2] = 1; labels[2] = "周期     k=1 ";

    for (c = 0; c < 3; c++) {
        memset(&g, 0, sizeof(Grid));
        if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
            return;
        }
        q = analytic_wavenumber(ks[c], pers[c]);
        for (i = 0; i < g.nn; i++) {
            g.u[i] = pers[c] ? cos(q * g.x[i]) : sin(q * g.x[i]);
        }
        if (crank_init(&cs, &g, pers[c] ? BC_PERIODIC : BC_DIRICHLET,
                       0.0, material_alpha(), DEFAULT_DT) != 0) {
            grid_free(&g);
            return;
        }
        ref = g.n / 2;
        u0ref = g.u[ref];
        gfac = crank_growth_factor(cs.r, q, g.dx);
        gcont = exp(-material_alpha() * q * q * DEFAULT_DT);

        /* 和 FTCS 一样防下溢：|G|^m 不能掉到伪模的 1e-16 底下 */
        msteps = 1000.0;
        if (fabs(gfac) < 1.0 && fabs(gfac) > 0.0) {
            mcap = log(1e-6) / log(fabs(gfac));
            if (mcap < 1.0) {
                mcap = 1.0;
            }
            if (msteps > mcap) {
                msteps = floor(mcap);
            }
        }
        for (i = 0; i < (long)msteps; i++) {
            crank_step(&cs);
        }
        measured = g.u[ref] / u0ref;
        theory = pow(gfac, msteps);

        printf("  %s  q = %10.4f  G = %+.14f  连续每步 = %.14f\n",
               labels[c], q, gfac, gcont);
        printf("      走 %.0f 步：实测 G^m = %.14e  理论 = %.14e  相对差 %.3e\n",
               msteps, measured, theory, fabs(measured / theory - 1.0));
        printf("      离散/连续 = %.14f（%+.4e %%）；同 r 下 FTCS 是 %+.14f\n",
               gfac / gcont, 100.0 * (gfac / gcont - 1.0),
               ftcs_growth_factor(cs.r, q, g.dx));
        sprintf(buf, "%s 相对差 %.3e", labels[c], fabs(measured / theory - 1.0));
        check("C1", "CN 单模衰减等于其离散特征值 G^m",
              theory > 0.0 && fabs(measured / theory - 1.0) < 1e-9, buf);
        crank_free(&cs);
        grid_free(&g);
    }
}

static void report_crank_stability(void)
{
    double rs[6];
    double dt;
    long i;
    long nstep;
    double mx;
    double mn;
    double e_cn;
    double e_ft;
    int rc;
    int all_ok;
    char buf[192];

    printf("\n== CN 无条件稳定，但不无条件单调 ==\n");
    printf("  %8s %10s %7s %11s %11s %13s %13s\n",
           "r", "dt/s", "步数", "max u", "min u", "CN 误差", "FTCS 同 r");
    rs[0] = 0.23;
    rs[1] = 0.5;
    rs[2] = 1.0;
    rs[3] = 5.0;
    rs[4] = 50.0;
    rs[5] = 500.0;
    all_ok = 1;
    for (i = 0; i < 6; i++) {
        dt = rs[i] * DEFAULT_DX * DEFAULT_DX / material_alpha();
        nstep = (long)ceil(SNAP_T1 / dt);
        rc = crank_extremes(DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, &mx, &mn);
        if (rc != 0) {
            all_ok = 0;
        }
        e_cn = scheme_error(1, DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, 0);
        printf("  %8.4f %10.5f %7ld %11.6f %11.6f %13.4e",
               rs[i], dt, nstep, mx, mn, e_cn);
        if (rs[i] <= FTCS_R_MAX) {
            e_ft = scheme_error(0, DEFAULT_DX, dt, SNAP_T1, BC_DIRICHLET, 0);
            printf("   FTCS %.4e", e_ft);
        } else {
            printf("   FTCS 已发散");
        }
        printf("\n");
    }
    printf("  max u 不超过 100、min u 不小于 0 只在 r 小的那几档成立。\n");
    printf("  r 一大，右端矩阵 B 的对角权重 1-r 变负，陡峭前沿下方被打出\n");
    printf("  负温度。CN 无条件稳定，但离散极值原理不再成立——这是它换来\n");
    printf("  的东西，不是免费午餐。FTCS 在 r<=1/2 时反过来两者都保。\n");

    sprintf(buf, "r=500（1 步走完 10 s）max=%.4f min=%.4f，有限", mx, mn);
    check("C2", "CN 在大 r 下仍稳定，不出现 NaN/inf",
          all_ok && fabs(mx) < 1e30 && fabs(mn) < 1e30, buf);
}

static void report_crank_order(void)
{
    double dts[5];
    double big[4];
    double d_ft[5];
    double d_cn[5];
    double e_ft[5];
    double e_cn[5];
    double r_fix;
    double dxs[3];
    double c0;
    double c1;
    double c2;
    double rr;
    long i;
    char buf[192];

    printf("\n== CN 的时间二阶：同网格 dt 与 dt/2 相减（纯模 Dirichlet k=3）==\n");
    dts[0] = 0.02;
    dts[1] = 0.01;
    dts[2] = 0.005;
    dts[3] = 0.0025;
    dts[4] = 0.00125;
    printf("  %9s %8s %15s %15s %10s %10s\n",
           "dt/s", "r", "CN 差值", "FTCS 差值", "CN 比", "FTCS 比");
    for (i = 0; i < 5; i++) {
        d_cn[i] = scheme_time_diff(1, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        d_ft[i] = scheme_time_diff(0, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %9.5f %8.4f %15.6e %15.6e", dts[i],
               material_fourier_number(material_alpha(), DEFAULT_DX, dts[i]),
               d_cn[i], d_ft[i]);
        if (i > 0) {
            printf(" %10.4f %10.4f", d_cn[i - 1] / d_cn[i], d_ft[i - 1] / d_ft[i]);
        }
        printf("\n");
    }
    printf("  CN 相邻比趋近 4（二阶），FTCS 趋近 2（一阶）。\n");
    sprintf(buf, "CN 比 %.4f / %.4f / %.4f / %.4f",
            d_cn[0] / d_cn[1], d_cn[1] / d_cn[2],
            d_cn[2] / d_cn[3], d_cn[3] / d_cn[4]);
    check("C3", "CN 时间二阶收敛（dt 减半差值降 4 倍）",
          fabs(d_cn[0] / d_cn[1] - 4.0) < 0.05
              && fabs(d_cn[1] / d_cn[2] - 4.0) < 0.05
              && fabs(d_cn[2] / d_cn[3] - 4.0) < 0.05
              && fabs(d_cn[3] / d_cn[4] - 4.0) < 0.05, buf);

    printf("\n== 绝对误差（纯模 k=3，vs 精确 PDE 解，t = %.0f s）==\n", SNAP_T1);
    printf("  %9s %8s %15s %15s %12s\n", "dt/s", "r", "CN err", "FTCS err", "FTCS/CN");
    for (i = 0; i < 5; i++) {
        rr = material_fourier_number(material_alpha(), DEFAULT_DX, dts[i]);
        e_cn[i] = scheme_error(1, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3);
        e_ft[i] = (rr <= FTCS_R_MAX)
                  ? scheme_error(0, DEFAULT_DX, dts[i], SNAP_T1, BC_DIRICHLET, 3)
                  : -1.0;
        printf("  %9.5f %8.4f %15.6e", dts[i], rr, e_cn[i]);
        if (e_ft[i] > 0.0) {
            printf(" %15.6e %11.2fx", e_ft[i], e_ft[i] / e_cn[i]);
        } else {
            printf(" %15s %12s", "(发散)", "");
        }
        printf("\n");
    }
    printf("  CN 这一列对 dt 几乎不动（%.4e ~ %.4e，只动 %.1f%%）：绝对误差已经\n",
           e_cn[4], e_cn[0], 100.0 * (e_cn[0] / e_cn[4] - 1.0));
    printf("  被 dx^2 的空间地板钉住，时间误差小到看不见。FTCS 反过来在 r=0.23\n");
    printf("  附近凹下去——那正是步骤 3 实测的时间项与空间项反号抵消点。所以按\n");
    printf("  绝对误差比，FTCS 在某些 dt 上反而更小；那是巧合，不是格式性质。\n");
    printf("  要比阶，只能比上面那张同网格 dt 与 dt/2 相减的纯时间误差表。\n");
    sprintf(buf, "dt=%.5f 纯时间误差 FTCS/CN = %.0f 倍", dts[1],
            d_ft[1] / d_cn[1]);
    check("C4", "同 dt 下 CN 的纯时间误差远小于 FTCS",
          d_cn[1] > 0.0 && d_ft[1] / d_cn[1] > 1000.0, buf);

    /* CN 能走到 FTCS 根本到不了的大步长，二阶阶数还保不保？ */
    big[0] = 0.2;
    big[1] = 0.1;
    big[2] = 0.05;
    big[3] = 0.025;
    printf("\n== CN 单独走大步（这些 dt 上 FTCS 的 r 早已超过 1/2）==\n");
    printf("  %9s %10s %15s %12s\n", "dt/s", "r", "CN 差值", "相邻比");
    for (i = 0; i < 4; i++) {
        d_cn[i] = scheme_time_diff(1, DEFAULT_DX, big[i], SNAP_T1, BC_DIRICHLET, 3);
        printf("  %9.4f %10.4f %15.6e", big[i],
               material_fourier_number(material_alpha(), DEFAULT_DX, big[i]),
               d_cn[i]);
        if (i > 0) {
            printf(" %12.4f", d_cn[i - 1] / d_cn[i]);
        }
        printf("\n");
    }
    sprintf(buf, "dt=0.1/0.05/0.025 相邻比 %.4f / %.4f",
            d_cn[1] / d_cn[2], d_cn[2] / d_cn[3]);
    check("C7", "CN 在大步长（r 达 11.5）下仍可测且保持二阶",
          d_cn[0] > 0.0 && fabs(d_cn[1] / d_cn[2] - 4.0) < 0.1
              && fabs(d_cn[2] / d_cn[3] - 4.0) < 0.1, buf);

    /* 空间二阶与周期边界 */
    r_fix = material_fourier_number(material_alpha(), DEFAULT_DX, DEFAULT_DT);
    dxs[0] = 0.002;
    dxs[1] = 0.001;
    dxs[2] = 0.0005;
    c0 = scheme_error(1, dxs[0], r_fix * dxs[0] * dxs[0] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    c1 = scheme_error(1, dxs[1], r_fix * dxs[1] * dxs[1] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    c2 = scheme_error(1, dxs[2], r_fix * dxs[2] * dxs[2] / material_alpha(),
                      SNAP_T1, BC_DIRICHLET, 3);
    printf("\n  CN 空间二阶（r 固定 = %.4f，纯模 k=3）：\n", r_fix);
    printf("      %.6e / %.6e / %.6e，误差比 %.4f 与 %.4f（应为 4.00）\n",
           c0, c1, c2, c0 / c1, c1 / c2);
    sprintf(buf, "CN dx 减半误差比 %.4f", c1 / c2);
    check("C5", "CN 空间二阶收敛", fabs(c1 / c2 - 4.0) < 0.2, buf);

    c0 = scheme_time_diff(1, DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_PERIODIC, 1);
    c1 = scheme_time_diff(0, DEFAULT_DX, DEFAULT_DT, SNAP_T1, BC_PERIODIC, 1);
    printf("  周期纯模 k=1，dt=%.4f：纯时间误差 CN = %.6e，FTCS = %.6e，"
           "FTCS/CN = %.0f 倍\n", DEFAULT_DT, c0, c1, c1 / c0);
    sprintf(buf, "周期纯时间误差 CN %.3e vs FTCS %.3e", c0, c1);
    check("C6", "周期边界下 CN 的纯时间误差同样远优于 FTCS",
          c0 > 0.0 && c1 / c0 > 1000.0, buf);
}

static void report_crank_vs_ftcs_pulse(void)
{
    double snaps[3];
    long i;
    double e_cn;
    double e_ft;
    double mean_now;
    double mean0;
    Grid g;
    Crank cs;
    double worst_drift;
    long step;
    long total;
    char buf[192];

    printf("\n== 物理方波：CN 与 FTCS 在交付时刻上的误差（dx=0.001, dt=0.01）==\n");
    snaps[0] = SNAP_T1;
    snaps[1] = SNAP_T2;
    snaps[2] = SNAP_T3;
    printf("  %8s %16s %16s %12s\n", "t/s", "CN max|err|", "FTCS max|err|", "倍差");
    for (i = 0; i < 3; i++) {
        e_cn = scheme_error(1, DEFAULT_DX, DEFAULT_DT, snaps[i], BC_DIRICHLET, 0);
        e_ft = scheme_error(0, DEFAULT_DX, DEFAULT_DT, snaps[i], BC_DIRICHLET, 0);
        printf("  %8.1f %16.6e %16.6e %11.1fx\n", snaps[i], e_cn, e_ft,
               e_ft / e_cn);
    }
    printf("  方波情形 CN 的优势远没有纯模那么大：总误差里约 85%% 是初值表示\n");
    printf("  误差，两个格式共用同一份 FV 初值，这部分谁也消不掉。\n");

    /* CN 的周期均值守恒：1^T A = 1^T 且 1^T B = 1^T，所以必须严格守恒 */
    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 1);
    mean0 = grid_mean_periodic(&g);
    if (crank_init(&cs, &g, BC_PERIODIC, COLD_TEMP,
                   material_alpha(), DEFAULT_DT) != 0) {
        grid_free(&g);
        return;
    }
    worst_drift = 0.0;
    total = (long)(SNAP_T3 / DEFAULT_DT + 0.5);
    for (step = 1; step <= total; step++) {
        crank_step(&cs);
        mean_now = fabs(grid_mean_periodic(&g) - mean0);
        if (mean_now > worst_drift) {
            worst_drift = mean_now;
        }
    }
    printf("  CN 周期 t=30 s（%ld 步）：平均温度最大漂移 %.3e，max/min = %.6f/%.6f\n",
           cs.steps, worst_drift, field_max(&g), field_min(&g));
    printf("  对照 FTCS 同条件：漂移 1.243e-14，max/min = 11.290223/8.710466\n");
    sprintf(buf, "CN 均值漂移 %.3e", worst_drift);
    check("C8", "CN 周期边界平均温度守恒", worst_drift <= 1e-10, buf);
    crank_free(&cs);
    grid_free(&g);
}


/* ============ 步骤 5：收敛判据、调度与数据导出 ============ */

#define CRIT_N 6

/* 副判据的扫描阈值；1e-6 那一档专门用来和主判据同量级地比 */
static const double CRIT_EPS[CRIT_N] = { 1e-4, 1e-6, 1e-8, 1e-9, 1e-11, 1e-13 };

typedef struct {
    double *v;
    long n;
    long cap;
} DArr;

static int darr_init(DArr *a, long cap0)
{
    a->v = (double *)malloc((size_t)cap0 * sizeof(double));
    a->n = 0;
    a->cap = (a->v != NULL) ? cap0 : 0;
    return (a->v != NULL) ? 0 : 1;
}

static int darr_push(DArr *a, double x)
{
    double *nv;

    if (a->n >= a->cap) {
        nv = (double *)realloc(a->v, (size_t)(a->cap * 2 + 1) * sizeof(double));
        if (nv == NULL) {
            return 1;
        }
        a->v = nv;
        a->cap = a->cap * 2 + 1;
    }
    a->v[a->n++] = x;
    return 0;
}

static void darr_free(DArr *a)
{
    free(a->v);
    a->v = NULL;
    a->n = 0;
    a->cap = 0;
}

static double *dup_field(const Grid *g)
{
    double *p;

    p = (double *)malloc((size_t)g->nn * sizeof(double));
    if (p != NULL) {
        memcpy(p, g->u, (size_t)g->nn * sizeof(double));
    }
    return p;
}

typedef struct {
    const char *dxtag;
    const char *bctag;
    double dx;
    int periodic;
    double target;
    double amp;
    double tau;
    double t_pred;
    long steps_conv;
    double t_conv;
    double dev_conv;
    double jump_conv;
    long last_above[CRIT_N];
    long stop_step[CRIT_N];
    double stop_time[CRIT_N];
    double stop_dev[CRIT_N];
    double *u0;
    double *u10;
    double *u20;
    double *u30;
    double *u10dt;
    double *ucv;
    double heat[4];
    double mx[4];
    double mn[4];
    double err[4];
    long nslice;
    int rc;
} Case;

static int run_case(Case *c, const char *outdir)
{
    Grid g;
    Ftcs s;
    Converge cv;
    EvoWriter ew;
    DArr devh;
    double *prev;
    double *uwork;
    double tau;
    double est_steps;
    double ratio;
    double next_slice;
    long i;
    long k;
    long st;
    long s1;
    long s2;
    long s3;
    char name[128];
    char meta[400];
    int periodic;
    BoundaryKind bc;

    periodic = c->periodic;
    bc = periodic ? BC_PERIODIC : BC_DIRICHLET;
    c->rc = 0;
    c->nslice = 0;
    memset(&g, 0, sizeof(Grid));
    memset(&cv, 0, sizeof(Converge));
    memset(&ew, 0, sizeof(EvoWriter));
    if (grid_init(&g, ROD_LENGTH, c->dx) != 0) {
        return 1;
    }
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, periodic);
    c->u0 = dup_field(&g);
    c->target = converge_target(bc, &g);
    c->amp = periodic ? fabs(analytic_a_periodic(1))
                      : fabs(analytic_b_dirichlet(1));
    tau = periodic ? material_tau_periodic(material_alpha(), ROD_LENGTH)
                   : material_tau_dirichlet(material_alpha(), ROD_LENGTH);
    c->tau = tau;
    c->t_pred = converge_predict_time(material_alpha(), ROD_LENGTH,
                                      c->amp, periodic, CONV_EPS);
    /*
     * 几何抽稀：ratio = est_steps^(1/MAX_SLICES)。均匀间隔会把 0..30 s
     * 的快速瞬态抹平（Dirichlet 收敛要七万多步，均匀取就只能给到两秒一片），
     * 而题目要画的恰恰是 10/20/30 s。几何级数早期几乎每步一张、后期越来越疏。
     */
    est_steps = ceil(c->t_pred / DEFAULT_DT * 1.2) + 100.0;
    ratio = pow(est_steps, 1.0 / (double)EXPORT_MAX_SLICES);
    if (ratio < 1.02) {
        ratio = 1.02;
    }
    next_slice = 1.0;

    if (darr_init(&devh, 4096) != 0 || c->u0 == NULL) {
        grid_free(&g);
        return 2;
    }
    prev = (double *)malloc((size_t)g.nn * sizeof(double));
    uwork = (double *)malloc((size_t)g.nn * sizeof(double));
    if (prev == NULL || uwork == NULL) {
        darr_free(&devh);
        grid_free(&g);
        return 3;
    }
    for (k = 0; k < CRIT_N; k++) {
        c->last_above[k] = 0;
    }

    sprintf(name, "evolution_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "完整时空演化 u(x_i,t_n)；dx=%.17g dt=%.17g 边界=%s "
                  "时间按公比 %.6f 几何抽稀（上限 %d 片）",
            g.dx, DEFAULT_DT, c->bctag, ratio, EXPORT_MAX_SLICES);
    if (evo_open(&ew, outdir, name, &g, meta) != 0) {
        free(prev);
        darr_free(&devh);
        grid_free(&g);
        return 4;
    }
    evo_write(&ew, 0.0, g.u);
    c->nslice++;

    if (ftcs_init(&s, &g, bc, COLD_TEMP, material_alpha(), DEFAULT_DT) != 0) {
        evo_close(&ew);
        free(prev);
        darr_free(&devh);
        grid_free(&g);
        return 5;
    }
    converge_init(&cv, c->target, CONV_EPS);

    s1 = (long)(SNAP_T1 / DEFAULT_DT + 0.5);
    s2 = (long)(SNAP_T2 / DEFAULT_DT + 0.5);
    s3 = (long)(SNAP_T3 / DEFAULT_DT + 0.5);

    while (!cv.converged) {
        memcpy(prev, g.u, (size_t)g.nn * sizeof(double));
        ftcs_step(&s);
        converge_observe(&cv, prev, g.u, g.nn);
        if (darr_push(&devh, cv.max_dev) != 0) {
            c->rc = 6;
            break;
        }
        for (k = 0; k < CRIT_N; k++) {
            if (cv.max_jump > CRIT_EPS[k]) {
                c->last_above[k] = cv.steps;
            }
        }
        if (cv.steps == s1) {
            c->u10 = dup_field(&g);
        } else if (cv.steps == s2) {
            c->u20 = dup_field(&g);
        } else if (cv.steps == s3) {
            c->u30 = dup_field(&g);
        }
        /* 相隔正好一个 dt 的一对剖面：给第 6 步把差分方程本身代进数核对 */
        if (cv.steps == s1 + 1) {
            c->u10dt = dup_field(&g);
        }
        if ((double)cv.steps >= next_slice) {
            evo_write(&ew, s.time, g.u);
            c->nslice++;
            next_slice *= ratio;
        }
        if (cv.steps >= CONV_MAX_STEPS) {
            c->rc = 7;
            break;
        }
    }

    c->steps_conv = cv.steps;
    c->t_conv = (double)cv.steps * DEFAULT_DT;
    c->dev_conv = cv.max_dev;
    c->jump_conv = cv.max_jump;
    c->ucv = dup_field(&g);
    evo_write(&ew, s.time, g.u);
    c->nslice++;
    evo_close(&ew);

    /* 副判据的停手时刻：从"最近一次超阈值"往后挪一步 */
    for (k = 0; k < CRIT_N; k++) {
        st = c->last_above[k] + 1;
        c->stop_time[k] = (double)st * DEFAULT_DT;
        if (st > c->steps_conv || st < 1 || st > devh.n) {
            c->stop_step[k] = -1;
            c->stop_dev[k] = 0.0;
        } else {
            c->stop_step[k] = st;
            c->stop_dev[k] = devh.v[st - 1];
        }
    }

    /* 四个交付时刻的统计量 */
    {
        double *src[4];
        double tt[4];
        src[0] = c->u0;  tt[0] = 0.0;
        src[1] = c->u10; tt[1] = SNAP_T1;
        src[2] = c->u20; tt[2] = SNAP_T2;
        src[3] = c->u30; tt[3] = SNAP_T3;
        for (i = 0; i < 4; i++) {
            double heat;
            double mx;
            double mn;
            double er;
            if (src[i] == NULL) {
                c->heat[i] = c->mx[i] = c->mn[i] = c->err[i] = 0.0;
                continue;
            }
            memcpy(uwork, src[i], (size_t)g.nn * sizeof(double));
            memcpy(g.u, src[i], (size_t)g.nn * sizeof(double));
            heat = periodic ? grid_heat_periodic(&g) : grid_heat_dirichlet(&g);
            mx = field_max(&g);
            mn = field_min(&g);
            er = 0.0;
            for (k = 0; k < g.nn; k++) {
                double d = fabs(uwork[k] - analytic_u(g.x[k], tt[i], 0, periodic));
                if (d > er) {
                    er = d;
                }
            }
            c->heat[i] = heat;
            c->mx[i] = mx;
            c->mn[i] = mn;
            c->err[i] = er;
        }
        /* 收敛后剖面：与终态比，同时给出与解析解的差 */
    }

    /* 导出剖面文件 */
    sprintf(name, "profiles_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "题目 (d)(e) 的交付剖面：10/20/30 s 与完全收敛后；"
                  "dx=%.17g dt=%.17g r=%.17g 边界=%s 终态=%.17g eps=%.17g "
                  "收敛于 t=%.17g s（%ld 步）",
            g.dx, DEFAULT_DT, s.r, c->bctag, c->target, CONV_EPS,
            c->t_conv, c->steps_conv);
    if (c->u10 != NULL && c->u20 != NULL && c->u30 != NULL && c->ucv != NULL) {
        if (export_profiles(outdir, name, &g, c->u0, c->u10, c->u20,
                            c->u30, c->ucv, c->target, CONV_EPS,
                            c->t_conv, periodic, meta) != 0) {
            c->rc = 8;
        }
    } else {
        c->rc = 9;
    }
    sprintf(name, "stepdt_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "相隔正好一个 dt 的两条剖面（t=%.17g 与 t+dt）；"
                  "第 6 步把 FTCS 差分方程直接代进这些数核对",
            SNAP_T1);
    if (c->u10 != NULL && c->u10dt != NULL) {
        if (export_stepdt(outdir, name, &g, c->u10, c->u10dt, SNAP_T1,
                          DEFAULT_DT, material_alpha(), periodic, meta) != 0) {
            c->rc = 11;
        }
    } else {
        c->rc = 12;
    }
    sprintf(name, "criterion_%s_%s", c->dxtag, c->bctag);
    sprintf(meta, "副判据（逐step变化量）在不同阈值下的停手时刻与那时的残差；"
                  "主判据 eps=%.17g，tau=%.17g s，tau/dt=%.17g",
            CONV_EPS, tau, tau / DEFAULT_DT);
    if (export_criterion(outdir, name, meta, CRIT_EPS, c->stop_step,
                         c->stop_time, c->stop_dev, CRIT_N, CONV_EPS,
                         c->tau, DEFAULT_DT) != 0) {
        c->rc = 10;
    }

    free(prev);
    free(uwork);
    darr_free(&devh);
    grid_free(&g);
    return c->rc;
}

/*
 * 附图用的数据。图里不许现算物理，所以这些剖面也由 C 写出来：
 *   stability.dat    —— 同一时刻 t=10 s，r 从 0.23 到 0.69；r>0.5 的两列
 *                        已经炸到 1e21/1e47，图上单独用早期步数那一组画。
 *   overshoot.dat    —— CN 在 r=0.23 / 50 / 500 的 t=10 s 剖面，看负温度
 *   decomposition.dat —— 总误差拆成格式误差与初值表示误差
 */
static void export_demo_data(const char *outdir)
{
    Grid g;
    Ftcs fs;
    Crank cs;
    static double xs[V_DEMO_NN];
    static double buf[6][V_DEMO_NN];
    const char *heads[6];
    const double *cols[6];
    char meta[300];
    double dt;
    long i;
    long j;
    long nn;

    memset(&g, 0, sizeof(Grid));
    if (grid_init(&g, ROD_LENGTH, DEFAULT_DX) != 0) {
        return;
    }
    nn = g.nn;
    if (nn > V_DEMO_NN) {
        grid_free(&g);
        return;
    }
    for (i = 0; i < nn; i++) {
        xs[i] = g.x[i];
    }

    /* 初值 */
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    for (i = 0; i < nn; i++) {
        buf[0][i] = g.u[i];
    }

    /* FTCS：r=0.23 推到 10 s；r=0.69 只走 1/5/20 步（再多就 1e10 量级） */
    dt = 0.23 * g.dx * g.dx / material_alpha();
    grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
    if (ftcs_init(&fs, &g, BC_DIRICHLET, COLD_TEMP, material_alpha(), dt) == 0) {
        ftcs_advance(&fs, SNAP_T1);
        for (i = 0; i < nn; i++) {
            buf[1][i] = g.u[i];
        }
        ftcs_free(&fs);
    }
    dt = 0.69 * g.dx * g.dx / material_alpha();
    {
        long marks[3];
        long prev;
        long want;
        long step;

        marks[0] = 1;
        marks[1] = 5;
        marks[2] = 20;
        grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
        if (ftcs_init(&fs, &g, BC_DIRICHLET, COLD_TEMP,
                      material_alpha(), dt) == 0) {
            prev = 0;
            for (j = 0; j < 3; j++) {
                want = marks[j];
                for (step = prev; step < want; step++) {
                    ftcs_step(&fs);
                }
                prev = want;
                for (i = 0; i < nn; i++) {
                    buf[2 + j][i] = g.u[i];
                }
            }
            ftcs_free(&fs);
        }
    }

    heads[0] = "u_t0";
    heads[1] = "ftcs_r0.23_t10s";
    heads[2] = "ftcs_r0.69_step1";
    heads[3] = "ftcs_r0.69_step5";
    heads[4] = "ftcs_r0.69_step20";
    for (j = 0; j < 5; j++) {
        cols[j] = buf[j];
    }
    sprintf(meta, "FTCS 稳定性对照：r=0.23 推到 10 s 仍是平滑剖面；"
                  "r=0.69（超过 1/2）每步放大 |1-4r sin^2|=%.4f，"
                  "走 1/5/20 步后已成锯齿", 4.0 * 0.69 - 1.0);
    export_columns(outdir, "stability", meta, xs, cols, 5, heads, nn);

    /* CN：r=0.23 / 50 / 500 都推到 10 s */
    {
        double rs[3];
        long nstep[3];

        rs[0] = 0.23;
        rs[1] = 50.0;
        rs[2] = 500.0;
        for (j = 0; j < 3; j++) {
            dt = rs[j] * g.dx * g.dx / material_alpha();
            grid_initial_fv(&g, HOT_LO, HOT_HI, HOT_TEMP, COLD_TEMP, 0);
            if (crank_init(&cs, &g, BC_DIRICHLET, COLD_TEMP,
                           material_alpha(), dt) == 0) {
                crank_advance(&cs, SNAP_T1);
                for (i = 0; i < nn; i++) {
                    buf[1 + j][i] = g.u[i];
                }
                nstep[j] = cs.steps;
                crank_free(&cs);
            } else {
                nstep[j] = -1;
            }
        }
        heads[0] = "u_t0";
        heads[1] = "cn_r0.23_t10s";
        heads[2] = "cn_r50_t10s";
        heads[3] = "cn_r500_t10s";
        for (j = 0; j < 4; j++) {
            cols[j] = buf[j];
        }
        sprintf(meta, "Crank-Nicolson 无条件稳定但不保单调：r=50 用 %ld 步、"
                      "r=500 用 %ld 步走完 10 s，都不发散，"
                      "却在前沿下方打出负温度（B 阵对角权重 1-r 变负）",
                nstep[1], nstep[2]);
        export_columns(outdir, "overshoot", meta, xs, cols, 4, heads, nn);
    }
    grid_free(&g);

    /* 误差分解：一行一个 dx，列是三种误差 */
    if (demo_n > 0) {
        const char *sheads[4];
        double srow[12];
        long j;

        sheads[0] = "dx";
        sheads[1] = "format_err_max";
        sheads[2] = "representation_err_max";
        sheads[3] = "total_err_max";
        for (j = 0; j < demo_n; j++) {
            srow[j * 4 + 0] = demo_dx[j];
            srow[j * 4 + 1] = demo_fmt[j];
            srow[j * 4 + 2] = demo_rep[j];
            srow[j * 4 + 3] = demo_tot[j];
        }
        sprintf(meta, "t=10 s 总误差的分解（Dirichlet，dt=%.4f）：dx=0.002 时"
                      "高温段边界正好落在单元边界上，表示误差恒为 0；"
                      "dx=0.001 时边界落在节点中心，表示误差约占 85%%",
                DEFAULT_DT);
        export_summary(outdir, "decomposition", srow, 4, demo_n, sheads, meta);
    }
}

static void report_step5(const char *outdir)
{
    Case cs[4];
    Case *c;
    double tau_dt;
    double pred_dev;
    double ratio;
    double worst_pred;
    double row[16];
    double *rows;
    long i;
    long k;
    int all_ok;
    int ok_n2;
    int ok_n3;
    int ok_n4;
    int ok_n6;
    char buf[256];
    const char *heads[16];

    printf("\n== 收敛判据与四种组合的完整调度 ==\n");
    printf("  主判据 max|u-u_inf| <= eps = %.1e 摄氏度；dt = %.4f s\n",
           CONV_EPS, DEFAULT_DT);

    memset(cs, 0, sizeof(cs));
    cs[0].dxtag = "fine";   cs[0].bctag = "dirichlet"; cs[0].dx = DEFAULT_DX; cs[0].periodic = 0;
    cs[1].dxtag = "fine";   cs[1].bctag = "periodic";  cs[1].dx = DEFAULT_DX; cs[1].periodic = 1;
    cs[2].dxtag = "coarse"; cs[2].bctag = "dirichlet"; cs[2].dx = COARSE_DX;  cs[2].periodic = 0;
    cs[3].dxtag = "coarse"; cs[3].bctag = "periodic";  cs[3].dx = COARSE_DX;  cs[3].periodic = 1;

    all_ok = 1;
    for (i = 0; i < 4; i++) {
        if (run_case(&cs[i], outdir) != 0) {
            all_ok = 0;
            printf("  组合 %s/%s 失败 rc=%d\n", cs[i].dxtag, cs[i].bctag,
                   cs[i].rc);
        }
    }

    printf("\n  %-6s %-10s %10s %10s %10s %12s %12s %10s %12s %8s\n",
           "dx", "边界", "终态", "最低模幅", "tau/s", "预测 t/s", "实测 t/s",
           "步数", "停手残差", "片数");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        printf("  %-6s %-10s %10.6f %10.6f %10.4f %12.2f %12.2f %10ld %12.3e %8ld\n",
               c->dxtag, c->bctag, c->target, c->amp, c->tau,
               c->t_pred, c->t_conv, c->steps_conv, c->dev_conv, c->nslice);
    }

    printf("\n  副判据（逐step变化量）在不同阈值下什么时候停手：\n");
    printf("     预测残差 = eps_step * tau/dt，因为单步变化量约等于 残差 x dt/tau\n");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        tau_dt = c->tau / DEFAULT_DT;
        printf("  -- %s/%s：主判据在 t=%.2f s 停手；tau/dt = %.0f\n",
               c->dxtag, c->bctag, c->t_conv, tau_dt);
        printf("     %10s %10s %10s %13s %13s %9s\n",
               "eps_step", "停手步数", "停手 t/s", "实测残差", "预测残差", "残差/eps");
        for (k = 0; k < CRIT_N; k++) {
            if (c->stop_step[k] > 0) {
                pred_dev = CRIT_EPS[k] * tau_dt;
                printf("     %10.0e %10ld %10.2f %13.4e %13.4e %9.2f\n",
                       CRIT_EPS[k], c->stop_step[k], c->stop_time[k],
                       c->stop_dev[k], pred_dev, c->stop_dev[k] / CONV_EPS);
            } else {
                printf("     %10.0e %10s\n", CRIT_EPS[k], "(未达到)");
            }
        }
    }

    /* ---- 验收 ---- */
    sprintf(buf, "四种组合全部收敛，rc 全 0");
    check("N1", "四种组合都跑到主判据达标", all_ok, buf);

    ok_n2 = 1;
    for (i = 0; i < 4; i++) {
        if (!(cs[i].dev_conv <= CONV_EPS && cs[i].dev_conv > CONV_EPS * 0.2)) {
            ok_n2 = 0;
        }
    }
    sprintf(buf, "残差介于 [0.2 eps, eps]，最大 %.3e", cs[0].dev_conv);
    check("N2", "停手时刻不多跑：残差落在 eps 的一个数量级内", ok_n2, buf);

    worst_pred = 0.0;
    for (i = 0; i < 4; i++) {
        ratio = fabs(cs[i].t_conv / cs[i].t_pred - 1.0);
        if (ratio > worst_pred) {
            worst_pred = ratio;
        }
    }
    sprintf(buf, "实测/预测 最大相对差 %.4f%%", 100.0 * worst_pred);
    check("N3", "收敛时刻与单指数预测 tau*ln(A/eps) 一致",
          worst_pred < 0.05, buf);

    /* 副判据的松紧：预测残差 = eps_step * tau/dt */
    ok_n4 = 1;
    printf("\n  副判据松多少（预测残差 = eps_step * tau/dt）：\n");
    printf("  %-6s %-10s %12s %14s %14s %10s\n",
           "dx", "边界", "eps_step", "实测残差", "预测残差", "比值");
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        k = 1; /* eps_step = 1e-6 那一档 */
        if (c->stop_step[k] <= 0) {
            ok_n4 = 0;
            continue;
        }
        pred_dev = CRIT_EPS[k] * c->tau / DEFAULT_DT;
        ratio = c->stop_dev[k] / pred_dev;
        printf("  %-6s %-10s %12.1e %14.4e %14.4e %10.3f\n",
               c->dxtag, c->bctag, 1e-6, c->stop_dev[k], pred_dev, ratio);
        if (ratio < 0.8 || ratio > 1.25) {
            ok_n4 = 0;
        }
        if (c->stop_time[k] >= c->t_conv) {
            ok_n4 = 0;
        }
    }
    sprintf(buf, "1e-6 档副判据提前停手，实测/预测残差比见上表");
    check("N4", "副判据在残差约 eps_step*tau/dt 处停手，且早于主判据", ok_n4, buf);

    ok_n6 = 1;
    for (i = 0; i < 4; i++) {
        if (cs[i].nslice > EXPORT_MAX_SLICES + 2) {
            ok_n6 = 0;
        }
    }
    sprintf(buf, "切片数 %ld/%ld/%ld/%ld（上限 %d）",
            cs[0].nslice, cs[1].nslice, cs[2].nslice, cs[3].nslice,
            EXPORT_MAX_SLICES);
    check("N6", "时空演化文件的时间抽稀不超上限", ok_n6, buf);

    /* 终态本身 */
    ok_n3 = 1;
    for (i = 0; i < 4; i++) {
        c = &cs[i];
        if (fabs(c->target - (c->periodic ? 10.0 : 0.0)) > 1e-9) {
            ok_n3 = 0;
        }
    }
    sprintf(buf, "Dirichlet 终态 %.1f，周期终态 %.1f", cs[0].target, cs[1].target);
    check("N5", "两种边界的终态分别是 0 度与 10 度", ok_n3, buf);

    /* 汇总文件 */
    heads[0] = "dx"; heads[1] = "periodic"; heads[2] = "dt"; heads[3] = "target";
    heads[4] = "amp_k1"; heads[5] = "tau"; heads[6] = "t_pred"; heads[7] = "t_conv";
    heads[8] = "steps_conv"; heads[9] = "dev_conv"; heads[10] = "jump_conv";
    heads[11] = "err_t10"; heads[12] = "err_t20"; heads[13] = "err_t30";
    heads[14] = "heat_t30"; heads[15] = "nslice";
    rows = (double *)malloc((size_t)(4 * 16) * sizeof(double));
    if (rows != NULL) {
        for (i = 0; i < 4; i++) {
            c = &cs[i];
            row[0] = c->dx; row[1] = (double)c->periodic; row[2] = DEFAULT_DT;
            row[3] = c->target; row[4] = c->amp; row[5] = c->tau;
            row[6] = c->t_pred; row[7] = c->t_conv; row[8] = (double)c->steps_conv;
            row[9] = c->dev_conv; row[10] = c->jump_conv;
            row[11] = c->err[1]; row[12] = c->err[2]; row[13] = c->err[3];
            row[14] = c->heat[3]; row[15] = (double)c->nslice;
            for (k = 0; k < 16; k++) {
                rows[i * 16 + k] = row[k];
            }
        }
        export_summary(outdir, "summary", rows, 16, 4, heads,
                       "四种组合的收敛与误差汇总；报告表格引这份数");
        free(rows);
    }

    for (i = 0; i < 4; i++) {
        free(cs[i].u0);
        free(cs[i].u10);
        free(cs[i].u20);
        free(cs[i].u30);
        free(cs[i].u10dt);
        free(cs[i].ucv);
    }
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s [--verify-only] [outdir]\n"
            "  默认：跑完全部计算并导出到 outdir（默认 data），最后做代入式核对\n"
            "  --verify-only：不重算，只核对 outdir 里已有的数据文件\n",
            prog);
}

int main(int argc, char **argv)
{
    const char *outdir;
    int verify_only = 0;
    int i;

    /*
     * 参数：目录名随意放，--verify-only 表示"不重算，只把 data 里现有的
     * 数字读回来核对"。没有这个入口，代入式核对就永远只在验自己刚写出的
     * 正确数据——篡改过的交付文件会被重算覆盖掉，检查形同虚设。
     */
    outdir = NULL;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify-only") == 0) {
            verify_only = 1;
        } else if (strcmp(argv[i], "-h") == 0
                   || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            outdir = argv[i];
        }
    }
    if (outdir == NULL) {
        outdir = "data";
    }

    if (verify_only) {
        printf("热传导作业 / 只核对不重算：目录 %s\n", outdir);
        printf("（跳过全部计算与导出，直接把 data 里现有的数字代进定义式）\n");
        if (verify_run(outdir) != 0) {
            printf("VERIFY-ONLY: FAIL\n");
            return 1;
        }
        printf("VERIFY-ONLY: PASS\n");
        return 0;
    }

    printf("热传导作业 / 步骤 1-5：参数、初值、FTCS、解析解、CN 对照、收敛与导出\n");
    printf("输出目录: %s\n\n", outdir);

    report_material();
    if (report_grid(DEFAULT_DX, "(a)(d) 细网格", "initial_fine", outdir) != 0) {
        return 1;
    }
    if (report_grid(COARSE_DX, "(e) 粗网格", "initial_coarse", outdir) != 0) {
        return 1;
    }

    report_evolution(DEFAULT_DX, "细网格", BC_DIRICHLET, "两端 0 度");
    report_evolution(DEFAULT_DX, "细网格", BC_PERIODIC, "周期边界");
    report_evolution(COARSE_DX, "粗网格", BC_DIRICHLET, "两端 0 度");
    report_evolution(COARSE_DX, "粗网格", BC_PERIODIC, "周期边界");

    report_stability(DEFAULT_DX, "细网格");

    printf("\n== 离散模态放大因子（细网格，走 1000 步 = 10 s）==\n");
    report_mode(DEFAULT_DX, BC_DIRICHLET, "Dirichlet", 1, 1000);
    report_mode(DEFAULT_DX, BC_DIRICHLET, "Dirichlet", 99, 1000);
    report_mode(DEFAULT_DX, BC_PERIODIC, "周期", 1, 1000);

    report_analytic_coeffs();
    report_analytic_series();
    report_analytic_vs_ftcs();
    report_error_decomposition();
    report_convergence_order();

    report_crank_mode();
    report_crank_stability();
    report_crank_order();
    report_crank_vs_ftcs_pulse();

    report_step5(outdir);
    export_demo_data(outdir);

    checks_failed += verify_run(outdir);

    printf("\n== 验收：程序内 %ld 项 + 代入式核对 %ld 项 = %ld 项，%ld 项失败 ==\n",
           checks_run, verify_checks_run(),
           checks_run + verify_checks_run(), checks_failed);
    if (checks_failed != 0) {
        return 1;
    }
    printf("STEP1-7: PASS\n");
    return 0;
}
