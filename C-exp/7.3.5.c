#include <stdio.h>
#include <math.h>
#include <float.h>
int main() {
    float x[5][6],max[5];
	int i=0, j=0;
    for (i = 0; i < 5; i++) {
        max[i] = -10000000; // ����һ����С�ĳ�ʼֵ
    }
    for (i = 0; i < 5; i++) {
        printf("������� %d �е� 6 ��Ԫ�أ��ո�ָ: ", (int)i + 1);
        for (j = 0; j < 6; j++) {
            scanf("%f", &x[(int)i][(int)j]);
            if (fabs(x[(int)i][(int)j]) > max[(int)i]) {
                max[(int)i] = fabs(x[(int)i][(int)j]);
            }
        }
    }
    printf("\n���������飨ÿ��Ԫ�س��������ֵ���ֵ��:\n");
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







