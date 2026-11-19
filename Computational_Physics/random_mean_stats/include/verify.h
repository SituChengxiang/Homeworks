#ifndef VERIFY_H
#define VERIFY_H

/*
 * 代入式核对：只把已经写进 data 目录里的 .dat 的数字读回来，代进恒等式检查。
 *
 * 为什么要有这一步：Python 交叉核对能发现「C 写错了」，但如果 Python 本身
 * 也写错了，两边可以一起错得自洽。代入式检查用的是定义级恒等式
 * （插值必须过节点、最小二乘的驻点条件、计数之和等于 N），
 * 任何人拿报告里的数字用计算器都能复算，不依赖第二份实现。
 *
 * 返回失败的检查项数（0 表示全部通过）。
 */
int verify_exported(const char *outdir);

#endif
