/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
#include "SensorCatalogWidget.h"
#include "AddSensorDialog.h"
#include "AddSensorComboDialog.h"
#include "exceptions/UserFriendlyExceptionHandler.h"

#include "nidas_qmv/NidasModel.h"
#include <QTreeView>


using namespace nidas::core;
namespace n_u = nidas::util;
using namespace config;


#include "DSMTableWidget.h"
#include "DSMDisplayWidget.h"

class QAction;
class QActionGroup;
class QLabel;
class QMenu;

class ConfigWindow : public QMainWindow
{
    Q_OBJECT

public:
    ConfigWindow();
    unsigned int getCurrentDSMId();
    DSMDisplayWidget * getCurrentDSMWidget();
    void rebuildProjectFromDocument();
    void parseAnalogSingleSensor(DSMSensor *sensor, DSMTableWidget * DSMTable);
    void parseOtherSingleSensor(DSMSensor *sensor, DSMTableWidget * DSMTable);
    
public slots:
    QString getFile();
    QString saveFile();
    QString saveAsFile();
    void toggleErrorsWindow(bool);
    void addSensorCombo();
    void deleteSensor();
    void quit();

protected:
    void reset();

private:
    void buildMenus();
    void buildFileMenu();
    void buildWindowMenu();
    void buildAddMenu();
    void buildSensorCatalog();
    void buildSensorMenu();
    QWidget * buildSiteTabs();
    QWidget * buildProjectWidget();
    QWidget * NEWbuildProjectWidget();

    UserFriendlyExceptionHandler * exceptionHandler;
    QTabWidget * SiteTabs;
    AddSensorComboDialog *sensorComboDialog;
    void parseAnalogSensors(const DSMConfig * dsm, DSMTableWidget * DSMTable);
    void parseOtherSensors(const DSMConfig * dsm, DSMTableWidget * DSMTable);

    void sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable);

    const int numA2DChannels;

    Document* doc;

    bool doCalibrations;

    void buildNewStuff(QWidget*);
    NidasModel *model;
    QTreeView *treeview;
    QTableView *tableview;

    static const QString viewName;

};
#endif

