#include <simple_encrypt.h>

namespace { // <anonymous>

  template<typename T>
  bool XOR(const std::vector<T> &key,
	   const T *in, T *out, const std::size_t size)
  {
    if (0 == key.size())
      return false;

    for(std::size_t i = 0; i < size; ++i) {
      out[i] = in[i] ^ key[ i % key.size() ];
    }

    return true;
  }

  template<typename T>
  bool XOR(const std::vector<T> &key,
	   const std::vector<T> &in,
	   std::vector<T> &out)
  {
    out.resize(in.size());
    return XOR(key, &in[0], &out[0], in.size());
  }
} // end namespace <anonymous>

namespace Encryption {

  template<typename T>
  bool Simple<T>::encrypt(const std::vector<T> &in, std::vector<T> &out) const
  {
    return XOR(m_key, in, out);
  }

  template<typename T>
  bool Simple<T>::decrypt(const std::vector<T> &in, std::vector<T> &out) const
  {
    return XOR(m_key, in, out);
  }

  template<typename T>
  bool Simple<T>::encrypt(const T *in, T *out, std::size_t size) const {
    return XOR(m_key, in, out, size);
  }

  template<typename T>
  bool Simple<T>::decrypt(const T *in, T *out, std::size_t size) const {
    return XOR(m_key, in, out, size);
  }
  
  template class Simple<uint8_t>;
  template class Simple<uint16_t>;
  template class Simple<uint32_t>;
  template class Simple<uint64_t>;

} // end namespace Encryption
