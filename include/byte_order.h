#ifndef _BYTE_ORDER_H_
#define _BYTE_ORDER_H_ 1

#include <arpa/inet.h>
#include <cassert>
#include <cstring>

/**
 * A namespace to encapsulate the functions that convert
 * values between host and network byte order.  Provides 
 * wrapper functions h2n() and n2h(), overloaded, so we
 * never have to remember which one to call.
 * Also provides functions for writing and reading in a
 * byte-order-independent way to an area of of memory
 * which is why h2n() and n2h() is defined for uint8_t. 
 */

// these aren't on BSD
uint64_t htonll(uint64_t ll);
uint64_t ntohll(uint64_t ll);

namespace EndianUtils {

  inline uint8_t h2n(uint8_t c)
  {
    return c; // for use w/ readN2H and writeH2N below
  }

  inline uint8_t n2h(uint8_t c)
  {
    return c; // for use w/ readN2H and writeH2N below
  }

  inline uint16_t h2n(uint16_t hs)
  {
    return htons(hs);
  }

  inline uint16_t n2h(uint16_t ns)
  {
    return ntohs(ns);
  }

  inline uint32_t h2n(uint32_t hl)
  {
    return htonl(hl);
  }

  inline uint32_t n2h(uint32_t nl)
  {
    return ntohl(nl);
  }

  inline uint64_t h2n(uint64_t hll)
  {
    return htonll(hll);
  }

  inline uint64_t n2h(uint64_t nll)
  {
    return ntohll(nll);
  }

  /**
   * Reads sizeof(T) bytes from the memory
   * pointed to by p, then advances p by that
   * many bytes and assigns the bytes read to ret
   * in host byte order.
   *
   * @param p the ref pointer to const where memory will be read from
   * @param ret the reference to where the bytes read will be stored
   * @return the number of bytes read, always sizeof(T)
   */

  template <typename T, typename V>
  std::size_t readN2H(const V *&p, T &ret)
  {
    assert(NULL != p);
    ret = n2h(*(reinterpret_cast<const T *&>(p)));
    p += sizeof(T)/sizeof(V);
    return sizeof(T);
  }
  
  /**
   * Writes t to the location pointed by p in
   * network byte order, then it advances p by
   * the sizeof(T).
   *
   * @param p the ref pointer to const where memory will be written to
   * @param t the value to be written.
   * @return the number of bytes written, always sizeof(T)
   */
  template <typename T, typename V>
  std::size_t writeH2N(V *&p, T t)
  {
    assert(NULL != p);
    t = h2n(t);
    memcpy(p, &t, sizeof(T));
    p += sizeof(T)/sizeof(V);
    return sizeof(T);
  }
} // end namespace EndianUtils
#endif // _BYTE_ORDER_H_ 
