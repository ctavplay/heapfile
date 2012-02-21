#ifndef _SIMPLE_ENCRYPT_H_
#define _SIMPLE_ENCRYPT_H_ 1

#include <vector>

namespace Encryption {
  
  template<typename T>
  class Simple {
  public:
    typedef T value_type;

    explicit Simple(const std::vector<T> &key)
      : m_key(key)
    {
      if (m_key.empty())
	m_key.resize(1);
    }

    /**
     * It is possible to decrypt and encrypt in-place by
     * passing in references to the same vector or area of memory
     * depending on your choice of overload.
     */
    bool encrypt(const std::vector<T> &in, std::vector<T> &out) const;
    bool decrypt(const std::vector<T> &in, std::vector<T> &out) const;
    
    bool encrypt(const T *in, T *out, std::size_t size) const;
    bool decrypt(const T *in, T *out, std::size_t size) const;

    std::vector<T> m_key;
  };
} // end namespace Encryption

#endif // _SIMPLE_ENCRYPT_H_
