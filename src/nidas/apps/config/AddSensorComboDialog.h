#ifndef _config_AddSensorComboDialog_h
#define _config_AddSensorComboDialog_h

#include "ui_AddSensorComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"

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
        SuffixText->clear();
        this->hide();
        }

    void newSensor(QString);
    void existingSensor(SensorItem* sensorItem);
    void setDevice(int);
    void show(NidasModel* model,QModelIndexList indexList);
    bool setUpDialog();
    void A2DTempSetup(const QString & sensor);

public:

    AddSensorComboDialog(QWidget * parent = 0);

    ~AddSensorComboDialog() {}

    void setDocument(Document * document) {_document = document;}

protected:

    QMessageBox * _errorMessage;
    Document * _document;

private:
    QModelIndexList _indexList;
    NidasModel* _model;
};

}

#endif
