#include <simple_encrypt.h>
#include <stdint.h>
#include <unit_test.h>
#include <vector>

using namespace std;
using namespace Encryption;
namespace { // <anonymous>

  void testEncryption(UnitTestControl &utc)
  {
    typedef vector<uint8_t> Bytes;
    Bytes vKey;
    vKey.push_back(0xde);
    vKey.push_back(0xad);
    vKey.push_back(0xbe);
    vKey.push_back(0xef);

    Simple<uint8_t> eKey(vKey);
   
    Bytes datum, encrypted;
    for(uint8_t i = 100; i < 200; ++i) {
      datum.push_back(i);
    }

    TEST_ASSERT(utc, Simple<uint8_t>(Bytes()).encrypt(datum, encrypted));
    TEST_ASSERT(utc, datum == encrypted);
    TEST_ASSERT(utc, eKey.encrypt(datum, encrypted));
    TEST_ASSERT(utc, datum.size() == encrypted.size());
    TEST_ASSERT(utc, datum != encrypted);

    Bytes decrypted;
    TEST_ASSERT(utc, eKey.decrypt(encrypted, decrypted));
    TEST_ASSERT(utc, datum == decrypted);

    TEST_ASSERT(utc, eKey.encrypt(datum, datum));
    TEST_ASSERT(utc, datum == encrypted);
    TEST_ASSERT(utc, datum != decrypted);
    TEST_ASSERT(utc, eKey.decrypt(datum, datum));
    TEST_ASSERT(utc, datum == decrypted);
    TEST_ASSERT(utc, datum != encrypted);

  }

} // end namespace <anonymous>

REGISTER_TEST(testEncryption, &::testEncryption)
