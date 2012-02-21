#include <heap_file.h>
#include <algorithm>
#include <byte_order.h>
#include <cassert>
#include <heap_blob.h>

using namespace EndianUtils;
using namespace std;

namespace FileUtils {
  namespace StructuredFiles {
    namespace { // <anonymous>
      struct HeapIndexLocation : std::pair<uint32_t, const char *>
      {
	typedef std::pair<uint32_t, const char *> INHERITED;

	HeapIndexLocation(uint32_t numRecords, const char *ptr)
	  : INHERITED(numRecords, ptr)
	{}

	uint32_t numRecs() const  { return INHERITED::first;  }
	const char *&recsRefPtr() { return INHERITED::second; }
      };
      
      // Will return the number of allocated HeapFile Records and a
      // pointer into the file where the seriliazed Records live.
      HeapIndexLocation findHeapIndex(const MmapFile &file)
      {
	// first sizeof(uint64_t) bytes in the heap file should have
	// the location of the HeapIndex.
	uint64_t heapIndexOffset = n2h(file.readOrThrow<uint64_t>(0));

	// first sizeof(uint32_t) at heapIndexOffset contains the number
	// of allocated Records.
	uint32_t numRecords = n2h(file.readOrThrow<uint32_t>(heapIndexOffset));
	uint64_t rawSize = numRecords * Record::SERIALIZED_SIZE;
	const char *ptr = file.getReadPtr<char>(heapIndexOffset +
						sizeof(numRecords),
						rawSize);

	return HeapIndexLocation(numRecords, ptr);
      }
      
      // Will write the location of the HeapIndex at the head of the file
      // then return a ptr where serialization of HeapIndex Records can
      // be written to.
      char *prepForCommit(const HeapIndex &index, MmapFile &file)
      {
	assert(not index.allRecords().empty());

	const Record *last = index.allRecords().back();
	assert(NULL != last);
	assert(not index.isFree(*last));

	uint64_t indexOffset = last->offset() + last->size();
	char *ptr = file.getWritePtr<char>(0);
	writeH2N(ptr, indexOffset); // advances ptr;

	const uint32_t numRecords = index.numAllocatedRecords();

	ptr = file.getWritePtr<char>(indexOffset, index.size());
	writeH2N(ptr, numRecords); // advances ptr
	return ptr; // serialization can begin here
      }

      Blob findBlob(const vector<uint8_t> &id,
		    const HeapIndex &index,
		    const MmapFile &file)
      {
	typedef RecordMap::const_iterator Itr;
	pair<Itr, Itr> range = index.allocRecords().equal_range(hash(id));

	for(; range.first != range.second; ++range.first) {
	  const Record *r = range.first->second;
	  assert( NULL != r);
	  Blob b(*r, file);
	  if (b.hasId(id)) {
	    return b;
	  }
	}
	return Blob();	
      }

      void release(const Record &r, HeapIndex &index, MmapFile &file)
      {
	bool isLast = index.isLast(r);
	uint64_t offset = r.offset();

	index.deallocate(r);

	if (isLast)
	  file.trim(offset + index.size());
      }

      bool eraseBlobEncryptedId(const std::vector<uint8_t> &id,
				HeapIndex &index, MmapFile &file)
      {
	const Blob &b = findBlob(id, index, file);
	
	if (b.isNil())
	  return true;

	release(b.record(), index, file);
	
	return true;
      }

    } // end namespace <anonymous>

    template<>
    HeapFileT<>::HeapFileT(const string &path,
			   const std::vector<uint8_t> &key)
      : m_index(), m_file(path), m_key(key), m_maxSize(-1)
    {
      if (0 == m_file.size())
	return;
    
      try {

	HeapIndexLocation loc = findHeapIndex(m_file);
	for(uint32_t i = 0; i < loc.numRecs(); ++i) {
	  std::auto_ptr<Record> p(new Record(loc.recsRefPtr()));
	  m_index.addAllocatedBlock(p);
	}

      }catch(const std::exception &e)
      {
	m_index.clear();
	m_file.clear();
      }
    }

    template<>
    HeapFileT<>::~HeapFileT()
    {
      try {
	if (0 == m_index.numAllocatedRecords()) {
	  m_file.clear();
	  return;
	}
	char *ptr = prepForCommit(m_index, m_file);

	typedef RecordList::const_iterator ConstItr;
	const RecordList &list = m_index.allRecords();

	uint32_t numSerialized = 0;
	for(ConstItr itr = list.begin(), itrEnd = list.end(); 
	    itr != itrEnd; ++itr) {
	  const Record *e = *itr;
	  assert( NULL != e);
	  if (m_index.isFree(*e))
	    continue;
	  e->serialize(ptr); // serialize advances ptr;
	  ++numSerialized;
	}

	assert(numSerialized == m_index.numAllocatedRecords());	
      }
      catch(const std::exception &e) // don't let exceptions escape destructors.
      {
	assert(!"Caught an exception in ~HeapFileT()");
      } 
    }

