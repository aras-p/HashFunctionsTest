#pragma once

#include <stdint.h>
#include <stddef.h>

// Various simple hash functions, with inline implementations

struct Hasher32Bit { typedef uint32_t HashType; };
struct Hasher64Bit { typedef uint64_t HashType; };
struct myuint128_t {
	uint64_t a, b;
	myuint128_t(uint64_t a_=0, uint64_t b_=0) : a(a_), b(b_) {}

	// not a good approx, but eh
	uint64_t operator%(uint64_t sz) { return a % sz; }
	uint64_t operator^(uint64_t sz) { return a ^ b ^ sz; }
	bool operator<(const myuint128_t& o) const
	{
		if (a != o.a)
			return a < o.a;
		return b < o.b;
	}
    bool operator==(const myuint128_t& o) const
    {
        return a==o.a && b==o.b;
    }
};
struct Hasher128Bit { typedef myuint128_t HashType; };


// djb2 based on http://www.cse.yorku.ca/~oz/hash.html
struct djb2_hash : public Hasher32Bit
{
	inline HashType operator()(const void* data, size_t size) const
	{
		const char* s = (const char*)data;
		HashType hash = 5381;
		for (size_t i = 0; i < size; ++i)
		{
			char c = *s++;
			hash = ((hash << 5) + hash) ^ c;
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

