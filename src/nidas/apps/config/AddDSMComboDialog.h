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
    void show(NidasModel* model,
              QModelIndexList indexList);
    bool setUpDialog();
    void existingDSM(DSMItem *dsmItem);

public:

    AddDSMComboDialog(QWidget * parent = 0);

    ~AddDSMComboDialog() {}

    void setDocument(Document * document) {_document = document;}
    void accept();


protected:

    QMessageBox * _errorMessage;
    Document * _document;

private:
    QModelIndexList _indexList;
    NidasModel* _model;

};

}

#endif
