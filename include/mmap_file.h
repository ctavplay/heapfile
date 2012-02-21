#ifndef _MMAP_FILE_H_
#define _MMAP_FILE_H_ 1

#include <sys/types.h>
#include <stdexcept>
#include <string>
#include <uncopyable.h>

namespace FileUtils {

  /**
   * This class is a wrapper around mmap/munmap.  It will
   * grow the file as needed for writes, but not for reads.
   * It will manage the mapping and unmapping of pages from the
   * disk to pages in virtual memory in a sliding window
   * fashion, the window being the currently mapped bytes of the file.
   */
  class MmapFile : private Uncopyable {
  public:
    explicit MmapFile(const std::string &path);
    ~MmapFile();

    /**
     * The size of the file in bytes.
     */
    off_t size() const { return m_fileSize; }

    /**
     * Gets a pointer to an area of the file at _offset_ bytes
     * from the beginning of the file and it'll map _size_
     * bytes from that offset to memory.  If you attempt to read past the
     * end of the file, you'll get a NULL pointer.
     */
    template <typename T>
    const T *getReadPtr(off_t offset, off_t size=sizeof(T)) const {
      return static_cast<const T *>(getPtr(offset, size));
    }

    /**
     * Same as w/ getReadPtr, except the file is grown to the 
     * appropriate size if it is not big enough.  Will not return
     * NULL but will throw an exception on failure.
     */
    template <typename T>
    T *getWritePtr(off_t offset, off_t size=sizeof(T)) {
      return static_cast<T *>(getPtr(offset, size));
    }

    /**
     * Read a T at _offset_ in the file.  Returns whether or
     * not it was successful.
     */
    template <typename T>
    bool read(off_t offset, T &t) const {
      const T *p = getReadPtr<T>(offset);
      if (NULL == p)
	return false;
      t = *p;
      return true;
    }

    /**
     * An exception-based interface for reading.  Reads a T
     * at _offset_ in the file.  If the read
     * fails, it will throw an exception, otherwise, if it returns it was
     * successful.
     */
    template <typename T>
    void readOrThrow(off_t offset, T &t) const {
      if (not read(offset, t))
	throw std::exception();
    }

    /**
     * A slightly different interface for returning the T on the
     * stack, same behavior as the previous readOrThrow().
     */
    template <typename T>
    T readOrThrow(off_t offset) const {
      const T *t = getReadPtr<T>(offset);
      if (NULL == t)
	throw std::exception();
      return *t;
    }

    /**
     * The previous functions were for convenience and this is 
     * the real implementation that wraps mmap/munmap.  Subsequent
     * calls to getPtr() on the same window of bytes will not
     * munmap the previously mmap'ed pages and mmap them again
     * (because we did it the last time).  If you know you're 
     * going to need a reasonably sized range of bytes into 
     * your mmap'ed file, it is prefferable
     * to call getPtr() on the entire range of interest and then
     * continue to call getPtr() on smaller portions of interest to your
     * application.  Keep in mind that this is consuming your virtual
     * address space, so tread carefully.
     *
     * munmap commits for you, so seeing your chagnes
     * on disk are as easy as calling getPtr on a range outside
     * of your window or destroying your MmapFile intance.
     */
    void *getPtr(off_t offset, off_t size);
    const void *getPtr(off_t offset, off_t size) const;

    /**
     * A simple way of telling if your next invocation of getPtr()
     * will cause new pages to be paged in (and previoi\usly mmaped
     * pages to be paged out).
     */
    bool isInWindow(off_t offset, off_t size) const;

    /**
     * calls trim(0)
     */
    void clear();

    /**
     * Truncates the file to numBytesToKeep.  Adjusts the window
     * accordingly.
     */
    void trim(off_t numBytesToKeep);
  private:
    void unmap() const;


    std::string m_path;
    int m_fd;                   // file descriptor
    off_t m_fileSize;           // file size in bytes
    mutable off_t m_offset;     // offset into file, a multiple of the page size
    mutable off_t m_windowSize; // size of window into the file
    mutable char *m_begin;      // pointer into beginning of mmap'ed area
  };

} // end namespace FileUtils

#endif // _MMAP_FILE_H_
