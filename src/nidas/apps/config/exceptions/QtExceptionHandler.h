#ifndef _QtExceptionHandler_h
#define _QtExceptionHandler_h


#include "UserFriendlyExceptionHandler.h"
#include <string>
#include <QMessageBox>

class QtExceptionHandler : public UserFriendlyExceptionHandler {

public:

virtual void displayException(std::string& where, std::string& what) {
    QMessageBox::information( 0,
        QString::fromStdString(where),
        QString::fromStdString(what), 
        "Acknowledged"
        );
    }

~QtExceptionHandler() {}

};

#endif
