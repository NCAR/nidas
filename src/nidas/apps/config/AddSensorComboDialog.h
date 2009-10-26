#ifndef _config_AddSensorComboDialog_h
#define _config_AddSensorComboDialog_h

#include "ui_AddSensorComboDialog.h"
#include <iostream>
#include <QMessageBox>

namespace config
{

class AddSensorComboDialog : public QDialog, public Ui_AddSensorComboDialog
{
    Q_OBJECT

public slots:
    void accept() ;

    void reject() {
        DeviceText->clear();
        IdText->clear();
        this->hide();
        }

public:

    AddSensorComboDialog(QWidget * parent = 0);

    ~AddSensorComboDialog() {}

protected:

    QMessageBox * _errorMessage;
};

}

#endif
