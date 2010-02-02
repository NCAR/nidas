/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision: $

    $LastChangedBy:  $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/config/configwindow.cc $
 ********************************************************************
*/

#include <QtGui>
#include <ctime>
//#include <QFileDialog>
//#include <QMenu>
//#include <QAction>

#include "configwindow.h"
#include "exceptions/exceptions.h"
#include "exceptions/QtExceptionHandler.h"
#include "exceptions/CuteLoggingExceptionHandler.h"
#include "exceptions/CuteLoggingStreamHandler.h"

#include <list>

using namespace nidas::core;
using namespace nidas::util;

const QString ConfigWindow::viewName( "Nidas View, original ConfigWindow version" );


ConfigWindow::ConfigWindow() : numA2DChannels(8)
{
try {
    reset();
    //if (!(exceptionHandler = new QtExceptionHandler()))
    //if (!(exceptionHandler = new CuteLoggingExceptionHandler(this)))
    if (!(exceptionHandler = new CuteLoggingStreamHandler(std::cerr,0)))
        throw 0;
    buildMenus();
    sensorComboDialog = new AddSensorComboDialog(this);
} catch (...) {
    InitializationException e("Initialization of the Configuration Viewer failed");
    throw e;
}
}



void ConfigWindow::reset()
{
doCalibrations=true;
}



void ConfigWindow::buildMenus()
{
buildFileMenu();
buildWindowMenu();
//buildAddMenu();
buildSensorMenu();
}



void ConfigWindow::buildFileMenu()
{
    QAction * openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setStatusTip(tr("Open a new configuration file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(getFile()));

    QAction * saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    saveAct->setStatusTip(tr("Save a configuration file"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));

    QAction * saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(tr("Ctrl+A"));
    saveAsAct->setStatusTip(tr("Save configuration as a new file"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAsFile()));

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(quit()));

    QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addAction(exitAct);
}


void ConfigWindow::quit()
{
QCoreApplication::quit();
}

void ConfigWindow::buildWindowMenu()
{
    QMenu * menu = menuBar()->addMenu(tr("&Windows"));
    QAction * act;

    act = new QAction(tr("&Errors"), this);
    act->setStatusTip(tr("Toggle errors window"));
    act->setCheckable(true);
    act->setChecked(false);
    connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleErrorsWindow(bool)));
    menu->addAction(act);

}

void ConfigWindow::buildSensorMenu()
{
    QMenu * menu = menuBar()->addMenu(tr("&Sensor"));
    QAction * act;

    act = new QAction(tr("&Add Sensor"), this);
    connect(act, SIGNAL(triggered()), this, SLOT(addSensorCombo()));
    menu->addAction(act);
    act = new QAction(tr("&Delete Sensor"), this);
    connect(act, SIGNAL(triggered()), this, SLOT(deleteSensor()));
    menu->addAction(act);
}

void ConfigWindow::buildAddMenu()
{
    QMenu * menu = menuBar()->addMenu(tr("&Add"));
    QAction * act;

    act = new QAction(tr("Sensor&Combo"), this);
    connect(act, SIGNAL(triggered()), this, SLOT(addSensorCombo()));
    menu->addAction(act);

}



void ConfigWindow::toggleErrorsWindow(bool checked)
{
exceptionHandler->setVisible(checked);
}



void ConfigWindow::addSensorCombo()
{
sensorComboDialog->show();
}

void ConfigWindow::deleteSensor()
{
  cerr << "deleteSensor called" << endl;
  doc->deleteSensor();
}


