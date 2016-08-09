#include "PlatformWrap.h"

#include "HashFunctions/city.h"
#include "HashFunctions/farmhash.h"
#include "HashFunctions/mum.h"
#include "HashFunctions/MurmurHash2.h"
#include "HashFunctions/MurmurHash3.h"
#include "HashFunctions/SimpleHashFunctions.h"
#include "HashFunctions/sha1.h"
#include "HashFunctions/SpookyV2.h"
#include "HashFunctions/xxhash.h"

#include <vector>
#include <string>
#include <set>
#include <map>
#include <stdio.h>
#include <math.h>


FILE* g_OutputFile = stdout;

extern void crc32 (const void * key, int len, uint32_t seed, void * out);
extern void md5_32 (const void * key, int len, uint32_t /*seed*/, void * out);
extern "C" int siphash(uint8_t *out, const uint8_t *in, uint64_t inlen, const uint8_t *k);


// ------------------------------------------------------------------------------------
// Data sets & reading them from file

struct DataSet
{
	DataSet() : totalSize(0) { }

	typedef std::pair<size_t,size_t> OffsetAndSize;

	std::string name;
	std::vector<char> buffer; // raw file contents
	std::vector<OffsetAndSize> entries; // entries in the file data array, each has offset and length
	size_t totalSize; // total size of data that will be hashed (file size, minus all entry delimiters)
};

static DataSet* ReadDataSet(const char* folderName, const char* filenameStr)
{
	std::string filename = std::string(filenameStr);
#	if PLATFORM_IOS || PLATFORM_XBOXONE
	filename.erase(0, 9); // remove TestData/ on iOS/XB1; files in resources only retain the filenames
#	endif

	std::string fullPath = std::string(folderName) + filename;
	FILE* f = fopen(fullPath.c_str(), "rb");
	if (!f)
	{
		fprintf(g_OutputFile, "error: can't open dataset file '%s'\n", filename.c_str());
		return NULL;
	}
	DataSet* data = new DataSet();
	data->name = filename;

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	data->buffer.resize(size);
	char* buffer = data->buffer.data();
	fread(buffer, size, 1, f);

	size_t pos = 0;
	size_t wordStart = 0;
	data->totalSize = 0;
	while (pos < size)
	{
		if (buffer[pos] == '\n')
		{
			size_t wordEnd = pos;
			// remove any trailing Windows style newlines
			while (wordEnd > wordStart+1 && buffer[wordEnd-1] == '\r')
				--wordEnd;
			data->entries.push_back(std::make_pair(wordStart, wordEnd-wordStart));
			data->totalSize += wordEnd-wordStart;
			wordStart = pos+1;
		}
		++pos;
	}
	
	fclose(f);
	return data;
}


// ------------------------------------------------------------------------------------
// Hash function testing code

inline uint32_t NextPowerOfTwo(uint32_t v)
{
	v -= 1;
	v |= v >> 16;
	v |= v >> 8;
	v |= v >> 4;
	v |= v >> 2;
	v |= v >> 1;
	return v + 1;
}


struct Result
{
	struct DataSetResult
	{
		DataSetResult() : hashsum(0), collisions(0), hashtabCollisionsIncrease(0) { }
		uint32_t hashsum;
		int collisions;
		float hashtabCollisionsIncrease; // % of how much hashtable collisions we'd get, compared to an ideal hash
	};
	struct PerfResult
	{
		PerfResult() : length(0), mbps(0), mbpsAligned(0) { }
		int length;
		int mbps;
		int mbpsAligned;
	};
	
	Result() : hashsum(0) { mbpsPerLength.reserve(32); }

	std::string name;
	std::vector<PerfResult> mbpsPerLength;
	std::vector<DataSetResult> datasets;
	uint32_t hashsum;
};


// http://stackoverflow.com/questions/9104504/expected-number-of-hash-collisions
static double CalculateExpectedCollisions(size_t bucketCount, size_t entryCount)
{
	double m = bucketCount;
	double n = entryCount;
	double e = n - m * (1 - pow((m-1)/m, n));
	return e;
}


