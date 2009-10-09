#ifndef _config_AddSensorDialog_h
#define _config_AddSensorDialog_h

#include "ui_AddSensorDialog.h"
#include <iostream>

namespace config
{

class AddSensorDialog : public QDialog, public Ui_AddSensorDialog
{
    Q_OBJECT

public slots:
    void accept() { std::cerr << "AddSensorDialog::accept()\n"; }

    void reject() {
        std::cerr << "AddSensorDialog::reject()\n";
        //SensorCatTbl->clear(); // no: this clears all items too, we just want to clear selection
        SensorText->clear();
        DeviceText->clear();
        IdText->clear();
        this->hide();
        }

    void copy() { std::cerr << "AddSensorDialog::copy()\n"; }

    void setSensorText(QTableWidgetItem *item) {
     std::cerr << "AddSensorDialog::setSensorText()\n";
     SensorText->setPlainText(item->text());
     }

public:
    AddSensorDialog(QWidget * parent = 0);
    ~AddSensorDialog() {}

};

}

#endif
