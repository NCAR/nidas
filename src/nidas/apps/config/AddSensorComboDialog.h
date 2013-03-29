#ifndef _config_AddSensorComboDialog_h
#define _config_AddSensorComboDialog_h

#include "ui_AddSensorComboDialog.h"
#include <iostream>
#include <map>
#include <QMessageBox>
#include "Document.h"
#include <raf/PMSspex.h>

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
    void dialogSetup(const QString & sensor);

public:

    //AddSensorComboDialog(QWidget * parent = 0);
    AddSensorComboDialog(QString a2dCalDir, QString pmsSpecsFile, QWidget *parent=0); 

    ~AddSensorComboDialog() {}

    void setDocument(Document * document) {_document = document;}
    void clearSfxMap() {_sfxMap.clear();}
    void addSensorSfx(QString sensor, QString sfx) {_sfxMap[sensor]=sfx;}
    void clearDevMap() {_devMap.clear();}
    void addSensorDev(QString sensor, QString dev) {_devMap[sensor]=dev;}

protected:

    QMessageBox * _errorMessage;
    Document * _document;

private:
    void setupPMSSerNums(QString pmsSpecsFile);
    std::map<std::string, std::string> _pmsResltn;  // for RESOLUTION indicator
    void setupA2DSerNums(QString a2dCalDir);
    QModelIndexList _indexList;
    NidasModel* _model;
    map<QString, QString> _sfxMap;
    map<QString, QString> _devMap;
};

}

#endif
