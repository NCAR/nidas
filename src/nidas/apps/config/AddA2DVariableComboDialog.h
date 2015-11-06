/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
#ifndef _config_AddA2DVariableComboDialog_h
#define _config_AddA2DVariableComboDialog_h

#include "ui_AddA2DVariableComboDialog.h"
#include <iostream>
#include <QMessageBox>
#include "Document.h"
#include "nidas_qmv/NidasModel.h"
#include "nidas_qmv/A2DVariableItem.h"
#include <raf/vardb.hh> // New Variable Database

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

    AddA2DVariableComboDialog(QWidget *parent); 

    ~AddA2DVariableComboDialog() {}

    void setDocument(Document* document) { _document = document;}

protected:

    Document * _document;

private: 
    QModelIndexList _indexList;
    NidasModel* _model;
    bool _addMode;
    int _origSRBoxIndex;
    VDBFile * _vardb;
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
    void setCalLabels();
    void clearCalLabels();
};

}

#endif
