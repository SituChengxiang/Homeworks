#include <stdio.h>
#include <math.h>

int isPrimenumber(int);

int main() {
    int a, b;
    printf("请输入两个正整数 a 和 b (a <= b): ");
    scanf("%d %d", &a, &b);

    if (a > b || a <= 1) {
        printf("%d 不是区间[1,%d]之间的有效数值\n", a, b);
        return 0;
    }

    if (isPrimenumber(a)) {
        printf("%d 是区间[1,%d]之间的素数\n", a, b);
    } else {
        printf("%d 不是区间[1,%d]之间的素数\n", a, b);
    }
    return 0;
}

int isPrimenumber(int a) {
    for (int i = 2; i <= sqrt(a); i++) {
        if (a % i == 0) {
            return 0; 
        }
    }
    return 1; 
}
