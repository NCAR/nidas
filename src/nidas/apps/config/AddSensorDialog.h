#ifndef _config_AddSensorDialog_h
#define _config_AddSensorDialog_h

#include "ui_AddSensorDialog.h"

namespace config
{

class AddSensorDialog : public QDialog, public Ui_AddSensorDialog
{
    Q_OBJECT

public slots:
    void accept() { cerr << "AddSensorDialog::accept()\n"; }
    void reject() { cerr << "AddSensorDialog::reject()\n"; }
    void copy() { cerr << "AddSensorDialog::copy()\n"; }

public:
    AddSensorDialog(QWidget * parent = 0);
    ~AddSensorDialog() {}

};

}

#endif
