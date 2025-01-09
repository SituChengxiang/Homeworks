#include <stdio.h>
#include <math.h>

int isPerfect(int a);

int main() {
    int i;
    for (i = 1; i < 1001; i++) {
        if (isPerfect(i)) {
            printf("%d\n", i);
        }
    }
    return 0;
}

int isPerfect(int a) {
    int i, sum = 1; 
    if (a <= 1) {
        return 0;
    }
    for (i = 2; i <= sqrt(a); i++) {
        if (a % i == 0) {
            sum += i;
            if (i * i != a) { 
                sum += a / i;
            }
        }
    }
    return sum == a;
}
