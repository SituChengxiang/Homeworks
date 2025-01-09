#include <stdio.h>
#include <math.h>

int main() {
    float x[10], y[10], distance = 0.0; 
    int i, j;
    for (i = 0; i < 10; i++) {
        printf("请输入第%d个点的 x 坐标: ", i + 1);
        scanf("%f", x + i);
        printf("请输入第%d个点的 y 坐标: ", i + 1);
        scanf("%f", y + i);
    }
    for (i = 0; i < 9; i++) {
        for (j = i + 1; j < 10; j++) { // 从 i+1 开始，避免重复计算
            distance += sqrt((x[i] - x[j]) * (x[i] - x[j]) + (y[i] - y[j]) * (y[i] - y[j]));
        }
    }
    printf("十个点的距离和为：%f\n", distance); 
    return 0;
}
