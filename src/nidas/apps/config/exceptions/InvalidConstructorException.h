#ifndef _InvalidConstructorException_h
#define _InvalidConstructorException_h

/*
 * InvalidConstructorException
 *  - exception for unexpected internal problems,
 *    e.g. null pointers that should not happen
 */

class InvalidConstructorException : public nidas::util::Exception
{
 public:

  InvalidConstructorException(const std::string & msg) :
    nidas::util::Exception("InvalidConstructorException",msg)
    { }

  InvalidConstructorException(const nidas::util::Exception & e) :
    nidas::util::Exception("InvalidConstructorException",e.what())
    { }

  InvalidConstructorException* clone() const {
    return new InvalidConstructorException(*this);
    }

};

#endif
