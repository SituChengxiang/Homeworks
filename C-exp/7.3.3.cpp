#include <stdio.h>
#include <math.h>
#include <stdlib.h>
int compare(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

int main(){
	int n = 0; float a=0.0;
	printf("����������Ҫ���ٸ�����");
	scanf("%d",&n);
	while(n<1||n>20){
		printf("������Ч����������");
	}
	float x[n];
	for (int i = 0; i < n; i++) {
	    printf("�������%d����: ", i + 1);
	    scanf("%f", &a);
	    x[i]=abs(a);
	}
	qsort(x, n, sizeof(float), compare);

    printf("����������: \n");
    for (int i = 0; i < n; i++) {
        printf("%.2f ", x[i]);
    }
    printf("\n");
	return 0;
}
