#ifndef __HASH_FUNCTIONS_H
#define __HASH_FUNCTIONS_H

#include <string>
#include <utility>
#include <stdint.h>

// http://www.cse.yorku.ca/~oz/hash.html
// Other hashes here: http://burtleburtle.net/bob/
template<typename T>
struct djb2_hash_t
{
	inline unsigned int operator()(const T& str) const
	{
		unsigned int hash = 5381;
		char c;
		const char* s = str.c_str();

		while ((c = *s++))
			hash = ((hash << 5) + hash) ^ c;

		return hash;
	}
};
typedef djb2_hash_t<std::string> djb2_hash;

#endif
