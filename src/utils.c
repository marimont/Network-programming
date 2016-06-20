/*
    This function computes a 32 bit hash code from
    the array of bytes named data, whose length is n,
    and the integer init.
*/
#include "utils.h"

uint32_t hashCode(char *data, uint32_t n, uint32_t init) {
	uint32_t i, result;

	result = init;
	for (i=0; i<n; i++)
		result = result * 31 + data[i];
	return result;
}
