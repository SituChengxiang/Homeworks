#include <stdio.h>
#include <math.h>

int isPrimenumber(int);

int main() {
    int a, b;
    printf("���������������� a �� b (a <= b): ");
    scanf("%d %d", &a, &b);

    if (a > b || a <= 1) {
        printf("%d ��������[1,%d]֮�����Ч��ֵ\n", a, b);
        return 0;
    }

    if (isPrimenumber(a)) {
        printf("%d ������[1,%d]֮�������\n", a, b);
    } else {
        printf("%d ��������[1,%d]֮�������\n", a, b);
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
