#include <heap_file.h>
#include <assert.h>
#include <byte_order.h>
#include <cstring>
#include <fstream>
#include <heap_blob.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <unit_test.h>
#include <vector>

using namespace EndianUtils;
using namespace FileUtils;
using namespace FileUtils::StructuredFiles;
using namespace std;

namespace { // <anonymous>

  void testInit(UnitTestControl &utc)
  {
    const string &tmpFileName = tmpnam(NULL);

    uint64_t offsets[]  = {64, 90, 150, 1100};
    uint32_t sizes[]    = {1, 10, 950, 20};
    uint32_t hashCode[] = {0, 1, 2, 3};
    std::vector<Record> entries;
    
    for(size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
      Record e(offsets[i], hashCode[i], sizes[i]);
      entries.push_back(e);
    }

    for(size_t i = 0; i < entries.size() - 1; ++i) {
      const Record &left = entries[i];
      const Record &right = entries[1+i];
      assert(left.offset() < right.offset());
      assert(left.offset() + left.size() <= right.offset());
    }
    uint64_t dataSize =  entries.back().offset() + 
      entries.back().size();
    uint32_t metadataSize = Record::SERIALIZED_SIZE * entries.size();

    {
      MmapFile file(tmpFileName);
      char * const ptr = file.getWritePtr<char>(0, dataSize + sizeof(uint32_t) + metadataSize);

      TEST_ASSERT(utc, ptr != NULL);

      {
	uint64_t location = h2n(dataSize);
	memcpy(ptr, &location, sizeof(location));
	
	uint32_t numEntries = h2n(static_cast<uint32_t>(entries.size()));
	memcpy(ptr+dataSize, &numEntries, sizeof(numEntries));
      }
      char *p = ptr + dataSize + sizeof(metadataSize);

      for(size_t i = 0; i < entries.size(); ++i) {
	assert(Record::SERIALIZED_SIZE == entries[i].serialize(p));
      }

      assert(static_cast<uint64_t>(file.size()) == dataSize + sizeof(metadataSize) + metadataSize);
    }
    
    struct stat st;
    assert(0 == stat(tmpFileName.c_str(), &st));
    assert(static_cast<uint64_t>(st.st_size) == dataSize + sizeof(metadataSize) + metadataSize);
		

    try {
      for(int DOITTWICE = 0; DOITTWICE < 2; ++DOITTWICE)
      {
	{
	  HeapFile file(tmpFileName);

	  {
	    typedef RecordMap::const_iterator ConstItr;

	    TEST_ASSERT(utc, file.getIndex().numAllocatedRecords() == entries.size());

	    for(size_t i = 0; i < sizeof(hashCode)/sizeof(hashCode[0]); ++i)
	    {
	      pair<ConstItr, ConstItr> range = file.getIndex().allocRecords().equal_range(hashCode[i]);
	      int count = 0;
	      for(; range.first != range.second; ++range.first) 
	      {
		TEST_ASSERT(utc, entries[i] == *(range.first->second));
		++count;
	      }
	      TEST_ASSERT(utc, 1 == count);
	    }

	    const HeapFile &f = file;
	    TEST_ASSERT(utc, f.getIndex().allRecords().size() - f.getIndex().numAllocatedRecords() 
			== f.getIndex().numFreeRecords());
	  }
	  {
	    typedef RecordList::const_iterator ConstItr;
	    const RecordList &blockList = file.getIndex().allRecords();

	    ConstItr second = blockList.begin();
	    ++second;
	    for(ConstItr first = blockList.begin(), itrEnd = blockList.end(); 
		second != itrEnd; ++first, ++second) {
	      TEST_ASSERT(utc, (**first).sharesRightBoundaryWith(**second));
	    }
	  }
	}
	
	assert(0 == stat(tmpFileName.c_str(), &st));
	assert(static_cast<uint64_t>(st.st_size) == dataSize + sizeof(metadataSize) + metadataSize);
      }

    }catch(const std::exception &e) {
      TEST_ASSERT(utc, !"Caught an unexpected exception");
      utc.errStream() 
	<< "Caught exception  w/ msg " 
	<< e.what() 
	<< endl;
    }

    unlink(tmpFileName.c_str());

    {
      try {
	HeapFile file(tmpFileName);
	(void)file;
      }catch(...) {
	TEST_ASSERT(utc, !"Constructing an HeapFile from a non-existent file failed");
      }

      TEST_ASSERT(utc, 0 == stat(tmpFileName.c_str(), &st)); // does file exist?
      TEST_ASSERT(utc, static_cast<uint64_t>(st.st_size) == 0); // should be empty
      unlink(tmpFileName.c_str());
    }

    {
      bool didThrow = false;
      try {
	HeapFile file("");
	(void)file;
      }catch(std::exception &) {
	didThrow = true;
      }

      TEST_ASSERT(utc, didThrow);
    }
  }


