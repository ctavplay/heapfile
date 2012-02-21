#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_ 1

#include <iosfwd>
#include <string>
#include <vector>

class UnitTestControl;
typedef void (*TestCaseFnPtr)(UnitTestControl &);

/**
 * A very rudimentary unit-testing framework.
 */
class UnitTestControl {
public:
  static bool addTestCase(const std::string &testName, TestCaseFnPtr fn);
  
  UnitTestControl(std::ostream &out, std::ostream &err);
  void assertTrue(bool b, int line, const char *file);
  std::ostream &stream() { return m_ostrm; }
  std::ostream &errStream() { return m_errStrm; }

  int runTests(const std::string &testName="");
  bool runTest(const std::string &testName, TestCaseFnPtr);

private:
  void preTest();
  void postTest();


  std::ostream &m_ostrm;
  std::ostream &m_errStrm;
  bool m_lastTestPassed;
  bool m_lastTestAsserted;
};

/**
 * Write your unit test function to look like the function pointer 
 * typedef above, TestCaseFnPtr, then invoke REGISTER_TEST
 * with test name and pointer to your function. 
 */
#define REGISTER_TEST(name, ptr)					\
  static bool register_test_##name = UnitTestControl::addTestCase(#name, ptr);
#define TEST_ASSERT(utc, assertion) utc.assertTrue(assertion, __LINE__, __FILE__)

#endif // _UNIT_TEST_H_
