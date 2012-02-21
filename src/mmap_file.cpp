#include <mmap_file.h>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>


using namespace std;

namespace { // <anonymous>
  
  const off_t g_pageSize = getpagesize();

  void raise(const string &msg)
  {
    throw runtime_error(msg);
  }

  void raise(int fd, int err, const char *action)
  {
    ostringstream strm;
    strm << "Error " << action << " file at file descriptor "
	 << fd << " with error: " << strerror(err);

    throw runtime_error( strm.str() );
  }

  off_t effectiveWindowSize(off_t fileSize, 
			    off_t windowOffset, 
			    off_t windowSize)
  {
    // It's possible to mmap more bytes than exist in the file
    // if the file size is not a multiple of g_pageSize bytes.

    assert( windowOffset <= fileSize ); // INVARIANT
    return std::min(windowSize, fileSize - windowOffset);
  }

  off_t statFileSize(int fd) 
  {
    struct stat st;
    if (0 != fstat(fd, &st))
      ::raise(fd, errno, "stat'ing");

    return st.st_size;
  }

  off_t setFileSize(int fd, off_t size)
  {
    if (0 != ftruncate(fd, size))
      ::raise(fd, errno, "truncating");

    assert( statFileSize(fd) == size );
    return size;
  }

  off_t growFile(int fd, off_t size)
  {
    assert( statFileSize(fd) < size );    
    return setFileSize(fd, size);
  }

  char *mmapFile(int fd, off_t offset, off_t size)
  {
    // MAP_FILE   - map a regular file to virtual memory
    // MAP_SHARED - writes are public (don't copy on-write)
    const int g_mmapFlags = MAP_FILE | MAP_SHARED;
    
    // PROT_READ  - mapped memory may be read
    // PROT_WRITE - mapped memory may be written to
    const int g_protectionFlags = PROT_READ | PROT_WRITE;


    assert( 0 == (offset % g_pageSize) );
    assert( 0 == (size % g_pageSize) );

    void *ptr = mmap(NULL, size, g_protectionFlags, g_mmapFlags, fd, offset);

    if (MAP_FAILED == ptr)
      ::raise(fd, errno, "mmap'ing");

    return static_cast<char *>(ptr);
  }
} // end namespace <anonymous>


namespace FileUtils {

  MmapFile::MmapFile(const string &path)
    : m_path(path), m_fd(0), m_fileSize(0), m_offset(0),
      m_windowSize(g_pageSize), m_begin(NULL)
  {
    // O_RDWR   - open file for reading and writing
    // O_CREAT  - create the file if it does not exist
    const int flags = O_RDWR | O_CREAT;

    // S_IRUSR - Read bit for file owner
    // S_IWUSR - Write bit for file owner
    const mode_t privs  = S_IRUSR | S_IWUSR;

    if (0 > (m_fd = open(m_path.c_str(), flags, privs)))
      ::raise("Failed to open " + m_path + " with error: " + strerror(errno));
      
    m_fileSize = statFileSize(m_fd);
    
    m_begin = mmapFile(m_fd, m_offset, m_windowSize);

  }

  MmapFile::~MmapFile()
  {
    // block until we write the mapped pages back to disk
    msync(m_begin, m_windowSize, MS_SYNC);
    munmap(m_begin, m_windowSize);
    close(m_fd);
  }
  
  void MmapFile::unmap() const
  {
    if( 0 != munmap(m_begin, m_windowSize) ) 
      ::raise(string("Failed to unmap memory w/ error: ") + strerror(errno));

    // these guys are mutable
    m_begin      = NULL;
    m_offset     = 0;
    m_windowSize = 0;
  }

  void MmapFile::clear()
  {
    unmap();
    m_fileSize   = setFileSize(m_fd, 0);
    m_windowSize = g_pageSize;
    m_begin = mmapFile(m_fd, m_offset, m_windowSize);
  }

  void MmapFile::trim(off_t numBytesToKeep)
  {
    unmap();
    m_fileSize = setFileSize(m_fd, numBytesToKeep);
    m_windowSize = g_pageSize;
    m_begin = mmapFile(m_fd, m_offset, m_windowSize);
  }

  bool MmapFile::isInWindow(off_t offset, off_t size) const
  {
    off_t mappedSize = effectiveWindowSize(m_fileSize, m_offset, m_windowSize);

    //     { size }
    //     |      |
    // -|--|------|--|- // this line is addresses in memory
    //  |  b      c  |
    //  a            d
    //  { mappedSize } 
    //
    // a is at position m_offset
    // b is at position offset
    // (b - c) is size bytes wide
    // (d - a) is mappedSize bytes wide
    return 
      m_offset <= offset &&
      m_offset + mappedSize >=
      offset + size;
  }

  const void *MmapFile::getPtr(const off_t offset, const off_t size) const
  {
    //
    // Clients of this class could still cast away constness and edit the
    // mapped-to memory, but I can't stop them from doing that...the same is
    // true of std::string::c_str(), however.  To really address this problem 
    // I'd have to provide read/write modes to this class which is not worth
    // doing right now but certainly worth thinking about in the future.
    //
    if( offset >= m_fileSize ) 
      return NULL;
    return const_cast<MmapFile &>(*this).getPtr(offset, size);
  }

  void *MmapFile::getPtr(const off_t offset, const off_t size)
  {
    if (isInWindow(offset, size))
      return m_begin + (offset - m_offset);

    unmap();

    // put the offset on a page boundary
    m_offset = offset - (offset % g_pageSize);

    // make the size a multiple of g_pageSize
    m_windowSize = (offset - m_offset + size);
    m_windowSize += g_pageSize - (m_windowSize % g_pageSize);

    if( m_fileSize < offset + size )
      m_fileSize = growFile(m_fd, offset + size);

    m_begin = mmapFile(m_fd, m_offset, m_windowSize);

    assert( m_offset <= offset );
    assert( m_offset + m_windowSize >= offset + size );
    
    return m_begin + (offset - m_offset);
  }

} // end namespace FileUtils