QString ConfigWindow::getFile()
{
reset();

    QString filename;
    std::string _dir("/"), _project;
    char * _tmpStr;
    QString _caption;
    QString _winTitle("Configview:  ");

    _tmpStr = getenv("PROJ_DIR");
    if (_tmpStr)
       _dir.append(_tmpStr);
    else
       _caption.append("No $PROJ_DIR. ");

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("PROJECT");
        if (_tmpStr)
        {
            _dir.append("/");
            _dir.append(_tmpStr);
            _project.append(_tmpStr);
        }
        else
            _caption.append("No $PROJECT.");
    }

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("AIRCRAFT");
        if (_tmpStr)
        {
            _dir.append("/");
            _dir.append(_tmpStr);
            _dir.append("/nidas");
        }
        else
            _caption.append(" No $AIRCRAFT.");
    }

    _caption.append(" Choose a file...");

    filename = QFileDialog::getOpenFileName(
                0,
                _caption,
                QString::fromStdString(_dir),
                "Config Files (*.xml)");

    if (filename.isNull() || filename.isEmpty()) {
        cerr << "filename null/empty ; not opening" << endl;
        _winTitle.append("(no file selected)");
        setWindowTitle(_winTitle);
        }
    else {
        doc = new Document(this);
        doc->setFilename(filename.toStdString());
      try {
        doc->parseFile();
        doc->printSiteNames();

            QWidget *oldCentral = centralWidget();
            if (oldCentral) {
                cerr << "got an old central widget\n";
                cerr << "NAME: " << oldCentral->objectName().toStdString() << "\n";
                cerr << "INFO::\n";
                oldCentral->dumpObjectInfo();
                cerr << "\n\nTREE:\n";
                oldCentral->dumpObjectTree();
                cerr << "\n\n";
                setCentralWidget(0);
                show();
                }

        if (QWidget *wid = buildProjectWidget()) {
            cerr << "NEW project widget\n";
            cerr << "NAME: " << wid->objectName().toStdString() << "\n";
            cerr << "INFO::\n";
            wid->dumpObjectInfo();
            cerr << "\n\nTREE:\n";
            wid->dumpObjectTree();
            cerr << "\n\n";

            buildNewStuff(0);
            QSplitter *hsplitter = new QSplitter(this);
            hsplitter->setObjectName(QString("the horizontal splitter!!!"));

            QSplitter *vsplitter = new QSplitter(Qt::Vertical);
            vsplitter->setObjectName(QString("the vertical splitter!!!"));

            hsplitter->addWidget(treeview);
            hsplitter->addWidget(tableview);

            vsplitter->addWidget(hsplitter);
            vsplitter->addWidget(wid);

            /*
            splitter->addWidget(treeview);
            treeview->setParent(splitter);
            */

            wid->setParent(vsplitter);

            setCentralWidget(vsplitter);

            //setCentralWidget(wid);
            show(); // XXX

            _winTitle.append(filename);
            setWindowTitle(_winTitle);  
            }
      }
      catch (const CancelProcessingException & cpe) {
        // stop processing, show blank window
        QStatusBar *sb = statusBar();
        if (sb) sb->showMessage(QString::fromAscii(cpe.what()));
      }
      catch(...) {
          exceptionHandler->handle("Project configuration file");
      }

      }

    resize(1000, 600);
    show();
    return filename;
}



QString ConfigWindow::saveFile()
{
    cerr << "saveFile called" << endl;
    doc->writeDocument();
    return(NULL);
}


QString ConfigWindow::saveAsFile()
{
    QString qfilename;
    QString _caption;

    qfilename = QFileDialog::getSaveFileName(
                0,
                _caption,
                doc->getDirectory(),
                "Config Files (*.xml)");

    cerr << "saveAs dialog returns " << qfilename.toStdString() << endl;

    if (qfilename.isNull() || qfilename.isEmpty()) {
        cerr << "qfilename null/empty ; not saving" << endl;
        return(NULL);
        }

    doc->setFilename(qfilename.toStdString().c_str());
    doc->writeDocument();
    return(NULL);
}



#include <time.h>

QWidget * ConfigWindow::buildProjectWidget()
{
    QWidget *widget = 0;
    if (!doc) return(0);

    buildSensorCatalog();
    widget = buildSiteTabs();
    return(widget);
}