template<typename Hasher>
void TestQualityOnDataSet(const DataSet& dataset, Result::DataSetResult& outResult)
{
	Hasher hasher;

	// test for "hash quality":
	// unique hashes found in all the entries (#entries - uniq == how many collisions found)
	std::set<typename Hasher::HashType> uniq;

	// unique buckets that we'd end up with, if we had a hashtable with a load factor of 0.8 that is
	// always power of two size.
	std::set<typename Hasher::HashType> uniqModulo;
	const size_t entryCount = dataset.entries.size();
	size_t hashtableSize = NextPowerOfTwo(entryCount / 0.8);

	double expectedCollisons = CalculateExpectedCollisions(hashtableSize, entryCount);
	outResult.hashsum = 0;
	for (size_t i = 0; i != entryCount; ++i)
	{
		typename Hasher::HashType h = hasher(dataset.buffer.data() + dataset.entries[i].first, dataset.entries[i].second);
		outResult.hashsum ^= (uint32_t)h;
		uniq.insert(h);
		uniqModulo.insert(h % hashtableSize);
	}
	outResult.collisions = (int)(entryCount - uniq.size());

	double hashtabCollisions = entryCount - uniqModulo.size();
	double collisionsIncrease = (hashtabCollisions / expectedCollisons - 1.0) * 100;
	if (collisionsIncrease < 0)
		collisionsIncrease = 0;
	outResult.hashtabCollisionsIncrease = collisionsIncrease;
}


const size_t kSyntheticDataTotalSize = 1024 * 1024 * 1;
#if PLATFORM_WEBGL
const int kSyntheticDataIterations = 3;
#else
const int kSyntheticDataIterations = 9;
#endif



// synthetic hash performance test on various string lengths
template<typename Hasher>
void TestPerformancePerLength(const std::vector<uint8_t>& data, bool aligned, Result& outResult)
{
	Hasher hasher;

	int step = 2;
	int index = 0;
	for (int len = 2; len < 5000; len += step, step += step/2, ++index)
	{
		size_t dataLen = data.size();

		const uint8_t* dataPtr = data.data();
		TimerBegin();
		size_t pos = 0;
		size_t lenAligned = len;
		//if (aligned)
			lenAligned = (lenAligned + 63) & ~63;
		size_t totalBytes = 0;
		while (pos + len < dataLen)
		{
			outResult.hashsum ^= hasher(dataPtr + pos, len);
			pos += lenAligned;
			totalBytes += len;
		}
		float sec = TimerEnd();

		// MB/s
		float mbps = (float)((totalBytes / 1024.0 / 1024.0) / sec);
		if (index < outResult.mbpsPerLength.size())
		{
			// if we got higher MB/s, use that (i.e. out of all iterations, we pick fastest one)
			assert(outResult.mbpsPerLength[index].length == len);
			if (aligned)
			{
				if (mbps > outResult.mbpsPerLength[index].mbpsAligned)
					outResult.mbpsPerLength[index].mbpsAligned = mbps;
			}
			else
			{
				if (mbps > outResult.mbpsPerLength[index].mbps)
					outResult.mbpsPerLength[index].mbps = mbps;
			}
		}
		else
		{
			// add result if no previous iterations did it yet
			Result::PerfResult res;
			res.length = len;
			if (aligned)
				res.mbpsAligned = mbps;
			else
				res.mbps = mbps;
			outResult.mbpsPerLength.push_back(res);
		}
	}
}


// ------------------------------------------------------------------------------------
// Individual hash functions for use in the testing code above

struct HasherXXH32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return XXH32(data, size, 0x1234); }
};
struct HasherXXH64_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return (HashType)XXH64(data, size, 0x1234); }
};
struct HasherXXH64 : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { return XXH64(data, size, 0x1234); }
};

struct HasherSpookyV2_64 : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { return SpookyHash::Hash64(data, (int)size, 0x1234); }
};

struct HasherMurmur2A : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return MurmurHash2A(data, (int)size, 0x1234); }
};
struct HasherMurmur3_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { HashType res; MurmurHash3_x86_32(data, (int)size, 0x1234, &res); return res; }
};
struct HasherMurmur3_x64_128 : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { HashType res[2]; MurmurHash3_x64_128(data, (int)size, 0x1234, &res); return res[0]; }
};

