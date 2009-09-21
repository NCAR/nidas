#ifndef _UserFriendlyExceptionHandler_h
#define _UserFriendlyExceptionHandler_h

#include <string>
#include <nidas/util/Exception.h>


class UserFriendlyExceptionHandler {
public:

 static void handleException(std::string where) {
  getImplementation()->handleExceptionImplementation(where);
  };

 static void handleException(const char *where) {
  std::string swhere(where);
  handleException(swhere);
  };


 virtual void displayException(std::string& where, std::string& what) = 0;

 static void displayException(const char* where, std::string& what) {
  std::string swhere(where);
  getImplementation()->displayException(swhere,what);
  }
 static void displayException(std::string& where, const char* what) {
  std::string swhat(what);
  getImplementation()->displayException(where,swhat);
  }
 static void displayException(const char* where, const char* what) {
  std::string swhere(where);
  std::string swhat(what);
  getImplementation()->displayException(swhere,swhat);
  }


 static UserFriendlyExceptionHandler* getImplementation();
 static void setImplementation(UserFriendlyExceptionHandler *);

protected:
 static UserFriendlyExceptionHandler *_impl;

 virtual void handleExceptionImplementation(std::string where) {
   const char *what = 0;
   try { throw; }
   catch (nidas::util::Exception e) { what = e.what(); }
   catch (...) { }
   getImplementation()->displayException(where,what);
   }

 UserFriendlyExceptionHandler() {};
 virtual ~UserFriendlyExceptionHandler();

};

#endif
