#ifndef _UserFriendlyExceptionHandler_h
#define _UserFriendlyExceptionHandler_h

#include <string>
#include <nidas/util/Exception.h>


class UserFriendlyExceptionHandler {
public:

 virtual void handleException(std::string & where) {
   const char *what = 0;
   try { throw; }
   catch (nidas::util::Exception e) { what = e.what(); }
   catch (...) { }
   displayException(where,what);
   }

 void handleException(const char *where) {
  std::string swhere(where);
  handleException(swhere);
  };


 virtual void displayException(std::string& where, std::string& what) = 0;

 void displayException(const char* where, std::string& what) {
  std::string swhere(where);
  displayException(swhere,what);
  }
 void displayException(std::string& where, const char* what) {
  std::string swhat(what);
  displayException(where,swhat);
  }
 void displayException(const char* where, const char* what) {
  std::string swhere(where);
  std::string swhat(what);
  displayException(swhere,swhat);
  }


protected:

 UserFriendlyExceptionHandler() {};
 virtual ~UserFriendlyExceptionHandler() {};

};

#endif
