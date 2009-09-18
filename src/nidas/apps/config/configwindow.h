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

using namespace nidas::core;
namespace n_u = nidas::util;


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
    int buildProjectWidget(Document*);
    void buildMenus();

public slots:
    QString getFile();
    QString saveFile();
    QString saveAsFile();

protected:
    void reset();

private:
    QTabWidget *SiteTabs;
    void sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable);
    void parseAnalog(const DSMConfig * dsm, DSMTableWidget * DSMTable);
    void parseOther(const DSMConfig * dsm, DSMTableWidget * DSMTable);

    const int numA2DChannels;

    Document* doc;

    bool doCalibrations;

};
#endif

