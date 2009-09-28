#ifndef _UserFriendlyExceptionHandler_h
#define _UserFriendlyExceptionHandler_h

#include <string>
#include <nidas/util/Exception.h>


class UserFriendlyExceptionHandler {
public:

 virtual void handle(std::string & where) {
   const char *what = 0;
   try { throw; }
   catch (nidas::util::Exception e) { what = e.what(); }
   catch (...) { }
   display(where,what);
   }

 void handle(const char *where) {
  std::string swhere(where);
  handle(swhere);
  };


 virtual void display(std::string& where, std::string& what) = 0;

 void display(const char* where, std::string& what) {
  std::string swhere(where);
  display(swhere,what);
  }
 void display(std::string& where, const char* what) {
  std::string swhat(what);
  display(where,swhat);
  }
 void display(const char* where, const char* what) {
  std::string swhere(where);
  std::string swhat(what);
  display(swhere,swhat);
  }


 // override to actually implement logging
 virtual void log(std::string & where, std::string & what) { display(where,what); }
 virtual void show() {}
 virtual void hide() {}
 virtual void setVisible(bool) {}


protected:

 UserFriendlyExceptionHandler() {};
 virtual ~UserFriendlyExceptionHandler() {};

};

#endif
