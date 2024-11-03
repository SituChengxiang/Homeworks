#include <stdio.h>

int main() {
	// 声明变量用于存储输入的字符
	char ch1, ch2;
	
	// 提示用户输入第一个字符并按下回车键
	printf("请输入第一个字符: ");
	
	// 读取一个字符，并将其赋值给ch1
	ch1 = getchar();
	if (ch1 == '\n') {  // 判断是否直接按下了回车键，如果是则提示重新输入
		printf("请不要直接按下回车键，请输入一个字符: ");
		ch1 = getchar();
	}
	
	// 提示用户输入第二个字符并按下回车键
	printf("请输入第二个字符: ");
	
	// 读取另一个字符，并将其赋值给ch2
	ch2 = getchar();
	if (ch2 == '\n') {  // 判断是否直接按下了回车键，如果是则提示重新输入
		printf("请不要直接按下回车键，请输入一个字符: ");
		ch2 = getchar();
	}
	
	// 输出两个字符
	putchar(ch1);
	putchar(ch2);
	
	return 0;
}
