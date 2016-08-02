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

#ifdef __APPLE__
#include <sys/time.h>
#endif

#ifdef _MSC_VER
static LARGE_INTEGER s_Time0;
#else
static timeval s_Time0;
#endif


static void TimerBegin()
{
	#ifdef _MSC_VER
	QueryPerformanceCounter (&s_Time0);
	#else
	gettimeofday(&s_Time0, NULL);
	#endif
}


static float TimerEnd()
{
	#ifdef _MSC_VER

	static bool timerInited = false;
	static LARGE_INTEGER ticksPerSec;
	if (!timerInited) {
		QueryPerformanceFrequency(&ticksPerSec);
		timerInited = true;
	}
	LARGE_INTEGER ttt1;
	QueryPerformanceCounter (&ttt1);
	float timeTaken = float(double(ttt1.QuadPart-s_Time0.QuadPart) / double(ticksPerSec.QuadPart));

	#else

	timeval ttt1;
	gettimeofday( &ttt1, NULL );
	timeval ttt2;
	timersub( &ttt1, &s_Time0, &ttt2 );
	float timeTaken = ttt2.tv_sec + ttt2.tv_usec * 1.0e-6f;

	#endif

	return timeTaken;
}


extern void crc32 (const void * key, int len, uint32_t seed, void * out);


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

struct Results
{
	double mbps;
	size_t collisions;
	size_t unique16bit;
	unsigned hashsum;
};

template<typename HashType, typename Hasher>
void TestHash(const Hasher& hasher, Results& outResults)
{
	HashType hashsum = 0x1234;
	TimerBegin();
	for (size_t i = 0, n = g_Words.size(); i != n; ++i)
	{
		hashsum ^= hasher(g_Words[i]);
	}
	float sec = TimerEnd();
	outResults.mbps = (g_TotalSize / 1024.0 / 1024.0) / sec;

	std::set<HashType> uniq;
	std::set<HashType> uniq16;
	for (size_t i = 0, n = g_Words.size(); i != n; ++i)
	{
		HashType h = hasher(g_Words[i]);
		uniq.insert(h);
		uniq16.insert(h & 0xFFFF);
	}
	outResults.collisions = g_Words.size() - uniq.size();
	outResults.unique16bit = uniq16.size();
	outResults.hashsum = (unsigned)hashsum;
}


struct HasherXXH32
{
	uint32_t operator()(const std::string& s) const
	{
		return XXH32(s.data(), s.size(), 0x1234);
	}
};

struct HasherXXH64_32
{
	uint32_t operator()(const std::string& s) const
	{
		return (uint32_t)XXH64(s.data(), s.size(), 0x1234);
	}
};

struct HasherXXH64
{
	uint64_t operator()(const std::string& s) const
	{
		return XXH64(s.data(), s.size(), 0x1234);
	}
};

struct HasherMurmur2A
{
	uint32_t operator()(const std::string& s) const
	{
		return MurmurHash2A(s.data(), (int)s.size(), 0x1234);
	}
};

struct HasherMurmur3_32
{
	uint32_t operator()(const std::string& s) const
	{
		uint32_t res;
		MurmurHash3_x86_32(s.data(), (int)s.size(), 0x1234, &res);
		return res;
	}
};


struct HasherFNV
{
	uint32_t operator()(const std::string& s) const
	{
		return FNVHash(s.c_str());
	}
};

struct HasherCRC32
{
	uint32_t operator()(const std::string& s) const
	{
		uint32_t res;
		crc32(s.data(), (int)s.size(), 0x1234, &res);
		return res;
	}
};

struct HasherCString
{
	uint32_t operator()(const std::string& s) const
	{
		return hash_cstring()(s.c_str());
	}
};

static void PrintResults(const char* name, const Results& res)
{
	printf("%15s: %8.1f MB/s, %5i collis, %5.2f%% fill at 16bit\n", name, res.mbps, (int)res.collisions, res.unique16bit / 65536.0 * 100.0);
}

static void DoTest(const char* filename)
{
	ReadWords(filename);
	if (g_Words.empty())
		return;
	printf("Testing on %s: %i words (%.1f MB size, avg len %.1f)\n", filename, (int)g_Words.size(), g_TotalSize / 1024.0 / 1024.0, double(g_TotalSize) / g_Words.size());
	Results resXXH32; TestHash<uint32_t, HasherXXH32>(HasherXXH32(), resXXH32);
	Results resXXH64_32; TestHash<uint32_t, HasherXXH64_32>(HasherXXH64_32(), resXXH64_32);
	Results resXXH64; TestHash<uint64_t, HasherXXH64>(HasherXXH64(), resXXH64);
	Results resMurmur2A; TestHash<uint32_t, HasherMurmur2A>(HasherMurmur2A(), resMurmur2A);
	Results resMurmur3_32; TestHash<uint32_t, HasherMurmur3_32>(HasherMurmur3_32(), resMurmur3_32);
	Results resCRC32; TestHash<uint32_t, HasherCRC32>(HasherCRC32(), resCRC32);
	Results resFNV; TestHash<uint32_t, HasherFNV>(HasherFNV(), resFNV);
	Results resDJB; TestHash<uint32_t, djb2_hash>(djb2_hash(), resDJB);
	Results resCString; TestHash<uint32_t, HasherCString>(HasherCString(), resCString);
	PrintResults("xxHash32", resXXH32);
	PrintResults("xxHash64-32", resXXH64_32);
	PrintResults("xxHash64", resXXH64);
	PrintResults("Murmur2A", resMurmur2A);
	PrintResults("Murmur3-32", resMurmur3_32);
	PrintResults("CRC32", resCRC32);
	PrintResults("FNV", resFNV);
	PrintResults("DJB2", resDJB);
	PrintResults("CString", resCString);
}

int main()
{
	DoTest("test-words.txt");
	DoTest("test-filenames.txt");
	DoTest("test-code.txt");
	return 0;
}
