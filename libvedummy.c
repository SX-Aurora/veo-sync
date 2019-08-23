#include <stdlib.h>

long dummy(void)
{
	int i, imax = 1000000;
	long sum = 0;
	for (i = 0; i < imax; i++)
		sum += rand() % 10;
	return sum;
}
