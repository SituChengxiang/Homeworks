#include <stdio.h>
#include <math.h>

int main() {
    float x[10], y[10], distance = 0.0; 
    int i, j;
    for (i = 0; i < 10; i++) {
        printf("�������%d����� x ����: ", i + 1);
        scanf("%f", x + i);
        printf("�������%d����� y ����: ", i + 1);
        scanf("%f", y + i);
    }
    for (i = 0; i < 9; i++) {
        for (j = i + 1; j < 10; j++) { // �� i+1 ��ʼ�������ظ�����
            distance += sqrt((x[i] - x[j]) * (x[i] - x[j]) + (y[i] - y[j]) * (y[i] - y[j]));
        }
    }
    printf("ʮ����ľ����Ϊ��%f\n", distance); 
    return 0;
}
