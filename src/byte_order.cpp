#include <byte_order.h>

uint64_t htonll(uint64_t ll)
{
#ifdef __LITTLE_ENDIAN__
  uint64_t tmp = htonl(static_cast<uint32_t>(ll));
  tmp <<= 32;
  tmp |= htonl(static_cast<uint32_t>(ll >> 32));
  return tmp;
#else
  return ll;
#endif
}

uint64_t ntohll(uint64_t ll)
{
  return htonll(ll);
}
