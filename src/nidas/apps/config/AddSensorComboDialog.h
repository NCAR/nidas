#ifndef _config_AddSensorComboDialog_h
#define _config_AddSensorComboDialog_h

#include "ui_AddSensorComboDialog.h"
#include <iostream>

namespace config
{

class AddSensorComboDialog : public QDialog, public Ui_AddSensorComboDialog
{
    Q_OBJECT

public slots:
    void accept() {
        std::cerr << "AddSensorComboDialog::accept()\n";
        std::cerr << " " + SensorBox->currentText().toStdString() + "\n";
        std::cerr << " " + DeviceText->text().toStdString() + "\n";
        std::cerr << " " + IdText->text().toStdString() + "\n";
        }

    void reject() {
        DeviceText->clear();
        IdText->clear();
        this->hide();
        }

public:

    AddSensorComboDialog(QWidget * parent = 0);

    ~AddSensorComboDialog() {}

};

}

#endif
