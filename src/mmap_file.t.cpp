#include <mmap_file.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unit_test.h>

using namespace std;
using namespace FileUtils;

namespace { // <anonymous>

  void testMmapFileBasics(UnitTestControl &utc)
  {
    const string &tmpFileName = tmpnam(NULL);
    
    const string &testString = "The quick brown fox jumped over the lazy dog.";
    const off_t offset = 10; // offset into file
    const off_t size = 0x1400; // size to read/write;

    {
      MmapFile file(tmpFileName);

      char *ptr = file.getWritePtr<char>(offset, size);
      
      TEST_ASSERT(utc, ptr != NULL );
      
      struct stat st;
      TEST_ASSERT(utc, 0 == stat(tmpFileName.c_str(), &st));
      TEST_ASSERT(utc, offset + size == st.st_size);
      
      strcpy(ptr, testString.c_str());

      const char *cstr = file.getReadPtr<char>(offset + size, size);

      uint32_t *p = file.getWritePtr<uint32_t>(4);
      *p = 0xdeadbeef;

      TEST_ASSERT(utc, NULL == cstr);
    }

    ifstream ifs(tmpFileName.c_str());

    char buf[offset + size + 1] = {1};

    ifs.get(buf, sizeof(buf));
    
    for(int i = 0; i < 4; ++i)
      TEST_ASSERT(utc, '\0' == buf[i]);

    TEST_ASSERT(utc, 0xdeadbeef == *(reinterpret_cast<uint32_t *>(buf + 4)));

    for(int i = 4 + sizeof(uint32_t); i < offset; ++i)
      TEST_ASSERT(utc, '\0' == buf[i]);

    TEST_ASSERT(utc, 0 == strncmp(buf + offset, testString.c_str(), 
				  testString.size()));

    for(int i = offset + testString.size(); i < size; ++i) 
      TEST_ASSERT(utc, '\0' == buf[i]);

    {
      MmapFile file(tmpFileName);
      const char *ptr = file.getReadPtr<char>(offset, size);

      TEST_ASSERT(utc, 0 == strncmp(ptr, testString.c_str(), testString.size()));

      file.trim(10);
      TEST_ASSERT(utc, 10 == file.size());
      struct stat st;
      TEST_ASSERT(utc, 0 == stat(tmpFileName.c_str(), &st));
      TEST_ASSERT(utc, 10 == st.st_size);

      file.clear();
      TEST_ASSERT(utc, 0 == file.size());
      TEST_ASSERT(utc, 0 == stat(tmpFileName.c_str(), &st));
      TEST_ASSERT(utc, 0 == st.st_size);
      TEST_ASSERT(utc, NULL == file.getReadPtr<char>(0, 4));
      TEST_ASSERT(utc, NULL == file.getReadPtr<char>(4, 4));
    }
    unlink(tmpFileName.c_str());

    {
      bool didThrow = false;
      try {
	MmapFile file("");
	(void)file;
      }catch(const exception &) {
	didThrow = true;
      }

      TEST_ASSERT(utc, didThrow);
    }
  }

  void testMmapReadOnlyFile(UnitTestControl &utc)
  {
    const string &tmpFileName = tmpnam(NULL);

    // touch a temporary file w/ r--r--r-- permissions
    {
      int fd = open(tmpFileName.c_str(), O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
      assert( fd > 0 );
      close(fd);
    }

    bool didCatchException = false;
    try {
      MmapFile m(tmpFileName);
    }catch(const exception &e) {
      didCatchException = true;
    }
    TEST_ASSERT(utc, didCatchException);
    unlink(tmpFileName.c_str());
  }
} // end namespace <anonymous>

REGISTER_TEST(testMmapFileBasics, &::testMmapFileBasics)
REGISTER_TEST(testMmapReadOnlyFile, &::testMmapReadOnlyFile)
