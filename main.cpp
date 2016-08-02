#define _CRT_SECURE_NO_WARNINGS

#include "xxhash.h"
#include "MurmurHash2.h"
#include "MurmurHash3.h"
#include "SpookyV2.h"
#include "FNVHash.h"
#include "CStringHash.h"
#include "HashFunctions.h"

#include <vector>
#include <string>
#include <set>
#include <stdio.h>


extern void crc32 (const void * key, int len, uint32_t seed, void * out);


// ------------------------------------------------------------------------------------
// Timer code

#ifdef _MSC_VER
	#include <Windows.h>
	static LARGE_INTEGER s_Time0;
	static void TimerBegin()
	{
		QueryPerformanceCounter (&s_Time0);
	}
	static float TimerEnd()
	{
		static bool timerInited = false;
		static LARGE_INTEGER ticksPerSec;
		if (!timerInited) {
			QueryPerformanceFrequency(&ticksPerSec);
			timerInited = true;
		}
		LARGE_INTEGER ttt1;
		QueryPerformanceCounter (&ttt1);
		float timeTaken = float(double(ttt1.QuadPart-s_Time0.QuadPart) / double(ticksPerSec.QuadPart));
		return timeTaken;
	}

#elif defined(__APPLE__)
	#include <sys/time.h>
	static timeval s_Time0;
	static void TimerBegin()
	{
		gettimeofday(&s_Time0, NULL);
	}
	static float TimerEnd()
	{
		timeval ttt1;
		gettimeofday( &ttt1, NULL );
		timeval ttt2;
		timersub( &ttt1, &s_Time0, &ttt2 );
		float timeTaken = ttt2.tv_sec + ttt2.tv_usec * 1.0e-6f;

		return timeTaken;
	}	
#elif defined(EMSCRIPTEN)
	#include "emscripten.h"
	static double s_Time0;
	static void TimerBegin()
	{
		s_Time0 = emscripten_get_now();
	}
	static float TimerEnd()
	{
		double t = emscripten_get_now();
		float timeTaken = (t - s_Time0) * 0.001;
		return timeTaken;
	}	
#else
	#error "Unknown platform, timer code missing"
#endif



// ------------------------------------------------------------------------------------
// Data sets & reading them from file


typedef std::vector<std::string> WordList;

static WordList g_Words;
static size_t g_TotalSize;

static void ReadWords(const char* filename)
{
	g_Words.clear();
	FILE* f = fopen(filename, "rb");
	if (!f)
	{
		printf("error: can't open dictionary file '%s'\n", filename);
		return;
	}
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buffer = new char[size];
	fread(buffer, size, 1, f);

	size_t pos = 0;
	size_t wordStart = 0;
	g_TotalSize = 0;
	while (pos < size)
	{
		if (buffer[pos] == '\n')
		{
			std::string word;
			word.assign(buffer+wordStart, pos-wordStart);
			// remove any trailing windows newlines
			while (!word.empty() && word[word.size()-1] == '\r')
				word.pop_back();
			g_Words.push_back(word);
			g_TotalSize += word.size();
			wordStart = pos+1;
		}
		++pos;
	}
	
	delete[] buffer;
	fclose(f);
}


// ------------------------------------------------------------------------------------
// Hash function testing code


template<typename Hasher>
void TestHash(const Hasher& hasher, const char* name)
{
	// hash all the entries; do several iterations and pick smallest time
	typename Hasher::HashType hashsum = 0x1234;
	float minsec = 1.0e6f;
	for (int iterations = 0; iterations < 5; ++iterations)
	{
		TimerBegin();
		for (size_t i = 0, n = g_Words.size(); i != n; ++i)
		{
			hashsum ^= hasher(g_Words[i]);
		}
		float sec = TimerEnd();
		if (sec < minsec)
			minsec = sec;
	}
	// MB/s
	double mbps = (g_TotalSize / 1024.0 / 1024.0) / minsec;

	// test for "hash quality":
	// unique hashes found in all the entries (#entries - uniq == how many collisions found)
	std::set<typename Hasher::HashType> uniq;
	// unique hashes found in lowest 16 bits of the computed hash. Ideally this spreads evenly,
	// and for # of entries larger than 64k would approach 64k entries in the low bits.
	std::set<typename Hasher::HashType> uniq16;
	for (size_t i = 0, n = g_Words.size(); i != n; ++i)
	{
		typename Hasher::HashType h = hasher(g_Words[i]);
		uniq.insert(h);
		uniq16.insert(h & 0xFFFF);
	}
	size_t collisions = g_Words.size() - uniq.size();
	double fillOf16bit = uniq16.size() / 65536.0 * 100.0;

	// use hashsum in a fake way so that it's not completely compiled away by the optimizer
	printf("%15s: %8.0f MB/s, %5i collis, %5.2f%% fill at 16bit\n", name, mbps, (int)collisions, fillOf16bit + (hashsum & 0x7) * 0.0001);
}


// ------------------------------------------------------------------------------------
// Individual hash functions for use in the testing code above

struct HasherXXH32
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		return XXH32(s.data(), s.size(), 0x1234);
	}
};

struct HasherXXH64_32
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		return (HashType)XXH64(s.data(), s.size(), 0x1234);
	}
};

struct HasherXXH64
{
	typedef uint64_t HashType;
	HashType operator()(const std::string& s) const
	{
		return XXH64(s.data(), s.size(), 0x1234);
	}
};

struct HasherSpookyV2_64
{
	typedef uint64_t HashType;
	HashType operator()(const std::string& s) const
	{
		return SpookyHash::Hash64(s.data(), (int)s.size(), 0x1234);
	}
};

struct HasherMurmur2A
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		return MurmurHash2A(s.data(), (int)s.size(), 0x1234);
	}
};

struct HasherMurmur3_32
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		HashType res;
		MurmurHash3_x86_32(s.data(), (int)s.size(), 0x1234, &res);
		return res;
	}
};

struct HasherFNV
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		return FNVHash(s.c_str());
	}
};

struct HasherCRC32
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		HashType res;
		crc32(s.data(), (int)s.size(), 0x1234, &res);
		return res;
	}
};

struct HasherBadHashFunctionWeUsedToHave
{
	typedef uint32_t HashType;
	HashType operator()(const std::string& s) const
	{
		return hash_cstring()(s.c_str());
	}
};


// ------------------------------------------------------------------------------------
// Main program


static void DoTest(const char* filename)
{
	ReadWords(filename);
	if (g_Words.empty())
		return;
	printf("Testing on %s: %i entries (%.1f MB size, avg length %.1f)\n", filename, (int)g_Words.size(), g_TotalSize / 1024.0 / 1024.0, double(g_TotalSize) / g_Words.size());
	TestHash(HasherXXH32(), "xxHash32");
	TestHash(HasherXXH64_32(), "xxHash64-32");
	TestHash(HasherXXH64(), "xxHash64");
	TestHash(HasherSpookyV2_64(), "SpookyV2-64");
	TestHash(HasherMurmur2A(), "Murmur2A");
	TestHash(HasherMurmur3_32(), "Murmur3-32");
	TestHash(HasherCRC32(), "CRC32");
	TestHash(HasherFNV(), "FNV");
	TestHash(djb2_hash(), "djb2");
	TestHash(HasherBadHashFunctionWeUsedToHave(), "BadHash");
}

int main()
{
	DoTest("test-words.txt");
	DoTest("test-filenames.txt");
	DoTest("test-code.txt");
	return 0;
}
