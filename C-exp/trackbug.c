#include<stdio.h>
#include<math.h>
int main()
	
{
	float z;
	double x,y;

	printf("请输入x和y的值:");
	scanf("%lf%lf",&x,&y);
	if(x>=0){
		if(y>=0){
			z=x*x+y*y;
		}else{
			z=x+y;
		}
	} else if(y>=0){
		z=x-y;
	}else{
		z=x*x-y*y;  
	}
	
	printf("函数的值为:%0.1lf",z);
	return 0;
}  

