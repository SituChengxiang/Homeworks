#include<stdio.h>
#include<math.h>
int main(){
	int a,b,c; float avg,da,db,dc;
	scanf("%d %d %d",&a,&b,&c);
	avg = (a+b+c)/3;da=abs(a-avg);db=abs(b-avg);dc=abs(c-avg);
	// da, db, dc����С�� 
	if(da>=db){//da>db ��a��������ӽ��� 
		if(db>=dc){//da>=db>=dc
			printf("%d",c);
		}else{
			printf("%d",b);
		}
	}else{//db>da ��b��������ӽ��� 
		if(da>=dc){//db>=da>=dc
			printf("%d",c);
		}else{
			printf("%d",a);
		}	
	}
	return 0;
}
