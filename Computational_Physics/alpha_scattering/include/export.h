#ifndef EXPORT_H
#define EXPORT_H

#include "trajectory.h"
#include "cases.h"

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

#endif /* EXPORT_H */
