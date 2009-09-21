
#include "ConsoleExceptionHandler.h"
#include "UserFriendlyExceptionHandler.h"

UserFriendlyExceptionHandler* UserFriendlyExceptionHandler::_impl=0;

UserFriendlyExceptionHandler* UserFriendlyExceptionHandler::getImplementation() {
    return( UserFriendlyExceptionHandler::_impl ? UserFriendlyExceptionHandler::_impl : (UserFriendlyExceptionHandler::_impl = new ConsoleExceptionHandler()) );
    }

void UserFriendlyExceptionHandler::setImplementation(UserFriendlyExceptionHandler *i) {
    UserFriendlyExceptionHandler::_impl=i;
    }

UserFriendlyExceptionHandler::~UserFriendlyExceptionHandler() {
    if (UserFriendlyExceptionHandler::_impl) delete UserFriendlyExceptionHandler::_impl;
    }
