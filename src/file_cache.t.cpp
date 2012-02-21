#include <file_cache.h>
#include <cstdio>
#include <memory>
#include <unit_test.h>

using namespace std;
using namespace FileCache;

namespace { // <anonymous>
  
  void testFileCache(UnitTestControl &utc)
  {
    const string tmpDirName = tmpnam(NULL);
    {
      string cmd = "mkdir -p ";
      cmd += tmpDirName;
      TEST_ASSERT(utc, 0 == system(cmd.c_str()));

      cmd = "chmod a=r ";
      cmd += tmpDirName;
      TEST_ASSERT(utc, 0 == system(cmd.c_str()));
    }

    TEST_ASSERT(utc, NULL == createFileCache(tmpDirName, 
					     vector<uint8_t>(1)));
    {
      string cmd = "chmod 0777 ";
      cmd += tmpDirName;
      TEST_ASSERT(utc, 0 == system(cmd.c_str()));
    }

    auto_ptr<Cache> cache(createFileCache(tmpDirName,
					  vector<uint8_t>(1)));


    TEST_ASSERT(utc, NULL != cache.get());

    vector<uint8_t> key(32, 0xBE); 
    vector<uint8_t> data(500, 0xEF);

    TEST_ASSERT(utc, not cache->hasObject(key));
    TEST_ASSERT(utc, cache->writeObject(key, data));
    TEST_ASSERT(utc, cache->hasObject(key));
    TEST_ASSERT(utc, cache->eraseObject(key));
    TEST_ASSERT(utc, not cache->hasObject(key));
    
    cache->setMaxSize(1000);
    TEST_ASSERT(utc, cache->writeObject(key, data));
    cache->setMaxSize(10);
    TEST_ASSERT(utc, cache->getCurrentSize() <= 10);
    TEST_ASSERT(utc, not cache->hasObject(key));
    cache->setMaxSize(-1);
    TEST_ASSERT(utc, cache->writeObject(key, data));
    vector<uint8_t> dataOut;
    TEST_ASSERT(utc, cache->readObject(key, dataOut));
    TEST_ASSERT(utc, data == dataOut);
    

    {
      string cmd = "rm -rf ";
      cmd += tmpDirName;
      TEST_ASSERT(utc, 0 == system(cmd.c_str()));
    }
  }
} // end namespace <anonymous>

REGISTER_TEST(testFileCache, &::testFileCache)
