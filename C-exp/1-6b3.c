#include <stdio.h>
#include <math.h>
int main()
{
	float r,h,V;
	printf("请输入圆柱体的半径和高：");
	scanf("%f %f",&r,&h);
	V=M_PI*r*r*h;
	printf("圆柱体的体积为：%f",V);
	return 0;
}
// 你好啊
// 这是一个注释
