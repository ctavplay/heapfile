#include <byte_order.h>
#include <unit_test.h>

using namespace EndianUtils;

namespace { // <anonymous>

  void testByteOrder64(UnitTestControl &utc)
  {
    const uint64_t forwards  = 0x0123456789abcdef;
    const uint64_t backwards = 0xefcdab8967452301;
    const uint64_t zero = 0x0;

    const uint16_t small  = 1 << 8;
    const uint32_t medium = 1 << 16;
    const uint64_t large  = uint64_t(1) << 32;
#ifdef __LITTLE_ENDIAN__
    TEST_ASSERT(utc, backwards == htonll(forwards));
    TEST_ASSERT(utc, forwards == ntohll(backwards));
#else
    TEST_ASSERT(utc, backwards == htonll(backwards));
    TEST_ASSERT(utc, forwards == ntohll(forwards));
#endif
    TEST_ASSERT(utc, backwards == ntohll(htonll(backwards)));
    TEST_ASSERT(utc, forwards == htonll(ntohll(forwards)));

    TEST_ASSERT(utc, zero == ntohll(zero) );
    TEST_ASSERT(utc, ~zero == ntohll(~zero) );
    
    TEST_ASSERT(utc, zero == htonll(zero) );
    TEST_ASSERT(utc, ~zero == htonll(~zero) );

    TEST_ASSERT(utc, h2n(small) == htons(small));
    TEST_ASSERT(utc, n2h(small) == ntohs(small));

    TEST_ASSERT(utc, h2n(medium) == htonl(medium));
    TEST_ASSERT(utc, n2h(medium) == ntohl(medium));

    TEST_ASSERT(utc, h2n(large) == htonll(large));
    TEST_ASSERT(utc, n2h(large) == ntohll(large));

  }
} // end namespace <anonymous>

REGISTER_TEST(testByteOrder64, &::testByteOrder64)
