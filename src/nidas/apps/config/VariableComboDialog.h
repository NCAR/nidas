#ifndef _config_VariableComboDialog_h
#define _config_VariableComboDialog_h

#include "ui_VariableComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"
#include "nidas_qmv/NidasModel.h"
#include "nidas_qmv/A2DVariableItem.h"

namespace config
{

class VariableComboDialog : public QDialog, public Ui_VariableComboDialog
{
    Q_OBJECT

public slots:
    void accept() ;

    void reject() {
        VariableText->clear();
        LongNameText->clear();
        UnitsText->clear();
        Calib1Text->clear();
        Calib2Text->clear();
        Calib3Text->clear();
        Calib4Text->clear();
        Calib5Text->clear();
        Calib6Text->clear();
        this->hide();
        }

    // Show the dialog.  Note: it's important that if the dialog is being
    // select indexes and if the dialog is being used for adding a new
    // variable, that indexList is empty.
    void show(NidasModel* model, QModelIndexList indexList);

public:

    VariableComboDialog(QWidget * parent = 0);

    ~VariableComboDialog() {}

    void setDocument(Document * document) {_document = document;}

protected:

    QMessageBox * _errorMessage;
    Document * _document;

private: 
    VariableItem *_varItem;
    QModelIndexList _indexList;
    NidasModel* _model;
    int _origSRBoxIndex;
};

}

#endif
