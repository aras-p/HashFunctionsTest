#ifndef CSTRINGHASH_H
#define CSTRINGHASH_H

// Hash function we had, from some super old code. Was not used much, but a few places did use it.
// It's a bad hash function! Do not do things like this.
struct hash_cstring
{
	unsigned operator ()(const char* key, int size) const
	{
		unsigned h = 0;
		const unsigned sr = 8 * sizeof (unsigned) - 8;
		const unsigned mask = 0xFu << (sr + 4);
		for (int i = 0; i < size; ++i)
		{
			h = (h << 4) + *key;
			unsigned g = h & mask;
			h ^= g | (g >> sr);
			key++;
		}
		return h;
	}
};

#endif
