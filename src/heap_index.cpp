#include <heap_index.h>
#include <byte_order.h>
#include <ostream>
#include <stdexcept>

using namespace std;
using namespace EndianUtils;

namespace FileUtils {
  namespace StructuredFiles {

    const std::size_t Record::SERIALIZED_SIZE = sizeof(uint64_t) + 2*sizeof(uint32_t);
    const uint32_t Record::MIN_SIZE = 256;

    namespace { // <anonymous>

      RecordMap::value_type toFreeKey(Record *p)
      {
	return make_pair(p->size(), p);
      }
      
      RecordMap::value_type toAllocKey(Record *p)
      {
	return make_pair(p->key(), p);
      }

      bool recordPtrCmp(const Record *lhs, const Record *rhs)
      {
	return lhs->offset() < rhs->offset();
      }

      bool removeIfFree(RecordMap &map, Record *r)
      {
	typedef RecordMap::iterator Itr;
	pair<Itr, Itr> range = map.equal_range(r->size());

	for(; range.first != range.second; ++range.first) {
	  if (range.first->second != r)
	    continue;
	  map.erase(range.first);
	  return true;
	}
	return false;
      }

    } // end namespace <anonymous>

    Record::Record()
      : m_offset(0), m_key(0), m_size(0)
    {}
  
    Record::Record(uint64_t off, uint32_t key, uint32_t size, bool toMinSize)
      : m_offset(off), m_key(key), m_size(std::max(size, toMinSize ? MIN_SIZE: 0))
    {}

    Record::Record(const char *&p)
      : m_offset(0), m_key(0), m_size(0)
    {
      deserialize(p);
    }

    Record::Record(const Record &lhs, const Record &rhs)
      : m_offset(lhs.m_offset + lhs.m_size), m_key(0), 
	m_size(rhs.m_offset - m_offset)
    {
      if( lhs.m_offset + lhs.m_size >=  rhs.m_offset ) {
	throw runtime_error("Attempt to construct empty Record failed");
      }
    }

    uint32_t Record::serialize(char *&p) const 
    {
      return writeH2N(p, m_offset)
	+ writeH2N(p, m_key)
	+ writeH2N(p, m_size);
    }

    uint32_t Record::deserialize(const char *&s)
    {
      return readN2H(s, m_offset)
	+ readN2H(s, m_key)
	+ readN2H(s, m_size);
    }
  
    bool Record::sharesRightBoundaryWith(const Record &rhs) const
    {
      return m_offset + m_size == rhs.m_offset;
    }

    void Record::coalesce(const Record &r)
    {
      assert(sharesRightBoundaryWith(r) or r.sharesRightBoundaryWith(*this));

      m_offset = std::min(m_offset, r.offset());
      m_size += r.size();
    }
    
    std::auto_ptr<Record> Record::splitOffLeft(const uint32_t size)
    {
      assert(m_size > size);

      // Take _size_ bytes from the left side of this record into a new
      // record and return it.

      std::auto_ptr<Record> left(new Record);
      left->m_offset = this->m_offset;
      left->m_size = size;

      this->m_offset += size;
      this->m_size -= size;

      return left;
    }

    bool operator==(const Record &lhs, const Record &rhs)
    {
      return 
	lhs.size()   == rhs.size() &&
	lhs.key()    == rhs.key()  && 
	lhs.offset() == rhs.offset();
    }

    bool operator!=(const Record &lhs, const Record &rhs)
    {
      return not(lhs == rhs);
    }

    ostream &operator<<(ostream &strm, const Record &e)
    {
      strm << "Record["
	   << "offset=" << e.offset()
	   << ", size=" << e.size()
	   << ", key=" << e.key()
	   << "]";
    
      return strm;
    }

    HeapIndex::~HeapIndex()
    {
      clear();
    }

    void HeapIndex::clear()
    {
      m_alloc.clear();
      m_free.clear();

      typedef RecordList::iterator Itr;
      for(Itr p = m_list.begin(), q = m_list.end(); p != q; ++p)
	delete *p;
    }

    // I'm keeping this pointer
    void HeapIndex::addAllocatedBlock(std::auto_ptr<Record> p)
    {
      if (not m_list.empty()) {
	const Record &last = *m_list.back();
	if (not last.sharesRightBoundaryWith(*p)) {
	  m_list.push_back(new Record(last, *p)); // empty record
	  m_free.insert(toFreeKey(m_list.back()));
	}
      }

      m_list.push_back(p.release());
      m_alloc.insert(toAllocKey(m_list.back()));
    }

