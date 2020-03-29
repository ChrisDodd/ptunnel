#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "tea.h"

#define tv_diff(s, e)	(((e).tv_sec - (s).tv_sec) * 1000000 +		\
			 (e).tv_usec - (s).tv_usec)

int main()
{
uint64_t        data, key0, key1;
struct timeval	s, e;
int		i;

    data = 0;
    key0 = 0x9abcdef012345678;
    key1 = 0x76543210fedcba98;

    gettimeofday(&s, 0);
    for (i=10000000; i>0; i--)
	data = tea_encode(data, key0, key1);
    gettimeofday(&e, 0);

    printf("time per block %f usec\n", tv_diff(s, e)/10000000.0);
}
