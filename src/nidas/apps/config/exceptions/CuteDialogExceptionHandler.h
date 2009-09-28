#ifndef _CuteDialogExceptionHandler_h
#define _CuteDialogExceptionHandler_h


#include "UserFriendlyExceptionHandler.h"
#include <string>
#include <QMessageBox>

class CuteDialogExceptionHandler : public UserFriendlyExceptionHandler {

public:

void display(std::string& where, std::string& what) {
    errorMessageDialog->showMessage( QString::fromStdString(where+what) );
    }



CuteDialogExceptionHandler(QWidget * parent = 0)
{
errorMessageDialog = new QErrorMessage(parent);
}



~CuteDialogExceptionHandler() {}



protected:

  QErrorMessage *errorMessageDialog;

};

#endif
