/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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
/*

  The main window of the nidas (for aircraft) Configuration Editor.
  This is the controller in regards to Qt and sets up windows,
        handles signals/slots, etc
*/

#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <QMainWindow>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>

#include <xercesc/util/PlatformUtils.hpp>

#include "Document.h"
#include "AddSensorComboDialog.h"
#include "AddDSMComboDialog.h"
#include "AddA2DVariableComboDialog.h"
#include "VariableComboDialog.h"
#include "NewProjectDialog.h"
#include "exceptions/UserFriendlyExceptionHandler.h"

#include "nidas_qmv/NidasModel.h"
#include "nidas_qmv/SiteItem.h"
#include <QTreeView>
#include <QTableView>
#include <QSplitter>

#include <iostream>
#include <fstream>
#include <string>

using namespace nidas::core;
namespace n_u = nidas::util;
using namespace config;
using namespace xercesc;


class QAction;
class QActionGroup;
class QLabel;
class QMenu;

class ConfigWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConfigWindow();

    ~ConfigWindow() {
        XMLPlatformUtils::Terminate();
    };
    // Refactor in such a way that Document doesn't need these
    // or they're in document or we don't need Document or something.
    NidasModel *getModel() const { cerr << "model pointer =" << model << "\n" ;return model; } // XXX
    QTableView *getTableView() const { return tableview; } // XXX
    
    void show();

    virtual void closeEvent(QCloseEvent *event) 
        { quit(); QWidget::closeEvent(event); }

public slots:
    void newFile();
    void openFile();
    void newProj();
    void saveOldFile();
    bool saveFile(std::string origFile);
    bool saveAsFile();
    void editProjName();
    void toggleErrorsWindow(bool);
    void addSensorCombo();
    void editSensorCombo();
    void deleteSensor();
    void addDSMCombo();
    void editDSMCombo();
    void deleteDSM();
    void editA2DVariableCombo();
    void addA2DVariableCombo();
    void editVariableCombo();
    void deleteA2DVariable();
    void quit();
    void changeToIndex(const QModelIndex&);
    void changeToIndex(const QItemSelection&);
    void setFilename(QString filename) { _filename = filename; return; }
    void writeProjectName(QString projName);

private:
    void buildMenus();
    void buildFileMenu();
    void buildWindowMenu();
    void buildAddMenu();
    void buildSensorCatalog();
    void buildA2DVarDB();
    void buildSensorMenu();
    void buildSensorActions();
    void buildDSMMenu();
    void buildDSMActions();
    void buildVariableMenu();
    void buildVariableActions();
    void buildA2DVariableMenu();
    void buildA2DVariableActions();
    void buildProjectMenu();

    UserFriendlyExceptionHandler * exceptionHandler;
    AddSensorComboDialog *sensorComboDialog;
    AddDSMComboDialog *dsmComboDialog;
    AddA2DVariableComboDialog *a2dVariableComboDialog;
    VariableComboDialog *variableComboDialog;
    NewProjectDialog *newProjDialog;
    QMessageBox * _errorMessage;

    Document* _doc;

    bool doCalibrations;

    void getFile();
    void setupDefaultDir();
    std::string _defaultDir;
    bool _noProjDir;
    QString _projDir;
    QString _defaultCaption;
    const QString _gvDefault;
    const QString _c130Default;
    const QString _a2dCalDir;
    const QString _engCalDirRoot;
    const QString _pmsSpecsFile;
    bool fileExists(QString filename);
    QString _filename;
    bool _fileOpen;
    bool saveFileCopy(std::string origFile);
    bool askSaveFileAndContinue();

    void setupModelView(QSplitter *splitter);
    NidasModel *model;
    QTreeView *treeview;
    QTableView *tableview;
    QSplitter *mainSplitter;

    QMenu   *sensorMenu;
    QAction *addSensorAction;
    QAction *editSensorAction;
    QAction *deleteSensorAction;
    QMenu   *dsmMenu;
    QAction *addDSMAction;
    QAction *editDSMAction;
    QAction *deleteDSMAction;
    QMenu   *sampleMenu;
    QMenu   *variableMenu;
    QAction *editVariableAction;
    QMenu   *a2dVariableMenu;
    QAction *editA2DVariableAction;
    QAction *addA2DVariableAction;
    QAction *deleteA2DVariableAction;

};
#endif