struct HasherMum_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return (uint32_t)mum_hash(data, size, 0x1234); }
};
struct HasherMum : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { return mum_hash(data, size, 0x1234); }
};

struct HasherCity32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return CityHash32((const char*)data, size); }
};
struct HasherCity64_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return (uint32_t)CityHash64((const char*)data, size); }
};
struct HasherCity64 : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { return CityHash64((const char*)data, size); }
};

struct HasherFarm32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return util::Hash32((const char*)data, size); }
};
struct HasherFarm64_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { return (uint32_t)util::Hash64((const char*)data, size); }
};
struct HasherFarm64 : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { return util::Hash64((const char*)data, size); }
};

// Reference SipHash implementation, https://github.com/veorq/SipHash
static const uint8_t kSipHashKey[16] = {0x75,0x4E,0x3F,0x38, 0x21,0x0A,0xFE,0x71, 0x9D,0xDC,0x54,0x72, 0x09,0x1A,0xD4,0x79};
struct HasherSipRef_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { uint64_t res; siphash((uint8_t*)&res, (const uint8_t*)data, size, kSipHashKey); return (HashType)res; }
};
struct HasherSipRef : public Hasher64Bit
{
	HashType operator()(const void* data, size_t size) const { uint64_t res; siphash((uint8_t*)&res, (const uint8_t*)data, size, kSipHashKey); return res; }
};

struct HasherCRC32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const { HashType res; crc32(data, (int)size, 0x1234, &res); return res; }
};
struct HasherMD5_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const
	{
		// Could do this on Apple platforms, from <CommonCrypto/CommonDigest.h> -- quick test
		// shows that it's around 25% faster, but that does not change things much.
		//uint32_t res[4]; CC_MD5(data, (unsigned int)size, (unsigned char*)res); return res[0];
		HashType res; md5_32(data, (int)size, 0x1234, &res); return res;
	}
};
struct HasherSHA1_32 : public Hasher32Bit
{
	HashType operator()(const void* data, size_t size) const
	{
		// Could do this on Apple platforms, from <CommonCrypto/CommonDigest.h> -- quick test
		// shows that it's around 25% faster, but that does not change things much.
		//uint32_t res[5]; CC_SHA1(data, (unsigned int)size, (unsigned char*)res); return res[0];
		HashType res; sha1_32a(data, (int)size, 0x1234, &res); return res;
	}
};


// ------------------------------------------------------------------------------------
// Main program

static std::vector<DataSet*> g_DataSets;
static std::vector<uint8_t> g_SyntheticData;
static std::vector<Result> g_Results;

typedef void (*TestHashQualityFunc)(const DataSet& dataset, Result::DataSetResult& outResult);
typedef void (*TestHashPerfFunc)(const std::vector<uint8_t>& data, bool aligned, Result& outResult);

struct HashToTest
{
	const char* name;
	TestHashQualityFunc qualityFunc;
	TestHashPerfFunc perfFunc;
	bool excludeFromPerf;
};
static std::vector<HashToTest> g_Hashes;

static void AddHash(const char* name, TestHashQualityFunc qualityFunc, TestHashPerfFunc perfFunc, bool excludeFromPerf)
{
	HashToTest h;
	h.name = name;
	h.qualityFunc = qualityFunc;
	h.perfFunc = perfFunc;
	h.excludeFromPerf = excludeFromPerf;
	g_Hashes.push_back(h);
}


static void CreateSyntheticData()
{
	g_SyntheticData.resize(kSyntheticDataTotalSize);
	for (size_t i = 0; i < kSyntheticDataTotalSize; ++i)
		g_SyntheticData[i] = i;
}

