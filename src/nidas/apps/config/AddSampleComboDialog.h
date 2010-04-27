#ifndef _config_AddSampleComboDialog_h
#define _config_AddSampleComboDialog_h

#include "ui_AddSampleComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"

namespace config
{

class AddSampleComboDialog : public QDialog, public Ui_AddSampleComboDialog
{
    Q_OBJECT

public slots:
    //void accept() ;

    void reject() {
        SampleIdText->clear();
        SampleRateText->clear();
        FilterText->clear();
        this->hide();
        }

    //void newSample(QString);
    void show();
    bool setUpDialog();

public:

    AddSampleComboDialog(QWidget * parent = 0);

    ~AddSampleComboDialog() {}

    void setDocument(Document * document) {_document = document;}
    void accept();

protected:

    QMessageBox * _errorMessage;
    Document * _document;
};

}

#endif
