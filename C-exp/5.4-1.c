#include <stdio.h>
int main(){
	float a,b,c,m;
	scanf("%f %f %f",&a,&b,&c);
	m = a;
	if(b>=a){
		m = b;
		if(c>=b){
			m = c;
		}
	}else if (c>=a){
		m = c;
	}
	printf("%f",m);
	return 0;
}

