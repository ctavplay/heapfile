#include <file_cache.h>
#include <heap_file.h>
#include <stdexcept>

using namespace std;
using namespace FileUtils;
using namespace FileUtils::StructuredFiles;

namespace FileCache {

  HeapFileCache::HeapFileCache(const string &path,
			       const vector<uint8_t> &encryptionKey)
    : m_pImpl(new HeapFile(path + "/cache.dat", encryptionKey))
  {}

  HeapFileCache::~HeapFileCache()
  {
    delete m_pImpl;
  }

  bool HeapFileCache::hasObject(const ObjectId &objId) const
  {
    return m_pImpl->hasBlob(objId);
  }

  bool HeapFileCache::readObject(const ObjectId &objId,
				 vector<uint8_t> &object) const
  {
    return m_pImpl->getBlob(objId, object);
  }
  
  uint64_t HeapFileCache::getCurrentSize() const
  {
    return m_pImpl->size();
  }
  
  bool HeapFileCache::eraseObject(const ObjectId &objId)
  {
    return m_pImpl->eraseBlob(objId);
  }
  
  bool HeapFileCache::writeObject(const ObjectId &objId,
				  const vector<uint8_t> &object)
  {
    return m_pImpl->writeBlob(objId, object);
  }
  
  void HeapFileCache::setMaxSize(uint64_t maxSize)
  {
    m_pImpl->setMaxSize(maxSize);
  }

  HeapFileCache *HeapFileCache::createFileCache(const string &path,
						const vector<uint8_t> &key)
  {
    return new HeapFileCache(path, key);
  }
} // end namespace FileCache


Cache *createFileCache(const string &path,
		       const vector<uint8_t> &encryptionKey)
{
  string cmd = "mkdir -p ";
  cmd += path;
  if (0 != system(cmd.c_str()))
    return NULL;

  try {
    return FileCache::HeapFileCache::createFileCache(path, encryptionKey);
  }catch(const std::exception &e) {
    return NULL;
  }
}
