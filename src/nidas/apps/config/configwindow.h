/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

  The main window of the nidas (for aircraft) Configuration Editor.
  This is the controller in regards to Qt and sets up windows,
        handles signals/slots, etc
 ********************************************************************
*/

#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <QMainWindow>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/PortSelectorTest.h>
#include <nidas/core/DSMConfig.h>

#include "Document.h"
#include "AddSensorComboDialog.h"
#include "AddDSMComboDialog.h"
#include "exceptions/UserFriendlyExceptionHandler.h"

#include "nidas_qmv/NidasModel.h"
#include "nidas_qmv/SiteItem.h"
#include <QTreeView>
#include <QTableView>
#include <QSplitter>


using namespace nidas::core;
namespace n_u = nidas::util;
using namespace config;


class QAction;
class QActionGroup;
class QLabel;
class QMenu;

class ConfigWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConfigWindow();

    // Refactor in such a way that Document doesn't need these
    // or they're in document or we don't need Document or something.
    NidasModel *getModel() const { return model; } // XXX
    QTableView *getTableView() const { return tableview; } // XXX
    
public slots:
    QString getFile();
    QString saveFile();
    QString saveAsFile();
    void toggleErrorsWindow(bool);
    void addSensorCombo();
    void deleteSensor();
    void addDSMCombo();
    void deleteDSM();
    void quit();
    void changeToIndex(const QModelIndex&);
    void changeToIndex(const QItemSelection&);

private:
    void buildMenus();
    void buildFileMenu();
    void buildWindowMenu();
    void buildAddMenu();
    void buildSensorCatalog();
    void buildSensorMenu();
    void buildSensorActions();
    void buildDSMMenu();
    void buildDSMActions();

    UserFriendlyExceptionHandler * exceptionHandler;
    AddSensorComboDialog *sensorComboDialog;
    AddDSMComboDialog *dsmComboDialog;

    Document* doc;

    bool doCalibrations;

    void setupModelView(QSplitter *splitter);
    NidasModel *model;
    QTreeView *treeview;
    QTableView *tableview;
    QSplitter *mainSplitter;


    QAction *addSensorAction;
    QAction *deleteSensorAction;
    QAction *addDSMAction;
    QAction *deleteDSMAction;

};
#endif

