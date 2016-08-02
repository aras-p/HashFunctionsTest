#ifndef CSTRINGHASH_H
#define CSTRINGHASH_H
#include <functional>

struct hash_cstring : std::unary_function<const char*, std::size_t>
{
	unsigned operator ()(const char* key) const
	{
		unsigned h = 0;
		const unsigned sr = 8 * sizeof (unsigned) - 8;
		const unsigned mask = 0xFu << (sr + 4);
		while (*key != '\0')
		{
			h = (h << 4) + *key;
			std::size_t g = h & mask;
			h ^= g | (g >> sr);
			key++;
		}
		return h;
	}
};

#endif
