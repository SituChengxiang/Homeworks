#include<stdio.h>
#include<math.h>
int main(){
	float x,y=0.0;
	scanf("%f",&x);
	if(0<=x&&x<=1){
		y= x+ cos(x);
	}else{
		y= x+ sin(x);
	}
	printf("%f",y);
	
	return 0;
}
