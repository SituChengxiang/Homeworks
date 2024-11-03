#include <stdio.h>

void f10_2(int n) {
	if (n == 0) { // 基本情况：当 n 为 0 时停止递归并返回
		return;
	} else {
		f10_2(n / 2); // 递归调用，处理 n 的整数部分
		printf("%d", n % 2); // 输出二进制位
	}
}

int main() {
	int n = 11; // 示例输入
	f10_2(n);
	return 0;
}
