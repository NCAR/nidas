#ifndef _config_AddDSMComboDialog_h
#define _config_AddDSMComboDialog_h

#include "ui_AddDSMComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"

namespace config
{

class AddDSMComboDialog : public QDialog, public Ui_AddDSMComboDialog
{
    Q_OBJECT

public slots:
    //void accept() ;

    void reject() {
        DSMNameText->clear();
        DSMIdText->clear();
        LocationText->clear();
        this->hide();
        }

    //void newDSM(QString);
    //void setDevice(int);
    void show();
    bool setUpDialog();

public:

    AddDSMComboDialog(QWidget * parent = 0);

    ~AddDSMComboDialog() {}

    void setDocument(Document * document) {_document = document;}

protected:

    QMessageBox * _errorMessage;
    Document * _document;
};

}

#endif