    // Worst-case scenario here is all free records
    // are of the same size!  So this degrades to O(n)
    // run-time cost from O(log n) + O(_num_collisions_)
    // where n is the number of free records and
    // _num_collisions_ is the number of Records with 
    // the same size.
    bool HeapIndex::isFree(const Record &p) const
    {
      typedef RecordMap::const_iterator Itr;
      pair<Itr, Itr> range = m_free.equal_range(p.size());
      for(; range.first != range.second; ++range.first) {
	const Record *q = range.first->second;
	assert(NULL != q);
	if (*q == p)
	  return true;
      }

      return false;
    }

    bool HeapIndex::isLast(const Record &r) const
    {
      if (m_list.empty())
	return false;

      return r == *(m_list.back());
    }

    bool HeapIndex::coalesce(Record *r)
    {
      typedef RecordList::iterator Itr;
      pair<Itr, Itr> range = std::equal_range(m_list.begin(), 
					      m_list.end(),
					      r,
					      recordPtrCmp);
      for(; range.first != range.second; ++range.first)
      {
	if (**(range.first) != *r)
	  continue;
	assert((*range.first) == r);
	  
	// coalesce left
	if (m_list.begin() != range.first) { // not the front
	  Itr leftItr(range.first); 
	  --leftItr;
	  Record *leftRec = *leftItr;
	  assert(NULL != leftRec);
	  if (removeIfFree(m_free, leftRec)) {
	    std::auto_ptr<Record> release(leftRec);
	    m_list.erase(leftItr);
	    r->coalesce(*release);
	  }
	}

	// coalesce right
	if (m_list.back() != *range.first) { // not the back
	  Itr rightItr(range.first);
	  ++rightItr;
	  Record *rightRec = *rightItr;
	  assert(NULL != rightRec);
	  if (removeIfFree(m_free, rightRec)) {
	    std::auto_ptr<Record> release(rightRec);
	    m_list.erase(rightItr);
	    r->coalesce(*release);
	  }
	}
	return true;
      }
      return false;
    }
   
    bool HeapIndex::deallocate(const Record &rec)
    {
      typedef RecordMap::iterator Itr;
      pair<Itr, Itr> range = m_alloc.equal_range(rec.key());
      
      for(; range.first != range.second; ++range.first) {
	Record *r = range.first->second;
	assert(NULL != r);
	if (*r != rec)
	  continue;

	m_alloc.erase(range.first);
	  
	coalesce(r);

	if (*m_list.back() == *r) {
	  std::auto_ptr<Record> release(m_list.back());
	  m_list.pop_back();
	}else {
	  m_free.insert(toFreeKey(r));
	}
	return true;
      }
						 
      return false;
    }

    // On the call to allocate(), we search for an empty record.
    // If the size of the record found is within some delta
    // then we allocate it whole.  Otherwise, we split
    // it up.  The resulting empty record should be 
    // at least as big as the minimum block size.
    // What's a good minimum here?  Well, we need
    // at least sizeof(uint8_t) + 2*sizeof(uint32_t) number of bytes
    // just to store metadata for both the ObjectId and the object.
    // Morover, in the metadata section of HeapIndex there are an additional
    // 16 bytes dedicated to metadata about each allocated record.
    // Since this is meant to store small- to medium-sized
    // objects, they should be cheaper to store on disk than, let's say,
    // to fetch from the internet.  So the objects themselves should
    // be at least in the hundreds of bytes.  So as to keep the
    // metadata-to-payload ratio low, I've set the minimum size to 256
    // bytes which puts said ratio at about .13 in the worst case.
    Record *HeapIndex::allocate(uint32_t size, uint32_t key)
    {

      size = std::max(size, Record::MIN_SIZE);

      RecordMap::iterator freeItr = m_free.lower_bound(size);
      
      if (m_free.end() == freeItr)
	return NULL;
      
      Record *r = freeItr->second;
      m_free.erase(freeItr); // remove it from the free blocks container

      if (size > r->size() - Record::MIN_SIZE) {
	r->setKey(key);
	m_alloc.insert(toAllocKey(r));
	return r; // allocate the whole block
      }
      // else, split'er up

      auto_ptr<Record> left = r->splitOffLeft(size);
      m_free.insert(toFreeKey(r)); // add _r_ back in w/ a new size


      // now find _r_ in the block list and insert _left_ in front of it.
      typedef RecordList::iterator Itr;
      Itr itr = std::lower_bound(m_list.begin(), m_list.end(), 
				 r, recordPtrCmp);
      assert(itr != m_list.end());
      assert(*itr == r);

      m_list.insert(itr, left.get());
      left->setKey(key);
      m_alloc.insert(toAllocKey(left.get()));
      return left.release();
    }

    uint32_t HeapIndex::size() const 
    {
      return sizeof(uint32_t) + Record::SERIALIZED_SIZE * numAllocatedRecords();
    }
    
    
  } // end namespace StructuredFiles
} // end namespace FileUtils