static void LoadDataSets(const char* folderName)
{
	// Basic collisions / hash quality tests on some real world data I had lying around:
	// - Dictionary of English words from /usr/share/dict/words
	// - A bunch of file relative paths + filenames from several Unity projects & test suites.
	//   Imaginary use case, hashing filenames in some asset database / file storage system.
	// - C++ source code, this was partial Unity sourcecode dump. I'm not releasing this one :),
	//   but it was 6069 entries, 43.7MB total size, average size 7546.6 bytes.
	// - Mostly binary data. I instrumented hash function calls, as used in Unity engine graphics
	//   related parts, to dump actually hashed data into a log file. Unlike the test sets above,
	//   most of the data here is binary, and represents snapshots of some internal structs in
	//   memory.
	DataSet* data;
	data = ReadDataSet(folderName, "TestData/test-words.txt"); if (data) g_DataSets.push_back(data);
	data = ReadDataSet(folderName, "TestData/test-filenames.txt"); if (data) g_DataSets.push_back(data);
	data = ReadDataSet(folderName, "TestData/test-code.txt"); if (data) g_DataSets.push_back(data);
	data = ReadDataSet(folderName, "TestData/test-binary.bin"); if (data) g_DataSets.push_back(data);
}

static void PrintResults()
{
	fprintf(g_OutputFile, "**** Quality evaluation\n");
	for (size_t id = 0; id < g_DataSets.size(); ++id)
	{
		const DataSet& data = *g_DataSets[id];
		fprintf(g_OutputFile, "%s, %i entries, %.1f MB size, avg length %.1f\n", data.name.c_str(), (int)data.entries.size(), data.totalSize / 1024.0 / 1024.0, double(data.totalSize) / data.entries.size());
		fprintf(g_OutputFile, "HashAlgorithm   Colis HTColsIncrease hashsum\n");
		for (size_t ia = 0; ia < g_Results.size(); ++ia)
		{
			const Result::DataSetResult& res = g_Results[ia].datasets[id];
			fprintf(g_OutputFile, "%15s %4i %6i           %08x\n", g_Results[ia].name.c_str(), res.collisions, (int)res.hashtabCollisionsIncrease, res.hashsum);
		}
	}

	fprintf(g_OutputFile, "\n**** Performance evaluation, MB/s\n");
	fprintf(g_OutputFile, "DataSize,");
	for (size_t ia = 0; ia < g_Hashes.size(); ++ia)
	{
		if (g_Hashes[ia].excludeFromPerf)
			continue;
		fprintf(g_OutputFile, "%s,", g_Hashes[ia].name);
	}
	fprintf(g_OutputFile, "\n");
	for (size_t is = 0; is < g_Results[0].mbpsPerLength.size(); ++is)
	{
		fprintf(g_OutputFile, "%i,", g_Results[0].mbpsPerLength[is].length);
		for (size_t ia = 0; ia < g_Results.size(); ++ia)
		{
			if (g_Hashes[ia].excludeFromPerf)
				continue;
			fprintf(g_OutputFile, "%i,", (int)g_Results[ia].mbpsPerLength[is].mbps);
		}
		fprintf(g_OutputFile, "\n");
	}
	fprintf(g_OutputFile, "\n");

	fprintf(g_OutputFile, "\n**** Aligned data performance evaluation, MB/s\n");
	fprintf(g_OutputFile, "DataSize,");
	for (size_t ia = 0; ia < g_Hashes.size(); ++ia)
	{
		if (g_Hashes[ia].excludeFromPerf)
			continue;
		fprintf(g_OutputFile, "%s,", g_Hashes[ia].name);
	}
	fprintf(g_OutputFile, "\n");
	for (size_t is = 0; is < g_Results[0].mbpsPerLength.size(); ++is)
	{
		fprintf(g_OutputFile, "%i,", g_Results[0].mbpsPerLength[is].length);
		for (size_t ia = 0; ia < g_Results.size(); ++ia)
		{
			if (g_Hashes[ia].excludeFromPerf)
				continue;
			fprintf(g_OutputFile, "%i,", (int)g_Results[ia].mbpsPerLength[is].mbpsAligned);
		}
		fprintf(g_OutputFile, "\n");
	}
	fprintf(g_OutputFile, "\n");
}