  uint8_t Rand() { return rand()%0x0FF; }
  
  void print(ostream &strm, const vector<uint8_t> &v)
  {
    for(size_t i = 0; i < v.size(); ++i)
    {
      char buf[3] = {0};
      snprintf(buf, sizeof(buf)/sizeof(buf[0]), "%02X", v[i]);
      strm << buf;
      
    }
  }

  void read(istream &strm, vector<uint8_t> &v, char delim)
  {
    char tmp = '\0'; strm.get(tmp);
    for(int i = 0; tmp != delim; strm.get(tmp), ++i) {
      char nibble = 0;

      if ('0' <= tmp and tmp <= '9') {
	nibble = tmp - '0'; 
      }else if ('A' <= tmp and tmp <= 'F' ) {
	nibble = tmp - 'A' + 10; 
      }else if ('a' <= tmp and tmp <= 'f' ) {
	nibble = tmp - 'a' + 10;
      }else {
	assert(!"fail");
      }

      assert (nibble < 16);
      if (0 == i%2) {
	v.push_back(static_cast<uint8_t>(nibble << 4));
      }else {
	v[i/2] = v[i/2] | (0x0f & nibble); 
      }
    }
  }

  void testHeapFileReadWriteOps(UnitTestControl &utc)
  {
    const string tmpFileName = tmpnam(NULL);
    const string txtFileName = tmpFileName + ".txt";

    {
      fstream fstrm(txtFileName.c_str(), ios_base::out);

      HeapFile file(tmpFileName);
      TEST_ASSERT(utc, file.size() == 0);
      TEST_ASSERT(utc, not file.hasBlob(vector<uint8_t>()));

      vector<uint8_t> fauxId(10, 10);
      TEST_ASSERT(utc, not file.hasBlob(fauxId));

      std::vector<uint8_t> key;
      key.reserve(32);

      std::vector<uint8_t> data;
      data.reserve(102400);

      for(int i = 0; i < 100; ++i) {
	uint8_t keySize = Rand() % 32;
	key.resize(keySize);
	std::generate(key.begin(), key.end(), Rand);
	
	uint32_t dataSize = rand() % (102400);
	data.resize(dataSize);
	std::generate(data.begin(), data.end(), Rand);


	print(fstrm, key);
	fstrm << "\t";
	print(fstrm, data);
	fstrm << endl;

	TEST_ASSERT(utc, file.writeBlob(key, data));
	TEST_ASSERT(utc, file.hasBlob(key));
      }


      fstrm.close();
    }

    {
      HeapFile file(tmpFileName);
      TEST_ASSERT(utc, file.size() != 0);
      fstream fstrm(txtFileName.c_str(), ios_base::in);

      for(int i = 0; i < 1; ++i) {
	std::vector<uint8_t> key;
	read(fstrm, key, '\t');
	std::vector<uint8_t> data;
	read(fstrm, data, '\n');

	TEST_ASSERT(utc, file.hasBlob(key));
	std::vector<uint8_t> dataOut;
	TEST_ASSERT(utc, file.getBlob(key, dataOut));
	TEST_ASSERT(utc, data == dataOut);
      }
    }
    unlink(tmpFileName.c_str());
    unlink(txtFileName.c_str());
  }

