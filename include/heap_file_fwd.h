#ifndef _HEAP_FILE_FWD_H_
#define _HEAP_FILE_FWD_H_ 1

#include <stdint.h>

namespace Encryption {
  template <typename> class Simple;
}

namespace FileUtils {
  namespace StructuredFiles {

    /**
     * Forward declaration for HeapFileT.  HeapFile is a typedef
     * of HeapFileT with the default encryption policy class.  Here it's
     * simple XOR encryption.  To change the encryption
     * used by HeapFileT, provide a class that implements
     * the implicit type interface of Encryption::Simple
     * and instantiate a HeapFileT with it as a type argument.
     */
    typedef Encryption::Simple<uint8_t> DefaultEncryptionPolicy;
    template <typename> class HeapFileT;
    typedef HeapFileT<DefaultEncryptionPolicy> HeapFile;

  }
}

#endif // _HEAP_FILE_FWD_H_
