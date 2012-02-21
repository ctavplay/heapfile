#include <heap_file.h>
#include <unit_test.h>

using namespace std;
using namespace FileUtils;
using namespace FileUtils::StructuredFiles;

namespace { // <anonymous>
  
  void testHeapFileRecord(UnitTestControl &utc)
  {
    {
      Record hfe;
    
      TEST_ASSERT(utc, 0 == hfe.size());
      TEST_ASSERT(utc, 0 == hfe.key());
      TEST_ASSERT(utc, 0L == hfe.offset());
    }

    Record hfe(1L<<40, 0xdeadbeef, (2<<10)*100);


    TEST_ASSERT(utc, sizeof(uint64_t) == sizeof(0L));
    TEST_ASSERT(utc, 100*(2<<10) == hfe.size());
    TEST_ASSERT(utc, 0xdeadbeef == hfe.key());
    TEST_ASSERT(utc, 1L<<40 == hfe.offset());

    // test the copy c'tor
    Record hfe2(hfe);
    TEST_ASSERT(utc, hfe.size() == hfe2.size());
    TEST_ASSERT(utc, hfe.key()    == hfe2.key());
    TEST_ASSERT(utc, hfe.offset()      == hfe2.offset());

    TEST_ASSERT(utc, hfe == hfe2);
    TEST_ASSERT(utc, not(hfe != hfe2));
    TEST_ASSERT(utc, hfe != Record());

    
    hfe = hfe2;
    TEST_ASSERT(utc, hfe == hfe2);

    Record foo(10, 0xdeadbeef, 50);
    Record baz(foo.size()+foo.offset(), 0xdeadbeaf, 50);

    TEST_ASSERT(utc, foo.sharesRightBoundaryWith(baz));
    foo.coalesce(baz);
    TEST_ASSERT(utc, foo.offset() == 10);
    TEST_ASSERT(utc, foo.key() == 0xdeadbeef);
    TEST_ASSERT(utc, foo.size() == 100);

    TEST_ASSERT(utc, foo.offset() + foo.size() == baz.offset() + baz.size());

    auto_ptr<Record> split = foo.splitOffLeft(50);
    TEST_ASSERT(utc, foo.size() == baz.size());
    TEST_ASSERT(utc, foo.offset() == baz.offset());
    TEST_ASSERT(utc, split->sharesRightBoundaryWith(foo));
    TEST_ASSERT(utc, split->offset() == 10);
    TEST_ASSERT(utc, split->size() == 50);
    TEST_ASSERT(utc, split->key() == 0);
  }  
  
  void testSerialization(UnitTestControl &utc)
  {

    // is this thing tightly aligned?
    const size_t size = sizeof(uint64_t) + 2*sizeof(uint32_t);

    Record mce(1L<<40, 0xdeadbeef, (2<<10)*100);

    char buffer[2*size] = {0};
    char *p = buffer;
    TEST_ASSERT(utc, size == mce.serialize(p));
    TEST_ASSERT(utc, buffer + size == p );

    Record mce2;
    const char *q = buffer;
    TEST_ASSERT(utc, size == mce2.deserialize(q));
    TEST_ASSERT(utc, buffer + size == p );

    TEST_ASSERT(utc, mce == mce2);
    q = buffer;
    TEST_ASSERT(utc, mce2 == Record(q));
  }

