#include<stdio.h>
#include<math.h>
int main(){
	int a,b,c,aba,abb,abc;
	scanf("%d %d %d",&a,&b,&c);
	aba=abs(a);abb=abs(b);abc=abs(c);
	if(aba>=abb){//|a|>=|b|
		if (abc>=aba){//|c|>=|a|>=|b|
			printf("%d %d %d",b,a,c);
		}else if(abb>=abc){//|a|>=|b|>|c|
			printf("%d %d %d",c,b,a);	
		}else{//|a|>=|c|=>|b|
			printf("%d %d %d",b,c,a);
		}
	}else{//|b|>=|a|
		if (abc>=abb){//|c|>=|b|>=|a|
			printf("%d %d %d",a,b,c);
		}else if(aba>=abc){//|b|>=|a|>=|c|
			printf("%d %d %d",c,a,b);
		}else{//|b|>=|c|>=|a|
			printf("%d %d %d",a,c,b);
		}
	}
	return 0;
}