void ConfigWindow::buildNewStuff(QWidget *parent)
{
model = new NidasModel(Project::getInstance(), this);

treeview = new QTreeView(parent);
treeview->setModel(model);
treeview->header()->hide();

tableview = new QTableView(parent);
tableview->setModel( model );
tableview->setSelectionModel( treeview->selectionModel() );  /* common selection model */
tableview->setSelectionBehavior( QAbstractItemView::SelectRows );
tableview->setSelectionMode( QAbstractItemView::SingleSelection );

//connect(treeview, SIGNAL(pressed(const QModelIndex &)), tableview, SLOT(setRootIndex(const QModelIndex &)));
connect(treeview, SIGNAL(pressed(const QModelIndex &)), this, SLOT(setRootIndex(const QModelIndex &)));

treeview->setCurrentIndex(treeview->rootIndex().child(0,0));
}

void ConfigWindow::setRootIndex(const QModelIndex & index)
{
//tableview->setHorizontalHeader( model->getHorizontalHeaderView(index) );

tableview->setRootIndex(index.parent());
tableview->scrollTo(index);

//treeview->columnCountChanged(model->columnCount(treeview->currentIndex()), model->columnCount(index));
//emit treeview->header()->sectionCountChanged(model->columnCount(treeview->currentIndex()), model->columnCount(index));
}


/*
MyQTableView::setRootIndex(QModelIndex & index)
{
h=model->getHeaderView(index);  // parentItem->getChildHeaderView()
setHeader(h);
super::setRootIndex(index);
}
*/

/*
QHeaderView::setRootIndex(QModelIndex & index)
{
for (i=0; i<model->columnCount(index); i++)
 QVariant v = model->headerData(i,Horizontal,Display,index);
 this->setItemDelegateForColumn(i,new delegate...);
}
*/

/*
QHeaderView::setRootIndex(QModelIndex & index)
{
 QList *cols = model->columnList(index)
 hv = tableview->horizontalHeader();
 hv->hide all columns
 foreach col in cols
   hv->showColumn(col);
}
*/


void ConfigWindow::buildSensorCatalog()
//  Construct the Sensor Catalog drop-down
{
Project *project = Project::getInstance();

    if(!project->getSensorCatalog()) {
        cerr<<"Configuration file doesn't contain a catalog!!"<<endl;
        return;
    }

    cerr<<"Putting together sensor Catalog"<<endl;
    map<string,xercesc::DOMElement*>::const_iterator mi;
    sensorComboDialog->SensorBox->clear();
    sensorComboDialog->SensorBox->addItem("Analog");
    for (mi = project->getSensorCatalog()->begin();
         mi != project->getSensorCatalog()->end(); mi++) {
        cerr<<"   - adding sensor:"<<(*mi).first<<endl;
        sensorComboDialog->SensorBox->addItem(QString::fromStdString(mi->first));
    }

    sensorComboDialog->setDocument(doc);
    return;
}



QWidget * ConfigWindow::buildSiteTabs()
{
    QString tmpStr;
    Project *project = Project::getInstance();

    QTabWidget * SiteTabs = new QTabWidget();
    SiteTabs->setObjectName(viewName);

    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));

    for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
        Site * site = si.next();

        QTabWidget *DSMTabs = new QTabWidget();

        for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {

            // XXX *** XXX
            // very bad casting of const to non-const to get a mutable pointer to our dsm
            // *** NEEDS TO BE FIXED either here or in nidas::core
            //
            DSMConfig * dsm = (DSMConfig*)(di.next());

            tmpStr.append("DSM: ");
            tmpStr.append(QString::fromStdString(dsm->getLocation()));
            tmpStr.append(", ["); tmpStr.append(QString::fromStdString(dsm->getName()));
            tmpStr.append("]");

            DSMDisplayWidget *dsmWidget = new DSMDisplayWidget(dsm,doc->getDomDocument(),tmpStr);

            parseOtherSensors(dsm, dsmWidget->getOtherTable());
            parseAnalogSensors(dsm, dsmWidget->getAnalogTable());

            DSMTabs->addTab(dsmWidget, QString::fromStdString(dsm->getLocation()));
            tmpStr.clear();

        }

        std::string siteTabLabel = project->getName();
        if (project->getSystemName() != site->getName()) siteTabLabel += "/" + project->getSystemName();
        siteTabLabel += ": " + site->getName();
        SiteTabs->addTab(DSMTabs, QString::fromStdString(siteTabLabel));

        // project version is reqd in xml, maybe put that in tooltip?
    }

    return (SiteTabs);
}

