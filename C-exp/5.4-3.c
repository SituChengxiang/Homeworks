#include <stdio.h>

int digit(int n, int k);

int main() {
    int n, k;
    printf("�ֱ�����������λ��: ");
    scanf("%d %d", &n, &k);

    if (k <= 0) {
        printf("λ�� k ������� 0\n");
        return 1;
    }

    if (n < 0) {
        n = -n; 
    }

    printf("���� %d ���ұ߿�ʼ�� %d λ���ֵ�ֵΪ %d\n", n, k, digit(n, k));
    return 0;
}

int digit(int n, int k) {
    int i;
    for (i = 0; i < k - 1; i++) {
        n = n / 10;
    }
    return (n % 10);
}
