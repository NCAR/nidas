/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