  void testHeapFileDeletes(UnitTestControl &utc)
  {
    const string tmpFileName = tmpnam(NULL);
    {
      HeapFile file(tmpFileName);
      TEST_ASSERT(utc, file.size() == 0);

      vector<uint8_t> key(30);
      vector<uint8_t> data(1024);
      generate(key.begin(), key.end(), Rand);
      generate(data.begin(), data.end(), Rand);

      TEST_ASSERT(utc, file.writeBlob(key, data));
      TEST_ASSERT(utc, file.hasBlob(key));
      TEST_ASSERT(utc, file.eraseBlob(key));
      TEST_ASSERT(utc, not file.hasBlob(key));
    }

    {
      HeapFile file(tmpFileName);
      TEST_ASSERT(utc, file.size() == 0);
      TEST_ASSERT(utc, file.getIndex().numAllocatedRecords() == 0);
    }
    {
      typedef vector<uint8_t> Vec;
      typedef map<uint8_t, Vec *> TestData;
      TestData testData;
      
      
      for(uint32_t i = 200; i < 700; i += 100) {
	Vec *v = new Vec(i);
	generate(v->begin(), v->end(), Rand);

	testData[static_cast<uint8_t>(i/10)] = v;
      }


      HeapFile file(tmpFileName);

      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	Vec key(1, itr->first);
	TEST_ASSERT(utc, file.writeBlob(key, *(itr->second)));
	TEST_ASSERT(utc, file.hasBlob(key));
      }

      // lets' now delete block w/ 200 and 400 bytes.

      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 20)));
      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 40)));
      TEST_ASSERT(utc, file.getIndex().freeRecords().size() == 2);
      
      // Now when we allocate for a block of size 256, I fully expect
      // the block w/ 400 bytes to remain free.  Minimum size block is 256
      
      TEST_ASSERT(utc, file.writeBlob(Vec(1, 21), *(testData[20])));
      TEST_ASSERT(utc, file.getIndex().freeRecords().begin()->second->size() >= testData[40]->size());
      const uint64_t fileSize = file.size();

      // now let's use the same key, 21, and allocate a bigger piece of data...like 300 bytes;
      TEST_ASSERT(utc, file.writeBlob(Vec(1, 21), *(testData[30])));
      TEST_ASSERT(utc, file.getIndex().freeRecords().size() == 1);
      TEST_ASSERT(utc, file.getIndex().freeRecords().begin()->second->size() >= testData[20]->size());
      TEST_ASSERT(utc, file.getIndex().freeRecords().begin()->second->size() < testData[30]->size());
      
      TEST_ASSERT(utc, file.size() == fileSize); // same metadata, should be the same size

      TEST_ASSERT(utc, file.hasBlob(Vec(1, 21)));
      Vec dataOut;
      TEST_ASSERT(utc, file.getBlob(Vec(1, 21), dataOut));
      TEST_ASSERT(utc, dataOut == *(testData[30]));
      TEST_ASSERT(utc, dataOut != *(testData[20]));

      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 21)));
      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 40)));
      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 50)));
      TEST_ASSERT(utc, file.eraseBlob(Vec(1, 60)));
      TEST_ASSERT(utc, file.eraseBlob(Vec(1,20))); // delete it again should return true

      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	delete itr->second;
      }
    }
    unlink(tmpFileName.c_str());
  }

  void testHeapFileSize(UnitTestControl &utc)
  {
    const string tmpFileName = tmpnam(NULL);
    
    typedef vector<uint8_t> Vec;
    typedef map<uint8_t, Vec *> TestData;
    TestData testData;
      
      
    for(uint32_t i = 200; i < 700; i += 100) {
      Vec *v = new Vec(i);
      generate(v->begin(), v->end(), Rand);

      testData[static_cast<uint8_t>(i/10)] = v;
    }

    HeapFile file(tmpFileName);

    for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	itr != itrEnd; ++itr) {
      Vec key(1, itr->first);
      TEST_ASSERT(utc, file.writeBlob(key, *(itr->second)));
      TEST_ASSERT(utc, file.hasBlob(key));
    }
    
    uint64_t oldSize = file.size();
    file.setMaxSize(file.size()/2);
    TEST_ASSERT(utc, file.size() <= oldSize/2);

    file.setMaxSize(200);
    TEST_ASSERT(utc, file.size() == 0);

    file.setMaxSize(11);
    TEST_ASSERT(utc, file.size() == 0);

    file.setMaxSize(2*Record::MIN_SIZE);
    TEST_ASSERT(utc, file.writeBlob(Vec(1, 20), *testData[20]));
    TEST_ASSERT(utc, not file.writeBlob(Vec(1, 21), *testData[20]));

    TEST_ASSERT(utc, file.size() <= 512);

    file.setMaxSize(556);
    TEST_ASSERT(utc, file.writeBlob(Vec(1, 21), *testData[20]));
    TEST_ASSERT(utc, file.hasBlob(Vec(1, 21)));
    TEST_ASSERT(utc, file.hasBlob(Vec(1, 20)));

    file.setMaxSize(555);
    TEST_ASSERT(utc, not file.hasBlob(Vec(1,21)));
    TEST_ASSERT(utc, file.hasBlob(Vec(1,20)));

    for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	itr != itrEnd; ++itr) {
      delete itr->second;
    }
    unlink(tmpFileName.c_str());
  }

  void testHeapFileEncryption(UnitTestControl &utc)
  {
    vector<uint8_t> encryptionKey(32);
    std::generate(encryptionKey.begin(), encryptionKey.end(), Rand);

    const string tmpFileName = tmpnam(NULL);

    typedef vector<uint8_t> Vec;
    typedef map<uint8_t, Vec *> TestData;
    TestData testData;
    
    for(uint32_t i = 200; i < 700; i += 100) {
      Vec *v = new Vec(i);
      generate(v->begin(), v->end(), Rand);
      
      testData[static_cast<uint8_t>(i/10)] = v;
    }
    
    
    { // try this with a real encryption key
      HeapFile file(tmpFileName, encryptionKey);
      
      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	Vec key(1, itr->first);
	TEST_ASSERT(utc, file.writeBlob(key, *(itr->second)));
	TEST_ASSERT(utc, file.hasBlob(key));
      }
     
    }

    { // make sure we can read an ecrypted file after committing it to disk
      HeapFile file(tmpFileName, encryptionKey);
      
      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	Vec key(1, itr->first);
	TEST_ASSERT(utc, file.hasBlob(key));
	Vec dataOut;
	TEST_ASSERT(utc, file.getBlob(key, dataOut));
	TEST_ASSERT(utc, dataOut == *(itr->second));
      }
    }

    { // make sure we can't read an encrypted file w/ a different key
      vector<uint8_t> keyMod(encryptionKey);
      keyMod[0]++;
			     
      HeapFile file(tmpFileName, keyMod);

      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	Vec key(1, itr->first);
	TEST_ASSERT(utc, not file.hasBlob(key));
	Vec dataOut;
	TEST_ASSERT(utc, not file.getBlob(key, dataOut));
      }
    }

    {
      HeapFile file(tmpFileName, encryptionKey);
      
      for(TestData::iterator itr = testData.begin(), itrEnd = testData.end();
	  itr != itrEnd; ++itr) {
	Vec key(1, itr->first);
	TEST_ASSERT(utc, file.hasBlob(key));
	Vec dataOut;
	TEST_ASSERT(utc, file.getBlob(key, dataOut));
	TEST_ASSERT(utc, dataOut == *(itr->second));
      }
    }

    unlink(tmpFileName.c_str());
  }


} // end namespace <anonymous>

REGISTER_TEST(testHeapFileInit, &::testInit)
REGISTER_TEST(testHeapFileReadWriteOperations, &::testHeapFileReadWriteOps)
REGISTER_TEST(testHeapFileDeletes, &::testHeapFileDeletes)
REGISTER_TEST(testHeapFileSize, &::testHeapFileSize)
REGISTER_TEST(testHeapFileEncryption, &::testHeapFileEncryption)
