#ifndef _CancelProcessingException_h
#define _CancelProcessingException_h

class CancelProcessingException : public nidas::util::Exception
{
 public:

  CancelProcessingException(const std::string & msg) :
    nidas::util::Exception("CancelProcessingException",msg)
    { }

  CancelProcessingException(const nidas::util::Exception & e) :
    nidas::util::Exception("CancelProcessingException",e.what())
    { }

  CancelProcessingException* clone() const {
    return new CancelProcessingException(*this);
    }

};

#endif
