#ifndef _config_AddA2DVariableComboDialog_h
#define _config_AddA2DVariableComboDialog_h

#include "ui_AddA2DVariableComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"
#include "nidas_qmv/NidasModel.h"
#include "nidas_qmv/A2DVariableItem.h"

namespace config
{

class AddA2DVariableComboDialog : public QDialog, public Ui_AddA2DVariableComboDialog
{
    Q_OBJECT

public slots:
    void accept() ;

    void reject() {
        //VariableText->clear();
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
    // used for editing an A2D variable, that the index list point to the 
    // select indexes and if the dialog is being used for adding a new
    // variable, that indexList is empty.
    void show(NidasModel* model, QModelIndexList indexList);
    //bool setUpDialog();
    void dialogSetup(const QString & variable);

    bool setup(std::string filename);

public:

    AddA2DVariableComboDialog(QWidget * parent = 0);

    ~AddA2DVariableComboDialog() {}

    void setDocument(Document * document) {_document = document;}

protected:

    Document * _document;

private: 
    QModelIndexList _indexList;
    NidasModel* _model;
    bool _addMode;
    int _origSRBoxIndex;
    int getVarDBIndex(const QString & varName);
    void SetUpChannelBox();
    void showVoltErr(int32_t vDBvLow, int32_t vDBvHi, int confIndx);
    void showSRErr(int vDBsr, int srIndx);
    QString removeSuffix(const QString & varName);
    QString getSuffix(const QString & varName);
    void checkUnitsAndCalCoefs();
    void clearForm();
    bool openVarDB(std::string filename);
    bool fileExists(QString filename);
    void buildA2DVarDB();
};

}

#endif
