#ifndef _ConsoleExceptionHandler_h
#define _ConsoleExceptionHandler_h


#include "UserFriendlyExceptionHandler.h"
#include <string>
#include <iostream>

class UserFriendlyExceptionHandler;

class ConsoleExceptionHandler : public UserFriendlyExceptionHandler {

public:

 void display(std::string& where, std::string& what)
  { std::cerr << where << std::endl << "    " << what << std::endl; }

 ~ConsoleExceptionHandler() {}

};
#endif
