#include <stdio.h>
#include <string.h>
int main()
{
	int i, j;
	char *name[] = {"HangZhou", "ShangHai", "GuangZhou", "BeiJing", "ShenZhen"};
	char *pstr;
	
	// 冒泡排序算法对字符串数组进行升序排列
	for (i = 0; i < 4; i++) { // 外层循环控制遍历次数
		for (j = 0; j < 4 - i; j++) { // 内层循环比较相邻元素
			if (strcmp(name[j], name[j + 1]) < 0) { // 如果前一个字符串大于后一个字符串，则交换
				pstr = name[j];
				name[j] = name[j + 1];
				name[j + 1] = pstr;
			}
		}
	}
	
	// 输出排序后的字符串数组
	for (i = 0; i < 5; i++)
		printf("%c\n", *name[i]); // 使用 %s 格式符打印字符串
	
	for (i = 0; i < 5; i++)
		printf("%c\n", name[i][1]); // 使用 %s 格式符打印字符串
	
	return 0;
}
