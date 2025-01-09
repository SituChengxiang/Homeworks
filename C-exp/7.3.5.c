#include <stdio.h>
#include <math.h>
#include <float.h>
int main() {
    float x[5][6],max[5];
	int i=0, j=0;
    for (i = 0; i < 5; i++) {
        max[i] = -10000000; // 设置一个很小的初始值
    }
    for (i = 0; i < 5; i++) {
        printf("请输入第 %d 行的 6 个元素（空格分割）: ", (int)i + 1);
        for (j = 0; j < 6; j++) {
            scanf("%f", &x[(int)i][(int)j]);
            if (fabs(x[(int)i][(int)j]) > max[(int)i]) {
                max[(int)i] = fabs(x[(int)i][(int)j]);
            }
        }
    }
    printf("\n处理后的数组（每行元素除以其绝对值最大值）:\n");
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 6; j++) {
            if (max[(int)i] == 0) {
                printf("NaN "); 
            } else {
                printf("%12.6f ", x[(int)i][(int)j] / max[(int)i]); 
            }
        }
        printf("\n");
    }
    return 0;
}