unsigned int ConfigWindow::getCurrentDSMId()
{
   DSMDisplayWidget *dsmWidget = this->getCurrentDSMWidget();
   unsigned int dsmId = dsmWidget->getDSMId();
   return dsmId;
}

DSMDisplayWidget * ConfigWindow::getCurrentDSMWidget()
{
   cerr << "ConfigWindow::getCurrentDSMWidget()\n";

   QWidget *cWid = centralWidget();
   cerr << " centralWidget is a " << cWid->metaObject()->className() << " named " << cWid->objectName().toStdString() << "\n";
   cerr << " looking for " << viewName.toStdString() << "\n";

   QTabWidget *siteTabs = cWid->findChild<QTabWidget*>( viewName );
   QTabWidget *siteTab = dynamic_cast <QTabWidget*>(siteTabs->currentWidget());

   QSplitter *splitter;

   if (siteTab == 0) {
     cerr << " siteTab==0 trying direct access\n";
     splitter = dynamic_cast <QSplitter*>(cWid);
    if (splitter) {
     cerr << " cWid is cast to QSplitter \n";
     cerr << " has " << splitter->count() << " widgets\n";
     for (int i=0; i<splitter->count(); i++)
        cerr << "  widget " << i << " isa " << splitter->widget(i)->metaObject()->className()
             << " named " << splitter->widget(i)->objectName().toStdString() << "\n";
     siteTab = dynamic_cast <QTabWidget*> (splitter->widget(1));
    }
   }

   if (siteTab == NULL) return 0;
    cerr << " siteTab isa " << siteTab->metaObject()->className()
         << " named " << siteTab->objectName().toStdString() << "\n";

   //return ( dynamic_cast <DSMDisplayWidget*> (siteTab->currentWidget()) );

   QWidget * currwid = siteTab->currentWidget();
   cerr << " currentWidget isa " << currwid->metaObject()->className() << " named " << currwid->objectName().toStdString() << "\n";

   DSMDisplayWidget * ddw = dynamic_cast <DSMDisplayWidget*> (currwid);
   if (ddw == 0) cerr << " DSMDisplayWidget == 0\n";

   return(ddw);

   //QTabWidget* cWid = dynamic_cast <QTabWidget*> (centralWidget());
   //if (cWid == NULL) return 0;
   //QTabWidget* siteTab = dynamic_cast <QTabWidget*> (cWid->currentWidget());
   //if (siteTab == NULL) return 0;
   //return ( dynamic_cast <DSMDisplayWidget*> (siteTab->currentWidget()) );

}



void ConfigWindow::sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable)
{
    DSMTable->addRow();
    if (sensor->getCatalogName().length() > 0) {
        DSMTable->setName(sensor->getCatalogName()+sensor->getSuffix());
    }
    else
    {
        DSMTable->setName(sensor->getClassName()+sensor->getSuffix());
    }

    DSMTable->setDevice(sensor->getDeviceName());

    const Parameter * parm = sensor->getParameter("SerialNumber");
    if (parm) {
        DSMTable->setSerialNumber(parm->getStringValue(0));
    }

    CalFile *cf = sensor->getCalFile();
    if (cf) {
        string A2D_SN(cf->getFile());
        A2D_SN = A2D_SN.substr(0,A2D_SN.find(".dat"));
        DSMTable->setSerialNumber(A2D_SN);
    }

    DSMTable->setNidasId(sensor->getSensorId());
}


void ConfigWindow::parseAnalogSensors(const DSMConfig * dsm, DSMTableWidget * DSMTable)
{
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        parseAnalogSingleSensor(si2.next(),DSMTable);
        }
}

void ConfigWindow::parseOtherSensors(const DSMConfig * dsm, DSMTableWidget * DSMTable)
{   
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        parseOtherSingleSensor(si2.next(),DSMTable);
        }
}


