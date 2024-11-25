#include <stdio.h>

int main() {
    int i, j;

    for (i = 0; i < 10; i++) {
        // µÚiÐÐ¿ØÖÆ
        if (0 == i) {
            printf("\t(1)\t(2)\t(3)\t(4)\t(5)\t(6)\t(7)\t(8)\t(9)\n");
        } else {
            // i = 1~9
            for (j = 0; j < 10; j++) {
                if (0 == j) {
                    printf("(%d)\t", i);
                } else {
                    printf("%d\t", i * j);
                    if (9 == j) {
                        printf("\n");
                    }
                }
            }
        }
    }

    return 0;
}