  void testHeapIndexOps(UnitTestControl &utc)
  {
    HeapIndex heap;

    // a HeapIndex doesn't know how to get more memory;
    // it only manages what we tell it about
    TEST_ASSERT(utc, NULL == heap.allocate(0, 0));
    TEST_ASSERT(utc, NULL == heap.allocate(10, 0));

    heap.addAllocatedBlock(auto_ptr<Record>(new Record(8, 0x0, 256)));
    heap.addAllocatedBlock(auto_ptr<Record>(new Record(8+256, 0x1, 512)));

    TEST_ASSERT(utc, heap.numAllocatedRecords() == 2);
    TEST_ASSERT(utc, heap.allRecords().size() == 2);
    TEST_ASSERT(utc, heap.freeRecords().size() == 0);
    TEST_ASSERT(utc, heap.allocRecords().find(0x0)->second->offset() == 8);
    TEST_ASSERT(utc, heap.allocRecords().find(0x1)->second->offset() == 8+256);

    heap.addAllocatedBlock(auto_ptr<Record>(new Record(2000, 0x2, 256)));
    
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 3);
    TEST_ASSERT(utc, heap.allRecords().size() == 4);
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.allocRecords().find(0x2)->second->offset() == 2000);


    uint32_t freeBlockSize = 2000 - 8 - 256 - 512;
    uint32_t freeBlockOffset = 8+256+512;

    const Record &empty = *(heap.freeRecords().begin()->second);
    TEST_ASSERT(utc, empty.offset() == freeBlockOffset);
    TEST_ASSERT(utc, empty.size() == freeBlockSize);
    TEST_ASSERT(utc, heap.freeRecords().find(freeBlockSize)->second->offset() == freeBlockOffset);

    // test simple allocation, no splitting
    const Record *r = heap.allocate(empty.size(), 0x3);

    TEST_ASSERT(utc, empty == *r);
    TEST_ASSERT(utc, heap.allocRecords().find(0x3)->second == r);
    TEST_ASSERT(utc, heap.freeRecords().empty());
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 4);

    // test simple deallocation, no coalescing, no trimming

    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 3);
    TEST_ASSERT(utc, heap.allRecords().size() == 4);

    // test that I'm able to reallocate previously freed block
    r = heap.allocate(empty.size(), 0x3);
    TEST_ASSERT(utc, NULL != r);
    TEST_ASSERT(utc, heap.freeRecords().empty());
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 4);

    // deallocate it again
    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 3);
    TEST_ASSERT(utc, heap.allRecords().size() == 4);
    
    r = *(++(heap.allRecords().begin())); // block at index 8+256, size 512

    // test coalesce right
    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 2);
    TEST_ASSERT(utc, heap.allRecords().size() == 3);
    
    typedef RecordList::const_iterator Itr;
    Itr right = heap.allRecords().begin();
    ++right;
    for(Itr left(heap.allRecords().begin()), end(heap.allRecords().end());
	right != end; ++left, ++right) {
      const Record *p = *left;
      const Record *q = *right;
      TEST_ASSERT(utc, p->sharesRightBoundaryWith(*q));
    }

    // simple allocation
    r = heap.allocate(2000-256-8, 0x1);
    TEST_ASSERT(utc, r != NULL);
    TEST_ASSERT(utc, r->key() == 0x1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 3);
    TEST_ASSERT(utc, heap.allRecords().size() == 3);
    TEST_ASSERT(utc, heap.freeRecords().size() == 0);

    // simple deallocate block at 8 w/ size 256

    r = *(heap.allRecords().begin()); // block at 8, size 256
    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 2);
    TEST_ASSERT(utc, heap.allRecords().size() == 3);

    // test coalesce left!
    r = *(++(heap.allRecords().begin())); // block at 8+256, size 2000-256-8
    
    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 1);
    TEST_ASSERT(utc, heap.allRecords().size() == 2);
    TEST_ASSERT(utc, (**(heap.allRecords().begin())).size() == 2000 - 8);

    // test window limit allocation
    // if we ask for a block size that is less than the smallest
    // available block but still within an acceptable delta...
    // we allocate the whole block.

    r = heap.allocate(2000 - 8 - 255, 0x0); // minimum block size is 256
    TEST_ASSERT(utc, NULL != r);
    TEST_ASSERT(utc, heap.freeRecords().size() == 0);
    TEST_ASSERT(utc, heap.allRecords().size() == 2);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 2);
    TEST_ASSERT(utc, r->size() == 2000 - 8);

    TEST_ASSERT(utc, heap.deallocate(*r));

    r = heap.allocate(2000 - 8 - 256, 0x0); // should split up the free block

    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.allRecords().size() == 3);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 2);
    TEST_ASSERT(utc, heap.allocRecords().find(0)->second->offset() == 8);
    TEST_ASSERT(utc, heap.allocRecords().find(0)->second->size() == 2000 - 8 - 256);
    TEST_ASSERT(utc, heap.freeRecords().find(256)->second->offset() == 2000 - 256);



    TEST_ASSERT(utc, heap.deallocate(*r));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.allRecords().size() == 2);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 1);

    // test scenario where we allocate less than minimum size block
    TEST_ASSERT(utc, NULL != heap.allocate(2, 0x0));
    TEST_ASSERT(utc, NULL != (r = heap.allocate(2, 0x1)));
    TEST_ASSERT(utc, NULL != heap.allocate(2, 0x3));

    TEST_ASSERT(utc, heap.numAllocatedRecords() == 4);
    Itr itr = heap.allRecords().begin();
    TEST_ASSERT(utc, (**itr).offset() == 8);
    TEST_ASSERT(utc, (**itr).size() == 256); 
    ++itr;
    TEST_ASSERT(utc, (**itr).offset() == 8 + 256);
    TEST_ASSERT(utc, (**itr).size() == 256);
    ++itr;
    TEST_ASSERT(utc, (**itr).offset() == 8+256+256);
    TEST_ASSERT(utc, (**itr).size() == 256);
    ++itr; // the free block
    TEST_ASSERT(utc, (**itr).size() == 2000 - 8 - 3*256);
    TEST_ASSERT(utc, (**itr).offset() == 8+3*256);
    TEST_ASSERT(utc, heap.freeRecords().find(2000 - 8 - 3*256)->second == *itr);

    --itr;
    TEST_ASSERT(utc, heap.deallocate(**itr));
    itr = heap.allRecords().begin();
    TEST_ASSERT(utc, heap.deallocate(**itr));

    // test coalescing on both sides!
    ++itr;
    TEST_ASSERT(utc, heap.deallocate(**itr));
    TEST_ASSERT(utc, heap.freeRecords().size() == 1);
    TEST_ASSERT(utc, heap.numAllocatedRecords() == 1);
    TEST_ASSERT(utc, heap.allRecords().size() == 2);
    TEST_ASSERT(utc, heap.freeRecords().find(2000-8)->second->offset() == 8);
    TEST_ASSERT(utc, heap.allocRecords().find(0x2)->second->offset() == 2000);

    
    TEST_ASSERT(utc, NULL == heap.allocate(2000, 0x0));
    
    // the trim test, which must coalesce as well.
    TEST_ASSERT(utc, heap.deallocate(**(heap.allRecords().rbegin())));
    TEST_ASSERT(utc, heap.allRecords().size() == 0);
    TEST_ASSERT(utc, heap.freeRecords().size() == 0);
    TEST_ASSERT(utc, heap.allocRecords().size() == 0);
    
  }
} // end namespace

REGISTER_TEST(testHeapFileRecord, &::testHeapFileRecord)
REGISTER_TEST(testHeapFileRecordSerialization, &::testSerialization)
REGISTER_TEST(testHeapIndexOperations, &::testHeapIndexOps)
