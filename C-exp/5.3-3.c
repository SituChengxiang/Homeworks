#include <stdio.h>
#define S x=y=z
#define P3(x,y,z) printf("x=%d\ty=%d\tz=%d\n",x,y,z)
int mian(){
	int x,y,z;
	S=1; ++x||++y||++z; P3(x,y,z);
	S=1; ++x&&++y||++z; P3(x,y,z);
	S=-1;++x||++y&&++z; P3(x,y,z);
	S=-1;++x&&++y&&++z; P3(x,y,z);
	return 0;
}
