#include "random.h"
#include <stdlib.h>

int randint(const int min, const int max)
{
    unsigned int tmp;
    const unsigned int diff = 1 + max - min;
    const unsigned int buckets = RAND_MAX / diff;
    const unsigned int limit = buckets * diff;

    do {
        tmp = rand();
    } while (tmp >= limit);

    return min + (tmp / buckets);
}
