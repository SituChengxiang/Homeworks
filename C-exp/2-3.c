#include <stdio.h>

int main(){
	int a;
	printf("请输入一个十进制整数：");
	scanf("%d",&a);
	printf("八进制为：%o；十进制为：%d，十六进制为：%x",a,a,a);
	return 0;
}
