#include <stdio.h>
#include <ctype.h>
int main(){
	char a,b,x; int i;
	while (! isupper(getchar()));
	for(a='A';a<=x;a++){
		for(b='A';b<'A'+x-a;b++){
			putchar(' ');
		}
		for(i=1;i<=2*(a-'A')+1;i++)
		{
			putchar(a);
		}
		putchar('\n');
	}
	return 0;
}

