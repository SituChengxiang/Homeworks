#ifndef VERIFY_H
#define VERIFY_H

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

#endif
