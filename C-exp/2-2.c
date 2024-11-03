#include <stdio.h>

int main(){
	int a;
	float b;
	double c;

	printf("请输入1个10进制整数、1个单精度浮点、1个双精度浮点数：");
	scanf("%d %f %lf",&a,&b,&c);
	printf("你输入的10进f制整数是: %d\n", a);
	printf("你输入的单精度浮点数是: %.1f\n", b);
	printf("你输入的双精度浮点数是: %.2lf\n", c);
	return 0;
}
