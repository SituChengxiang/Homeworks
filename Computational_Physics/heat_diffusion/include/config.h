#ifndef CONFIG_H
#define CONFIG_H

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

#endif