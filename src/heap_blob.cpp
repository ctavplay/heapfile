#include <heap_blob.h>
#include <algorithm>
#include <byte_order.h>
#include <heap_index.h>
#include <limits>
#include <mmap_file.h>

using namespace EndianUtils;
using namespace std;

namespace FileUtils {
  namespace StructuredFiles {
    namespace { // <anonymous> 
      typedef uint32_t HashType;
      typedef uint32_t BlobSizeType;
      typedef uint8_t  IdSizeType;

      std::size_t overhead() {
	return
	  sizeof(HashType) +
	  sizeof(BlobSizeType) +
	  sizeof(IdSizeType);
      }

      std::size_t diskSize(const vector<uint8_t> &id,
			   const BlobWriter &wr)
      {
	return overhead() + id.size() + wr.size();
      }
    } // end namespace <anonymous>

    
    Blob::Blob() 
      : m_rec(Record()), m_ptr(NULL)
    {}

    Blob::Blob(uint8_t *p, const Record &r)
      : m_rec(r), m_ptr(p)
    {}

    Blob::Blob(const Record &r, const MmapFile &file)
      : m_rec(r),
	m_ptr(file.getReadPtr<uint8_t>(r.offset(), r.size()))
    {}
  
    bool Blob::hasId(const std::vector<uint8_t> &id) const
    {
      if (NULL == m_ptr)
	return false;

      const uint8_t *p = m_ptr;
      IdSizeType idSize;
      readN2H(p, idSize); // advances p;

      if (id.size() != idSize or idSize > m_rec.size())
	return false;

      for(uint8_t i = 0; i < idSize; ++i) {
	if (id[i] != p[i])
	  return false;
      }
      return true;
    }

    uint32_t Blob::blobSize(size_t keySize, size_t dataSize)
    {
      return overhead() + keySize + dataSize; 
    }

    bool Blob::writeData(const vector<uint8_t> &id,
			 const BlobWriter &wr)
    {
      assert(diskSize(id, wr) <= m_rec.size());

      if (id.size() > numeric_limits<IdSizeType>::max() or 
	  wr.size() > numeric_limits<BlobSizeType>::max())
	return false;

      uint8_t *p = const_cast<uint8_t *>(m_ptr); // a teeny cop-out
      writeH2N(p, IdSizeType(id.size())); // advances p
      p = std::copy(id.begin(), id.end(), p); // advances p

      uint8_t *hashPtr = p;
      p += sizeof(uint32_t)/sizeof(uint8_t); // advance p...write the hash last

      writeH2N(p, wr.size()); // advances p

      wr.writeBlob(p); // does not advance p

      writeH2N(hashPtr, hash(p, wr.size()));

      return true;
    }

    bool Blob::getData(const BlobReader &br) const
    {
      if (isNil())
	return false;

      const uint32_t recSize = m_rec.size();
      const uint8_t *p = m_ptr;

      IdSizeType keySize;
      readN2H(p, keySize); // advances p;
      
      if (keySize + overhead() > recSize)
	return false;

      p += keySize; // move it passed the key

      HashType storedHashCode;
      readN2H(p, storedHashCode); // advances p

      BlobSizeType dataSize;
      readN2H(p, dataSize); // advances p

      if (dataSize > recSize - overhead() - keySize)
	return false; // possible corruption

      uint32_t hashCode = hash(p, dataSize);

      if (hashCode != storedHashCode)
	return false; // if the hashes don't match, could be corrupt

      br.readBlob(dataSize, p);
      return true;
    }

    // djb2 hash fn w/ the XOR substitution
    uint32_t hash(const uint8_t *p, size_t size)
    {
      uint32_t hash = 5381;
      
      for(size_t i = 0; i < size; ++i)
	hash = ((hash << 5) + hash) ^ p[i];
      
      return hash;
    }
    
    uint32_t hash(const std::vector<uint8_t> &v)
    {
      return hash(&v[0], v.size());
    }

  } // end namespace StructuredFiles
} // end namespace FileUtils
