#include "random.h"
#include <stdlib.h>

int randint(const int min, const int max)
{
	int tmp;
	const int diff = 1 + max - min;
	const int buckets = RAND_MAX / diff;
	const int limit = buckets * diff;

	do {
		tmp = rand();
	} while (tmp >= limit);

	return min + (tmp / buckets);
}
