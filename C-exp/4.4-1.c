#include <stdio.h>
void main(){
	int m,n,jc=1;
	scanf("%d",&m);
	for(n=2;jc<=m;n++){
		jc=jc*n;
	}
	printf("n=%d\n",n-2);
}


