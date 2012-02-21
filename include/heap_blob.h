#ifndef _HEAP_BLOB_H_
#define _HEAP_BLOB_H_ 1

#include <stdint.h>
#include <vector>

namespace FileUtils {
  class MmapFile;

  namespace StructuredFiles {
    class Record;
    
    /**
     * An interface for writing data into a Blob object
     */
    class BlobWriter {
    public:
      virtual uint32_t size() const = 0;
      virtual void writeBlob(uint8_t *dest) const = 0;
    };

    /**
     * An interface for reading data from a Blob object
     */
    class BlobReader {
    public:
      virtual void readBlob(uint32_t size, const uint8_t *src) const = 0;
    };

    /**
     * This class encapsulates the layout of how the ObjectId and
     * and the Object are stored.
     * Each Record instance describes a Blob instance; as such,
     * each Blob intance keeps a reference to its describing 
     * Record instance.  The raw pointer it keeps around to write to
     * or read from is produced by an MmapFile, although this is not
     * a requirement--this pointer could be pointing anywhere.
     */
    class Blob
    {
    public:
      Blob();
      Blob(uint8_t *p, const Record &r);
      Blob(const Record &r, const MmapFile &file);

      /**
       * Tests if there's anywhere to read from or write to.
       */
      bool isNil() const { return NULL == m_ptr; }

      /**
       * Compares the ObjectId passed in with the one stored here.
       */
      bool hasId(const std::vector<uint8_t> &id) const;

      /**
       * Read the object stored by this Blob.  Returns true
       * on success.  By abstracting away the reading into
       * another class we prevent ourselves from copying/sifting through the
       * data a second time if, let's say, we'd like to decrypt
       * what is stored.  If the data has somehow become corrupt
       * it will return false w/o any attempt to call
       * BlobReader::readBlob().  Returns true on success.
       */
      bool getData(const BlobReader &reader) const;

      /**
       * Returns a reference to an object with all of this Blob's
       * metadata--it's size in bytes, the hash code of the ObjectId
       * and its location in the file.
       */
      const Record &record() const { return m_rec; }

      /**
       * Stores the Object and ObjectId in this Blob.  The BlobWriter
       * will only write the Object, the ObjectId is passed by reference.
       * By abstracting away the writing of the object into another
       * class we prevent ourselves from copying/sifting through the
       * data a second time if, let's say, we'd like to encrypt
       * what is stored.  If the data to be written is bigger than
       * the available space in this Blob, then it will return false;
       * Returns true on sucess.
       */
      bool writeData(const std::vector<uint8_t> &id,
		     const BlobWriter &writer);

      /**
       * Given the length in bytes of each the ObjectId and Object, this
       * returns the amount of space the Blob would take up on disk
       * in bytes.
       */
      static uint32_t blobSize(size_t keySize, size_t dataSize);

    private:
      const Record &m_rec;
      const uint8_t * const m_ptr;
    };

    /**
     * Functions for computing a hash value for raw data.  It's used
     * by the Record class to compute the hash of the ObjectId and
     * by Blob to compute the hash of the Object.  The hash is used
     * to test for corruption of data in Blob::getData();
     */
    uint32_t hash(const uint8_t *p, size_t size);
    uint32_t hash(const std::vector<uint8_t> &id);
  } // end namespace StructuredFiles
} // end namespace FileUtils

#endif // _HEAP_BLOB_H_
