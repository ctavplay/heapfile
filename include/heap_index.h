#ifndef _HEAP_INDEX_H_
#define _HEAP_INDEX_H_ 1

#include <iosfwd>
#include <list>
#include <map>
#include <memory>
#include <stdint.h>
#include <uncopyable.h>

namespace FileUtils {
  namespace StructuredFiles {

    /**
     * Each instance of Record keeps metadata about one Blob instance:
     * it's location in the file, the hash code of the ObjectId,
     * and the capacity of the Blob on disk.
     * A record knows not whether the Blob it describes is empty or not
     * by design.
     */
    class Record {
    public:
      /**
       * The size of a Record instance in bytes.
       */
      static const std::size_t SERIALIZED_SIZE;

      /**
       * The minimum size of a Blob.
       */
      static const uint32_t MIN_SIZE;
      
      Record();
      Record(uint64_t offset, uint32_t key, uint32_t size, bool toMinSize=false);
      Record(const Record &lhs, const Record &rhs);
      explicit Record(const char *&p);
    
      /**
       * The capacity of the described Blob.
       */
      uint32_t size()   const { return m_size;   }

      /**
       * The hash value of the ObjectId stored in the described Blob.
       */
      uint32_t key()    const { return m_key;    }

      /**
       * The location of the described Blob on disk
       */
      uint64_t offset() const { return m_offset; }

      void setKey(uint32_t key) { m_key = key; }

      /**
       * Returns true of the Blob described by the Record referenced by
       * _rhs_ is butting up against and to the right of the Blob this
       * Record instance describes.
       */
      bool sharesRightBoundaryWith(const Record &rhs) const;

      /**
       * Serializes this Record instance.  Advances p by the size
       * of this Record (Record::SERIALIZED_SIZE bytes).
       */
      uint32_t serialize(char *&p) const;

      /**
       * Deserializes data pointed to by p.  Advances p by the size
       * of this Record (Record::SERIALIZED_SIZE bytes).
       */
      uint32_t deserialize(const char *&p);

      /**
       * Absorbs the metadata in _r_ into this Record object.  It's used
       * in the deallocation of a Record for merging/coalescing two
       * adjacent free Blobs.  It is expected that the Record referenced
       * by _r_ will soon be deleted
       */
      void coalesce(const Record &r);

      /**
       * Splits this Record into two.  The returned record is the left-most
       * _size_ bytes of this record.  This is used in the allocation of
       * a free record that is too big for the requested size.
       */
      std::auto_ptr<Record> splitOffLeft(const uint32_t size);
      
    private:
      uint64_t m_offset; // offset into file where cache entry is stored
      uint32_t m_key;    // a hash of the ObjectId;
      uint32_t m_size;   // the size of the payload
    };


    bool operator==(const Record &lhs, const Record &rhs);
    bool operator!=(const Record &lhs, const Record &rhs);
    std::ostream &operator<<(std::ostream &strm, const Record &e);


    typedef std::multimap<uint32_t, Record *> RecordMap;
    typedef std::list<Record *> RecordList;


    /**
     * This is a variation on the implicit free-list implementation
     * of malloc/free found in Kernighan and Ritchie's The C Programming
     * Language, Second Edition (aka K&R).
     *
     * Unlike the K&R implementation, the metadata portion of each allocated
     * block is divorced from the actual payload.  The intent is to keep
     * all the metadata in memory while the heap file is open.  There is no
     * sbrk() equivalent here.  When we fail to find a sufficiently large
     * free block, we fail.  New allocations are added w/ addAllocatedBlock().
     *
     * Each Record (whether allocated or free) is kept in a std::list
     * ordered by offset into the file with the property that for any two
     * Records at index i and i+1 in this list, the Record at i shares
     * its right-most boundary with the Record at i+1.
     *
     * There are two more data structures.  Each Record is additionally stored
     * in either the multimap for allocated Records or the multimap for
     * free Records.  The key of the allocated Records multimap is 
     * the hash code of the ObjectId of the Object stored on disk.  Provided
     * there are few if any collisions in this multimap, reads for previously
     * stored objects are nearly constant time.  Testing for the existence of
     * an object is O(log N) in the multimap where N is the number of 
     * allocated Records, but it's only O(M) trips to the disk
     * where M is the number of unique ObjectIds that hash to the same
     * value (a collision).  The number of collisions will largely depend 
     * on my choice of hash function and the distribution of ObjectIds.
     *
     * For allocation requests, the free Records multimap is key'ed on
     * the size of each Record--this is expected to collide more often,
     * however, multimap::lower_bound gives us logarithmic running time
     * in the number of free blocks.  This beats K&R's linear time
     * lookup for a suitable free block.
     *
     * This class manages the moving of Records into and out of
     * their respective multimaps as allocations and deallocations
     * happen.  It will also coalesce adjacent free blocks and manage
     * the construction/destruction of new Record instances as needed.
     */
    class HeapIndex : private Uncopyable {
    public:

      ~HeapIndex();

      /**
       * Note the transfer of ownership with the auto_ptr passed by
       * value.  This will append to the Record list the newly allocated
       * block.  It is expected that the new block is contiguous with 
       * the last Record in m_list.
       */ 
      void addAllocatedBlock(std::auto_ptr<Record> p);
      
      /**
       * Clears the index entirely.  Called by the destructor.
       */
      void clear();

      /**
       * Is the passed-in Record in the free Records multimap?
       */
      bool isFree(const Record &r)   const;

      /**
       * Is this Record at the end of the index? (Does it have the
       * highest offset?)
       */
      bool isLast(const Record &r)   const;
      
      /**
       * Returns the number of allocated Records.
       */
      uint32_t numAllocatedRecords() const { return m_alloc.size(); }

      /**
       * Returns the number of free Records.
       */
      uint32_t numFreeRecords()      const { return m_free.size();  }

      /*
       * Returns the number of bytes this index will take up on disk.
       * It is sufficent to store only the allocated Records.
       */
      uint32_t size() const; // in bytes

      /*
       * Analagous to K&R's free()
       */
      bool deallocate(const Record &r);
      
      /*
       * Analogous to K&R's malloc()...note the key
       * is for placement in the allocRecords() multimap.
       */
      Record *allocate(uint32_t size, uint32_t key);
      
      // used for testing, primarily.
      const RecordList &allRecords()  const { return m_list;  }
      const RecordMap &allocRecords() const { return m_alloc; }
      const RecordMap &freeRecords()  const { return m_free;  }
      
    private:
      bool coalesce(Record *r);

      RecordList m_list;  // list of all blocks, sorted by offset
      RecordMap  m_alloc; // lookup of allocated records by Record::key()
      RecordMap  m_free;  // lookup of free records by Record::size()
    };


  } // end namespace StructuredFiles
} // end namespace FileUtils

#endif // _HEAP_INDEX_H_
