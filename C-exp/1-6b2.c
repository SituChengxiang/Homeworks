#include <stdio.h>

int main() {
	int a, b, max;
	
	printf("请输入A,B的值（整数！）：\n");
	scanf("%d,%d", &a, &b);
	
	if (a > b) {
		max = a;
	} else {
		max = b;
	}
	
	printf("较大的值是：%d\n", max);
	
	return 0; // 添加return语句，使程序正常退出
}