void ConfigWindow::parseAnalogSingleSensor(DSMSensor * sensor, DSMTableWidget * DSMTable)
{
if (!sensor) return;
if (!DSMTable) return;

    int gain=0, bipolar=0, channel=0;

        if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
           return;

        sensorTitle(sensor, DSMTable);

        CalFile *cf = 0;
        if (doCalibrations) {
         cf = sensor->getCalFile();
         };
        try {
            if (cf) cf->open();
            }
        catch(const n_u::IOException& e) {
            cerr << e.what() << endl;
            exceptionHandler->display("Error parsing calibration file", e.what());
#if 0
            int button =
                QMessageBox::information( 0, "Error parsing calibration file",
                    QString::fromAscii(e.what())+
                    QString::fromAscii("\nCancel processing, continue without this calibration file, or skip all cal files?"),
                    "Cancel", "Continue", "Skip all", 1, 2 );
            if (button == 0) {
                CancelProcessingException cpe(e);
                throw(cpe);
                }
            cf=0; // button 1 or 2
            if (button == 2) doCalibrations = false;
#endif
            }

        const Parameter * parm;
        QString varStr;
        int tagNum = 0;
        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag * tag = ti.next();
            if (!tag->isProcessed()) continue;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable * var = vi.next();
                tagNum++;
                if (tagNum > 1) DSMTable->addRow();

                DSMTable->setSampRate(tag->getRate());
                varStr.append("S");
                varStr.append(QString::number(tag->getSampleId()));
                varStr.append(":");
                varStr.append(QString::fromStdString(var->getName()));
                DSMTable->setAnalogVariable(varStr);
                varStr.clear();

                channel = var->getA2dChannel();
                DSMTable->setAnalogChannel(channel);

                parm = var->getParameter("gain");
                if (parm) {
                    gain = (int) parm->getNumericValue(0);
                    DSMTable->setGain(gain);
                }

                parm = var->getParameter("bipolar");
                if (parm) {
                    bipolar = (int) parm->getNumericValue(0);
                    DSMTable->setBiPolar(bipolar);
                }

                parm = var->getParameter("linear");
                if (parm) {
                    std::string tmpStr = parm->getStringValue(0);
                    cerr<<"Found a linear cal: "<< tmpStr <<endl;
                }

                parm = var->getParameter("poly");
                if (parm) {
                    std::string tmpStr = parm->getStringValue(0);
                    cerr<<"Found a poly cal: "<< tmpStr <<endl;
                }

 
                parm = var->getParameter("corIntercept");
                QString tmpStr;
                //cerr.width(12); cerr.precision(6);
                if (parm) {
                    // A2D cals are in "old school" form rather than cal file
                    //cerr << right << parm->getNumericValue(0);
                    tmpStr.append("(");
                    tmpStr.append(QString::number(parm->getNumericValue(0)));
                    parm = var->getParameter("corSlope");
                    //cerr.width(10); cerr.precision(6);
                    if (parm) {
                        //cerr << right << parm->getNumericValue(0);
                        tmpStr.append(", ");
                        tmpStr.append(QString::number(parm->getNumericValue(0)));
                        tmpStr.append(")");
                        DSMTable->setA2DCal(tmpStr);
                        tmpStr.clear();
                    }  // TODO: else alert user to fact that we only have half a cal
                }
                else if (cf) {
                  float slope = 1, intercept = 0;
                  try {
                        // i.e. cf->reset()
                        cf->close();
                        cf->open();

                        // time_t curTime = time(NULL);
                        dsm_time_t tnow = getSystemTime();
                        dsm_time_t calTime = 0;

//cerr<<"Working on calfile:"<<cf->getFile()<< "  channel:"<< channel<< "  tnow:"<<tnow<<endl;
                        while (tnow > calTime && channel >= 0) {
                            int nd = 2 + numA2DChannels  * 2;
                            float d[nd];
                                int n = cf->readData(d,nd);
                                calTime = cf->readTime().toUsecs();
//cerr<<" calTime:"<<calTime<<endl;
                                if (n < 2) { cerr<<"VAR="<<var->getName()<<"; "<<"ERR: only found 2 items on the line"<<endl;continue; }
                                int cgain = (int)d[0];
                                int cbipolar = (int)d[1];
//cerr<<"   cgain:"<<cgain<<" gain:"<<gain;
//cerr<<"   cbipolar:"<<cbipolar<<" bipolar:"<<bipolar<<" firstcal:"<<d[2]<<endl;
                                if ((cgain < 0 || gain == cgain) &&
                                    (cbipolar < 0 || bipolar == cbipolar))
                                {
                                    intercept = d[2+channel*2];
                                    slope = d[3+channel*2];
//cerr<<"  *** setting :(" <<intercept<<", " << slope << ")"<<endl;
                                }

                        }
 
                        QString calStr;
                        calStr.append("(" + QString::number(intercept) + ", " +
                             QString::number(slope) + ")");
                        DSMTable->setA2DCal(calStr);

                }
                catch(const n_u::EOFException& e) {
                    if (slope == 0) {
                        std::string msg( "No slope before End of config file" );
                        msg += e.what();
                        exceptionHandler->display(
                            "Analog calibrations",
                            msg
                            );
                    }
                  }
                catch(...) {
                    exceptionHandler->handle("Analog calibrations");
                }

              }
            }
        }
}


