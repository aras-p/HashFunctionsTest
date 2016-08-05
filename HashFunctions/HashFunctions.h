#ifndef __HASH_FUNCTIONS_H
#define __HASH_FUNCTIONS_H

#include <stdint.h>
#include <stddef.h>

// http://www.cse.yorku.ca/~oz/hash.html
// Other hashes here: http://burtleburtle.net/bob/
struct djb2_hash
{
	typedef unsigned int HashType;
	inline unsigned int operator()(const char* s, size_t size) const
	{
		unsigned int hash = 5381;
		char c;
		for (size_t i = 0; i < size; ++i)
		{
			c = *s++;
			hash = ((hash << 5) + hash) ^ c;
		}
		return hash;
	}
};

#endif
