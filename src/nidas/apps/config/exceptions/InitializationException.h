
#ifndef _InitializationException_h
#define _InitializationException_h

class InitializationException : public nidas::util::Exception
{
 public:

  InitializationException(const std::string & msg) :
    nidas::util::Exception("InitializationException",msg)
    { }

  InitializationException* clone() const {
    return new InitializationException(*this);
    }

};

#endif
