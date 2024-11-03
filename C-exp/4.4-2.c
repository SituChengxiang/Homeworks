#include<stdio.h>
int main() {
	int a, b, c, i;
	
	for (a = 6; a <= 10000; a++) {
		b = 1;
		//计算 a 的真因数之和
		for (i = 2; i <= a / 2; i++) {
			if (a % i == 0) {
				b += i;
			}
		}
		
		c = 1;
		//计算 b 的真因数之和
		for (i = 2; i <= b / 2; i++) {
			if (b % i == 0) {
				c += i;
			}
		}
		
		//检查是否是亲密数对，并且 a 和 b 不相等
		if (c == a && a != b) {
			printf("%d  %d\n", a, b);
		}
	}
	
	return 0;
}
