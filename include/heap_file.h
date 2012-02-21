#ifndef _HEAP_FILE_H_
#define _HEAP_FILE_H_ 1

#include <heap_file_fwd.h>
#include <heap_index.h>
#include <mmap_file.h>
#include <simple_encrypt.h>
#include <stdint.h>
#include <uncopyable.h>
#include <vector>

namespace FileUtils {
  namespace StructuredFiles {

    /**
     * This class can be thought of as a hash table serialized
     * to disk.  It supports encryption by policy class.
     *
     * If the wrong encryption key is used, the heap file
     * will behave as if it were empty in every respect except
     * the size() function, and anything queried from the HeapIndex.
     * It is possible to write to this heap
     * file with a different encryption key than initially used
     * and not interfere with previously written objects under a 
     * different key.  In other words, the data that it stores is 
     * encrypted but not any of its metadata.
     *
     * The DefaultEncryptionPolicy is typedef'ed in 
     * heap_file_fwd.h.  We use simple XOR encryption here
     * but can easily be changed at some later date.
     * Providing the empty key is equivalent to using no encryption.
     * This is because (A^0 == A).
     */
    template <class EncryptionPolicy = DefaultEncryptionPolicy>
    class HeapFileT : private Uncopyable {
    public:
      HeapFileT(const std::string &path, 
		const std::vector<uint8_t> &encryptionKey = std::vector<uint8_t>());
      ~HeapFileT();

      const HeapIndex &getIndex() const { return m_index; }
      
      /**
       * HeapFile size on disk in bytes.
       */
      uint64_t size() const { return m_file.size(); }

      /**
       * Existence test.  Will actually read from disk to compare
       * the object id stored to the one passed in.
       */
      bool hasBlob(const std::vector<uint8_t> &id) const;

      /**
       * Returns true if the data appears to not be corrupt
       * and is successfully read.  Returns false on failure.
       * It is not erased from the heap file on failure, however.
       */
      bool getBlob(const std::vector<uint8_t> &id, 
		   std::vector<uint8_t> &blob) const;

      /**
       * Returns true if the object mapped to by the ObjectId
       * was successfully erased or if it never existed in the
       * first place.  Does not actually write to disk unless
       * the blob lived at the end of the heap file, in which 
       * case the file is truncated to its actual size.  Erasures
       * from the middle of the file are practically free.
       */
      bool eraseBlob(const std::vector<uint8_t> &id);

      /**
       * Will call eraseBlob on the ObjectId passed in first.
       * Then it will write the new object, growing the heap file
       * as necessary.
       */
      bool writeBlob(const std::vector<uint8_t> &id,
		     const std::vector<uint8_t> &blob);
      /**
       * Clears the state of the HeapFile...file, index and all.
       */
      void clear();

      /**
       * When this function returns, the size of the HeapFile on
       * disk will be no larger than the maxSize.  This will be
       * done by deleting blobs from the end of the heap file.
       * until a suitable size is reached.  This is fairly
       * effecient as we just call deallocate on the last allocated
       * Record until the metadata says we've made the cut--only
       * then do we truncate the file to the appropriate size.
       */
      void setMaxSize(uint64_t maxSize);

    private:
      HeapIndex m_index;
      MmapFile m_file;
      EncryptionPolicy m_key;
      uint64_t m_maxSize;
    };
  } // end namespace StructuredFiles
} // end namespace FileUtils

#endif // _HEAP_FILE_H_
