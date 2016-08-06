#pragma once

#include <stdint.h>
#include <stddef.h>

// Various simple hash functions, with inline implementations

struct Hasher32Bit { typedef uint32_t HashType; };
struct Hasher64Bit { typedef uint64_t HashType; };


// djb2 based on http://www.cse.yorku.ca/~oz/hash.html
struct djb2_hash : public Hasher32Bit
{
	inline HashType operator()(const char* s, size_t size) const
	{
		HashType hash = 5381;
		for (size_t i = 0; i < size; ++i)
		{
			char c = *s++;
			hash = ((hash << 5) + hash) ^ c;
		}
		return hash;
	}
};

// SDBM based on http://www.cse.yorku.ca/~oz/hash.html
struct SDBM_hash : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const
	{
		HashType hash = 0;
		const char* str = (const char*)data;
		const char* end = str + size;
		while (str < end)
		{
			HashType c = *str++;
			hash = c + (hash << 6) + (hash << 16) - hash;
		}
		return hash;
	}
};


// 32 bit Fowler-Noll-Vo hash (FNV-1a) https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
struct FNV1aHash : public Hasher32Bit
{
	inline HashType operator()(const void* data, size_t size) const
	{
		const char* c = (const char*)data;
		const char* end = c + size;
		const uint32_t p = 16777619U;
		uint32_t hash = 2166136261U;
		while (c < end)
		{
			hash = (hash ^ *c++) * p;
		}
		return hash;
	}
};

// Modified 32 bit FNV-1a hash, from http://papa.bretmulvey.com/post/124027987928/hash-functions
// with better avalanche behavior and uniform distribution with larger hash sizes.
struct FNV1aModifiedHash : public Hasher32Bit
{
	inline HashType operator()(const void* data, size_t size) const
	{
		const char* c = (const char*)data;
		const char* end = c + size;

		const uint32_t p = 16777619U;
		uint32_t hash = 2166136261U;
		while (c < end)
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
};


// Hash function we had in our codebase, from some super old code. Was not used much, but
// a few places did use it. No one knows where it came from, but seems similar to the one
// used by ELF symbol table: http://www.sco.com/developers/gabi/latest/ch5.dynamic.html#hash
struct ELF_Like_Bad_Hash : public Hasher32Bit
{
	HashType operator ()(const void* data, size_t size) const
	{
		const char* key = (const char*)data;
		const char* end = key + size;

		HashType h = 0;
		const HashType sr = 8 * sizeof (unsigned) - 8;
		const HashType mask = 0xFu << (sr + 4);
		while (key < end)
		{
			h = (h << 4) + *key;
			HashType g = h & mask;
			h ^= g | (g >> sr);
			key++;
		}
		return h;
	}
};
