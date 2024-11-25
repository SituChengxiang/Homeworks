#include <stdio.h>

int digit(int n, int k);

int main() {
    int n, k;
    printf("分别输入整数和位数: ");
    scanf("%d %d", &n, &k);

    if (k <= 0) {
        printf("位数 k 必须大于 0\n");
        return 1;
    }

    if (n < 0) {
        n = -n; 
    }

    printf("整数 %d 从右边开始第 %d 位数字的值为 %d\n", n, k, digit(n, k));
    return 0;
}

int digit(int n, int k) {
    int i;
    for (i = 0; i < k - 1; i++) {
        n = n / 10;
    }
    return (n % 10);
}
