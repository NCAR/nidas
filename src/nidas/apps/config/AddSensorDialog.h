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
    void accept() {
        std::cerr << "AddSensorDialog::accept()\n";
        std::cerr << " " + SensorText->text().toStdString() + "\n";
        std::cerr << " " + DeviceText->text().toStdString() + "\n";
        std::cerr << " " + IdText->text().toStdString() + "\n";
        }

    void reject() {
        SensorCatTbl->setItemSelected( SensorCatTbl->currentItem(), false );
        SensorText->clear();
        DeviceText->clear();
        IdText->clear();
        this->hide();
        }

    void setSensorText(QTableWidgetItem *item) {
        SensorText->setText(item->text());
        }

public:
    AddSensorDialog(QWidget * parent = 0);
    ~AddSensorDialog() {}

};

}

#endif
