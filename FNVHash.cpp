#include "FNVHash.h"

// 32 bit Fowler–Noll–Vo hash.
uint32_t FNVHash(const char* key)
{
	const uint32_t p = 16777619U;
	uint32_t hash = 2166136261U;
	const char* c = key;
	while (*c)
	{
		hash = (hash ^ *c++) * p;
	}

	hash += hash << 13;
	hash ^= hash >> 7;
	hash += hash << 3;
	hash ^= hash >> 17;
	hash += hash << 5;
	return hash;
}
