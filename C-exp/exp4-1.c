#include <stdio.h>

int main() {
    double max, min, result, x;
    max = 0.0; 
    min = 0.0; 

    for (x = -2; x < 2.5; x += 0.5) {
        result = x * x - 3.14 * x - 6;
        max = (result >= max) ? result : max;
        min = (result <= min) ? result : min;
    }

    printf("最大值为：%f，最小值为：%f\n", max, min);

    return 0;
}