extern "C" void HashFunctionsTestEntryPoint(const char* folderName)
{
	// load data
	fprintf(g_OutputFile, "Loading data\n");
	CreateSyntheticData();
	LoadDataSets(folderName);
	g_Results.reserve(50);
	
	// setup hash functions to test
#	define ADDHASH(name,clazz,exclude) AddHash(name, TestQualityOnDataSet<clazz>, TestPerformancePerLength<clazz>, exclude)

	ADDHASH("xxHash64", HasherXXH64, 0);
	ADDHASH("xxHash64-32", HasherXXH64_32, 1);
	ADDHASH("City64", HasherCity64, 0);
	ADDHASH("City64-32", HasherCity64_32, 1);
	ADDHASH("Mum", HasherMum, 0);
	ADDHASH("Farm64", HasherFarm64, 0);
	ADDHASH("Farm64-32", HasherFarm64_32, 1);
	ADDHASH("SpookyV2-64", HasherSpookyV2_64, 0);

	ADDHASH("xxHash32", HasherXXH32, 0);
	ADDHASH("Murmur3-X64-64", HasherMurmur3_x64_128, 0);
	ADDHASH("Murmur2A", HasherMurmur2A, 0);
	ADDHASH("Murmur3-32", HasherMurmur3_32, 0);
	ADDHASH("Mum-32", HasherMum_32, 1);
	ADDHASH("City32", HasherCity32, 0);
	ADDHASH("Farm32", HasherFarm32, 0);
	ADDHASH("SipRef", HasherSipRef, 0);
	ADDHASH("SipRef-32", HasherSipRef_32, 1);
	ADDHASH("CRC32", HasherCRC32, 0);
	ADDHASH("MD5-32", HasherMD5_32, 0);
	ADDHASH("SHA1-32", HasherSHA1_32, 0);
	ADDHASH("FNV-1a", FNV1aHash, 0);
	ADDHASH("FNV-1amod", FNV1aModifiedHash, 0);
	ADDHASH("djb2", djb2_hash, 0);
	ADDHASH("SDBM", SDBM_hash, 0);
	ADDHASH("ELFLikeBadHash", ELF_Like_Bad_Hash, 1);

#	undef ADDHASH

	// do quality evaluations on all hash functions
	fprintf(g_OutputFile, "Doing quality evals...\n  ");
	for (size_t i = 0; i < g_Hashes.size(); ++i)
	{
		const HashToTest& hash = g_Hashes[i];
		g_Results.push_back(Result());
		Result& res = g_Results.back();
		res.name = hash.name;
		res.datasets.resize(g_DataSets.size());
		fprintf(g_OutputFile, "%s ", hash.name);
		fflush(g_OutputFile);
		for (size_t id = 0, nd = g_DataSets.size(); id != nd; ++id)
		{
			hash.qualityFunc(*g_DataSets[id], res.datasets[id]);
		}
	}
	fprintf(g_OutputFile, "\n");

	// Do performance evaluations on all hash functions.
	// Perform several iterations: for (iterations) { for (hashes) { DoPerfTest } }.
	// Iterations are performed in the outer loop, so that any clock changes affect
	// all hash functions in a fair way.
	fprintf(g_OutputFile, "Doing performance evals...\n");
	for (int iter = 0; iter < kSyntheticDataIterations; ++iter)
	{
		fprintf(g_OutputFile, "  iter %i/%i\n", iter+1, kSyntheticDataIterations);
		for (size_t i = 0; i < g_Hashes.size(); ++i)
		{
			const HashToTest& hash = g_Hashes[i];
			if (hash.excludeFromPerf)
				continue;
			Result& res = g_Results[i];
			hash.perfFunc(g_SyntheticData, false, res);
		}
	}
	fprintf(g_OutputFile, "  aligned data...\n");
	for (int iter = 0; iter < kSyntheticDataIterations; ++iter)
	{
		fprintf(g_OutputFile, "  iter %i/%i\n", iter+1, kSyntheticDataIterations);
		for (size_t i = 0; i < g_Hashes.size(); ++i)
		{
			const HashToTest& hash = g_Hashes[i];
			if (hash.excludeFromPerf)
				continue;
			Result& res = g_Results[i];
			hash.perfFunc(g_SyntheticData, true, res);
		}
	}

	// print results
	PrintResults();
}


// iOS & XB1 has main entry points elsewhere
#if !PLATFORM_IOS && !PLATFORM_XBOXONE

int main()
{
	#if PLATFORM_PS4
	const char* folderName = "/app0/";
	#else
	const char* folderName = "";
	#endif
	HashFunctionsTestEntryPoint(folderName);
	return 0;
}

#endif // #if !PLATFORM_IOS && !PLATFORM_XBOXONE