void ConfigWindow::parseOtherSingleSensor(DSMSensor * sensor, DSMTableWidget * DSMTable)
{
if (!sensor) return;
if (!DSMTable) return;

        if (sensor->getClassName().compare("raf.DSMAnalogSensor") == 0)
           return;

        sensorTitle(sensor, DSMTable);

        if (sensor->getCatalogName().compare("IRIG") == 0)
            return;

        if (sensor->getDeviceName().compare(0, 10, "/dev/arinc") == 0)
            return;

        //QStringList columnHeaders;
        //columnHeaders << "Samp#" << "Rate" << "Variables";

        int row=0, column=0;
        QString sampleIdStr;
        QString rateStr;
        int sampleNumber=0;

        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag* tag = ti.next();
            if (!tag->isProcessed()) continue;

            sampleNumber++;
            if (sampleNumber>1) DSMTable->addRow();

            DSMTable->setSampRate(tag->getRate());

            sampleIdStr = QString::number(tag->getSampleId());
            QTableWidgetItem *sampleWidgetItem =  new QTableWidgetItem(sampleIdStr);
            sampleWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());

            rateStr = QString::number(tag->getRate());
            QTableWidgetItem *rateWidgetItem = new QTableWidgetItem(rateStr);
            rateWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());

            QComboBox * variableComboBox = new QComboBox(this);
            // It would be nice to have the combo box always return to "Sample N" heading after a 
            // user has viewed variables in the box.  The following commented out code returns an 
            // error at runtime that setCurrentIndex(0) is not a valid SLOT.  
	    // connect(variableComboBox, SIGNAL(currentIndexChanged(int)), variableComboBox, 
            //         SLOT(setCurrentIndex(0)));
            variableComboBox->addItem(QString("Sample " + QString::number(tag->getSampleId())));
            QString varInfo;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable* var = vi.next();
                varInfo.append(QString::fromStdString(var->getName()));
                VariableConverter* varConv = var->getConverter();
                if (varConv) {
                    varInfo.append(QString::fromStdString(" - " + varConv->toString()));
                }
                variableComboBox->addItem(varInfo);
                varInfo.clear();
            }

            DSMTable->setOtherVariables(variableComboBox);

            sampleIdStr.clear();
            rateStr.clear();
            row++; column = 0;
        }
}


void ConfigWindow::rebuildProjectFromDocument() {
    try {
        if (QWidget *wid = buildProjectWidget()) {
            setCentralWidget(wid);
            //_winTitle.append(filename);
            //setWindowTitle(_winTitle);  
            }
      }
      catch (const CancelProcessingException & cpe) {
        // stop processing, show blank window
        QStatusBar *sb = statusBar();
        if (sb) sb->showMessage(QString::fromAscii(cpe.what()));
      }
      catch(...) {
          exceptionHandler->handle("Project configuration file");
      }
}
