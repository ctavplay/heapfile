#include <heap_blob.h>
#include <byte_order.h>
#include <cstdlib>
#include <heap_index.h>
#include <iostream>
#include <unit_test.h>

using namespace std;
using namespace EndianUtils;
using namespace FileUtils;
using namespace FileUtils::StructuredFiles;

namespace { // <anonymous> 

  struct Reader : public BlobReader
  {
    Reader(std::vector<uint8_t> &dataOut)
      : m_dataOut(dataOut)
    {}

    virtual void readBlob(uint32_t size, const uint8_t *src) const
    {
      m_dataOut.resize(size);
      std::copy(src, src + size, &m_dataOut[0]);
    }
    
    std::vector<uint8_t> &m_dataOut;
  };

  struct Writer : public BlobWriter
  {
    Writer(const std::vector<uint8_t> &data)
      : m_data(data)
    {}

    virtual uint32_t size() const { return m_data.size(); }
    virtual void writeBlob(uint8_t *dst) const {
      std::copy(m_data.begin(), m_data.end(), dst);
    }

    const std::vector<uint8_t> &m_data;
  };

  uint8_t Rand() { return rand()%0x0FF; }
  void fauxBlob(std::vector<uint8_t> &blob,
		const std::vector<uint8_t> &key,
		const std::vector<uint8_t> &data)
  {
    //
    //  Blob format is as follows:
    //           Byte          - Description
    // ------------------------------------------------------------------
    //            0            - key.size() of type uint8_t
    //            1            - bytes of key (0 - 255 bytes in length)
    //   above+key.size()      - hash of the data of type uint32_t
    //  above+sizeof(uint32_t) - length of data of type uint32_t
    //  above+sizeof(uint32_t) - data (0 - ~uint32_t(0) bytes in length)
    //

    blob = std::vector<uint8_t>( sizeof(uint8_t) + key.size() + 
				 2*sizeof(uint32_t) + data.size());
    blob[0] = key.size();
    std::copy(key.begin(), key.end(), ++(blob.begin()));
    
    uint8_t *p = &blob[sizeof(uint8_t) + key.size()];

    writeH2N(p, hash(data)); // advances p
    writeH2N(p, static_cast<uint32_t>(data.size())); // advances p

    std::copy(data.begin(), data.end(), p);    
  }

  void testNilBlob(UnitTestControl &utc)
  {
    Blob b;
    TEST_ASSERT(utc, not b.hasId(vector<uint8_t>()));
    TEST_ASSERT(utc, b.isNil());

    vector<uint8_t> blank;
    TEST_ASSERT(utc, not b.getData(Reader(blank)));
  }

  void testBlobReads(UnitTestControl &utc)
  {
    const size_t COLS = 2;
    size_t arr[][COLS] = 
      { 
	{32, 1024*100}, 
	{0, 10}, 
	{10, 0},
	{255, 0},
	{255, 0x0fffff}
      };

    for( size_t row = 0; row < sizeof(arr)/sizeof(arr[0][0])/COLS; ++row) {

      std::vector<uint8_t> key(arr[row][0]);
      std::generate(key.begin(), key.end(), Rand);
    
      std::vector<uint8_t> data(arr[row][1]);
      std::generate(data.begin(), data.end(), Rand);

      std::vector<uint8_t> blob;
      fauxBlob(blob, key, data);

      Blob b(&blob[0], Record(10, 0xdeadbeef, blob.size()));

      TEST_ASSERT(utc, not b.isNil());
      TEST_ASSERT(utc, b.hasId(key));
      key.push_back(0);
      TEST_ASSERT(utc, not b.hasId(key));
      key.pop_back();
      blob[0]++;
      TEST_ASSERT(utc, not b.hasId(key));
      blob[0]--;

      std::vector<uint8_t> dataOut;
      TEST_ASSERT(utc, b.getData(Reader(dataOut)));
      TEST_ASSERT(utc, data == dataOut);

      blob[blob.size() - 1] += 1; // try to make this not hash

      TEST_ASSERT(utc, not b.getData(Reader(dataOut)));
      blob.pop_back();
      TEST_ASSERT(utc, not b.getData(Reader(dataOut)));
      
      // fudge w/ the size
      blob[1 + key.size()+sizeof(uint32_t)] += 1;
      
      TEST_ASSERT(utc, not b.getData(Reader(dataOut)));
      
    }
  }


  void testBlobWrites(UnitTestControl &utc)
  {
    const size_t COLS = 2;
    size_t arr[][COLS] = 
      { 
	{32, 1024*100}, 
	{0, 10}, 
	{10, 0},
	{255, 0},
	{255, 0x0ff}
      };

    for( size_t row = 0; row < sizeof(arr)/sizeof(arr[0][0])/COLS; ++row) {
      vector<uint8_t> id(arr[row][0]);
      generate(id.begin(), id.end(), Rand);

      vector<uint8_t> data(arr[row][1]);
      generate(data.begin(), data.end(), Rand);

      vector<uint8_t> blob(id.size() + data.size() + 
			   sizeof(uint8_t) + 2*sizeof(uint32_t));

      {
	Blob b(&blob[0], Record(8, 0xdeadbeef, blob.size()));

	TEST_ASSERT(utc, b.writeData(id, Writer(data)));
	TEST_ASSERT(utc, b.hasId(id));
      
	vector<uint8_t> dataOut;
	TEST_ASSERT(utc, b.getData(Reader(dataOut)));
	TEST_ASSERT(utc, data == dataOut);
      }

      {
	Blob b(&blob[0], Record(8, 0xdeadbeef, 256+blob.size()));

	id.resize(256);
	TEST_ASSERT(utc, not b.writeData(id, Writer(data)));
	
      }
    }
  }

} // end namespace <anonymous>

REGISTER_TEST(testEmptyBlob, &::testNilBlob)
REGISTER_TEST(testBlobReads, &::testBlobReads)
REGISTER_TEST(testBlobWrites, &::testBlobWrites)
