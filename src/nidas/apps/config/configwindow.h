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


using namespace nidas::core;
namespace n_u = nidas::util;
using namespace config;


#include "dsmtablewidget.h"

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
    
public slots:
    QString getFile();
    QString saveFile();
    QString saveAsFile();
    void toggleSensorCatalog(bool);
    void toggleErrorsWindow(bool);
    void addSensor();
    void addSensorCombo();
    void rebuildProjectFromDocument();

protected:
    void reset();

private:
    void buildMenus();
    void buildFileMenu();
    void buildWindowMenu();
    void buildAddMenu();
    QWidget * buildSensorCatalog();
    QWidget * buildSiteTabs();
    QWidget * buildProjectWidget();
    QWidget * NEWbuildProjectWidget();

    UserFriendlyExceptionHandler * exceptionHandler;
    SensorCatalogWidget * _sensorCat;
    QTabWidget * SiteTabs;
    AddSensorDialog *sensorDialog;
    AddSensorComboDialog *sensorComboDialog;

    void sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable);
    void parseAnalog(const DSMConfig * dsm, DSMTableWidget * DSMTable);
    void parseOther(const DSMConfig * dsm, DSMTableWidget * DSMTable);

    const int numA2DChannels;

    Document* doc;

    bool doCalibrations;

};
#endif