    template<>
    bool HeapFileT<>::hasBlob(const std::vector<uint8_t> &clearId) const
    {
      std::vector<uint8_t> id(clearId);
      m_key.encrypt(id, id);
      const Blob &b = findBlob(id, m_index, m_file);
      return not b.isNil();
    }

    template<class EP>
    bool HeapFileT<EP>::getBlob(const std::vector<uint8_t> &clearId,
				std::vector<uint8_t> &data) const
    {
      std::vector<uint8_t> id(clearId);
      m_key.encrypt(id, id);
      const Blob &b = findBlob(id, m_index, m_file);

      if (b.isNil())
	return false;

      struct Reader : public BlobReader
      {
	Reader(std::vector<uint8_t> &dataOut,
	       const EP &key)
	  : m_dataOut(dataOut), m_key(key)
	{}

	virtual void readBlob(uint32_t size, const uint8_t *src) const
	{
	  m_dataOut.resize(size);
	  m_key.decrypt(src, &m_dataOut[0], size);
	}

	std::vector<uint8_t> &m_dataOut;
	const EP &m_key;
      };

      return b.getData(Reader(data, m_key));
    }

    template<>
    bool HeapFileT<>::eraseBlob(const std::vector<uint8_t> &clearId)
    {
      std::vector<uint8_t> id(clearId);
      m_key.encrypt(id, id);
      return eraseBlobEncryptedId(id, m_index, m_file);
    }

    template<class EP>
    bool HeapFileT<EP>::writeBlob(const std::vector<uint8_t> &clearId,
				  const std::vector<uint8_t> &blob)
    {
      std::vector<uint8_t> id(clearId);
      m_key.encrypt(id, id);
      if (not eraseBlobEncryptedId(id, m_index, m_file))
	return false;

      uint32_t blobSize = Blob::blobSize(id.size(), blob.size());
      uint32_t hashCode = hash(id);
      
      Record *r = m_index.allocate(blobSize, hashCode);

      if (NULL == r) {
	// grab more from the disk
	uint64_t offset = sizeof(uint64_t);
	if (not m_index.allRecords().empty()) {
	  Record *last = m_index.allRecords().back();
	  offset = last->offset() + last->size();
	}
	auto_ptr<Record> allocated(new Record(offset, hashCode, blobSize, true));
	r = allocated.get();
	m_index.addAllocatedBlock(allocated);
	uint64_t proposedSize = r->offset() + r->size() + m_index.size();
	if (proposedSize > m_maxSize) {
	  m_index.deallocate(*r);
	  return false;
	}
	m_file.trim(proposedSize);
      }

      uint8_t *writePtr = m_file.getWritePtr<uint8_t>(r->offset(), 
						      r->size());
      Blob b(writePtr, *r);
      
      struct Writer : public BlobWriter
      {
	Writer(const std::vector<uint8_t>& data,
	       EP &key)
	  : m_data(data), m_key(key)
	{}
	
	virtual uint32_t size() const { return m_data.size(); }
	virtual void writeBlob(uint8_t *dest) const
	{
	  m_key.encrypt(&m_data[0], dest, m_data.size());
	}
	
	const std::vector<uint8_t> &m_data;
	const EP &m_key;
      };

      if (b.writeData(id, Writer(blob, m_key)))
	return true;

      // well, if we made it here, something went horribly wrong.
      // so let's clean up.

      release(*r, m_index, m_file);
      return false;
    }
    
    template<>
    void HeapFileT<>::clear()
    {
      m_index.clear();
      m_file.clear();
      m_maxSize = -1;
    }

    // To guarantee that the HeapFile will shrink in size
    // we have to remove entries from the end of the file, which could
    // end up erasing recently added entries....depending on overall
    // available space and how often we remove an entry from the heap file.
    template<>
    void HeapFileT<>::setMaxSize(uint64_t maxSize)
    {
      m_maxSize = maxSize;

      if (static_cast<uint64_t>(m_file.size()) < m_maxSize)
	return;

      if (0 == m_index.numAllocatedRecords()) {
	clear();
	return;
      }
      
      const Record *rec = m_index.allRecords().back();
      uint64_t currentSize = m_file.size();

      // remove blobs from the end; it's a sure-fire way
      // to shrink the heap file
      do {
	m_index.deallocate(*rec);

	if (0 == m_index.numAllocatedRecords()) {
	  clear();
	  return;
	}

	rec = m_index.allRecords().back();
	currentSize = rec->offset() + rec->size() + m_index.size();
      }while(currentSize > maxSize);
      
      m_file.trim(currentSize); // the real deallcation happens here
    }

    template class HeapFileT<DefaultEncryptionPolicy>;
  } // end namespace StructuredFiles
} // end namespace FileUtils

