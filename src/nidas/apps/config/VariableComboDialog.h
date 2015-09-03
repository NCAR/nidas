/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
    void setCalLabels();
    bool _xmlCals;
};

}

#endif
