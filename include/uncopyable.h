#ifndef _UNCOPYABLE_H_
#define _UNCOPYABLE_H_ 1

//
// This class is from Scott Meyers' Effective C++, 3rd Edition (2005)
//
class Uncopyable {
protected:
  Uncopyable(){}  // allow classes that derive from Uncopyable to be
  ~Uncopyable(){} // created and destroyed
private:
  Uncopyable(const Uncopyable &);
  Uncopyable &operator=(const Uncopyable &);
};

#endif // _UNCOPYABLE_H_
