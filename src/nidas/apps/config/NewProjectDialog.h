#ifndef _config_NewProjectDialog_h
#define _config_NewProjectDialog_h

#include "ui_NewProjectDialog.h"
#include <iostream>
#include <QMessageBox>

class ConfigWindow;

namespace config 
{

class NewProjectDialog : public QDialog, public Ui_NewProjectDialog
{
    Q_OBJECT

public slots:
    //void accept() ;

    void reject() {
        ProjName->clear();
        this->hide();
        }

    void show();
    bool setUpDialog();

public:

    NewProjectDialog(QString projDir, QWidget * parent = 0);

    ~NewProjectDialog() {}

//    void setConfigWin(ConfigWindow * configWin) {_configWin=configWin;}
    void accept();


protected:

    QMessageBox * _errorMessage;
    QString * _fileName;
//    ConfigWindow * _configWin;

private:
    QString _defaultDir;
    ConfigWindow* _confWin;

};

}

#endif
