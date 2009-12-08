#ifndef _InternalProcessingException_h
#define _InternalProcessingException_h

class InternalProcessingException : public nidas::util::Exception
{
 public:

  InternalProcessingException(const std::string & msg) :
    nidas::util::Exception("InternalProcessingException",msg)
    { }

  InternalProcessingException(const nidas::util::Exception & e) :
    nidas::util::Exception("InternalProcessingException",e.what())
    { }

  InternalProcessingException* clone() const {
    return new InternalProcessingException(*this);
    }

};

#endif
