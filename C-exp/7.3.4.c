#include <stdio.h>
int main() {
    int x[5][6], i, j, min, min_row, min_column;
    for (i = 0; i < 5; i++) {
        printf("������� %d �е� 6 ��Ԫ�أ��ո�ָ: ", i + 1);
        for (j = 0; j < 6; j++) {
            scanf("%d", &x[i][j]);
        }
    }

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 6; j++) {
            printf("%d ", x[i][j]);
        }
        printf("\n");
    }
    min = x[0][0];
    min_row = 0;
    min_column = 0;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 6; j++) {
            if (min >= x[i][j]) {
                min = x[i][j];
                min_row = i;
                min_column = j;
            }
        }
    }
    printf("��СֵΪ��%d���ڵ� %d �� �� %d ��\n", min, min_row + 1, min_column + 1);
    return 0;
}

