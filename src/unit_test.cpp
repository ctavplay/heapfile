#include <unit_test.h>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>


namespace { // <anonymous>
  typedef std::map<TestCaseFnPtr, std::string> TestsMap;
  TestsMap &getTests() {
    static TestsMap g_tests;
    return g_tests;
  }
} // end namespace <anonymous>

UnitTestControl::UnitTestControl(std::ostream &ostrm, std::ostream &errStrm)
  : m_ostrm(ostrm), m_errStrm(errStrm), 
    m_lastTestPassed(true), m_lastTestAsserted(false)
{}

bool UnitTestControl::addTestCase(const std::string &name, TestCaseFnPtr ptr)
{
  getTests()[ptr] = name; // two tests may have the same name, but not the same address!
  return true; // see the REGISTER_TEST macro in unit_test.h
}

void UnitTestControl::preTest()
{
  m_lastTestPassed = true;
  m_lastTestAsserted = false;
}

void UnitTestControl::assertTrue(bool b, int line, const char *file)
{
  m_lastTestPassed = m_lastTestPassed && b;
  m_lastTestAsserted = true;

  if (not b)
    errStream() << "\n\t***assertion failed at " << file << ":" << line << "\n";
}

int UnitTestControl::runTests(const std::string &testName)
{
  typedef TestsMap::iterator Itr;

  bool allTestsPass = true;
  for(Itr itr = getTests().begin(), itrEnd = getTests().end();
      itr != itrEnd; ++itr) {
    if( not testName.empty() and testName != itr->second )
      continue;
    allTestsPass = runTest(itr->second, itr->first) && allTestsPass;
  }

  return allTestsPass ? EXIT_SUCCESS : EXIT_FAILURE;
}

bool UnitTestControl::runTest(const std::string &testName, TestCaseFnPtr ptr)
{
  try {
    preTest();
    ptr(*this);
  }catch(std::exception &e) {
    errStream() 
      << "\n\t\tTest " << testName
      << " did not catch exception with msg: \"" << e.what()
      << "\"" << std::endl;
    m_lastTestAsserted = true;
    m_lastTestPassed = false;
  }catch(...) {
    errStream()
      << "\n\t\tTest " << testName
      << " threw an unkown object or exception"
      << std::endl;
  }

  stream() << "    Running test " << testName;

  bool testPassed = false;
  if( not m_lastTestAsserted )
    stream() << "...[NO ASSERTIONS]\n";
  else if( m_lastTestPassed ){
    stream() << "...[OK]\n";
    testPassed = true;
  }
  else
    stream() << "...[FAILED]\n";

  return testPassed;
}

int main(int argc, char *argv[])
{
  UnitTestControl utc(std::cout, std::cerr);

  if (argc > 1)
    return utc.runTests(argv[1]);
  
  return utc.runTests();
}
