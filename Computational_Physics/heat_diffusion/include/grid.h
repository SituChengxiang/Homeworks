#ifndef GRID_H
#define GRID_H

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

#endif