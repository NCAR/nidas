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
#include <list>

#include "configwindow.h"
#include "exceptions/exceptions.h"
#include "exceptions/QtExceptionHandler.h"
#include "exceptions/CuteLoggingExceptionHandler.h"
#include "exceptions/CuteLoggingStreamHandler.h"

using namespace nidas::core;
using namespace nidas::util;


ConfigWindow::ConfigWindow()
{
try {
    //if (!(exceptionHandler = new QtExceptionHandler()))
    //if (!(exceptionHandler = new CuteLoggingExceptionHandler(this)))
    //if (!(exceptionHandler = new CuteLoggingStreamHandler(std::cerr,0)))
     //   throw 0;
    buildMenus();
    sensorComboDialog = new AddSensorComboDialog(this);
} catch (...) {
    InitializationException e("Initialization of the Configuration Viewer failed");
    throw e;
}
}



void ConfigWindow::buildMenus()
{
buildFileMenu();
buildWindowMenu();
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
    buildSensorActions();

    QMenu * menu = menuBar()->addMenu(tr("&Sensor"));
    menu->addAction(addSensorAction);
    menu->addAction(deleteSensorAction);
}



void ConfigWindow::buildSensorActions()
{
    addSensorAction = new QAction(tr("&Add Sensor"), this);
    connect(addSensorAction, SIGNAL(triggered()), this, SLOT(addSensorCombo()));
    addSensorAction->setEnabled(false);

    deleteSensorAction = new QAction(tr("&Delete Sensor"), this);
    connect(deleteSensorAction, SIGNAL(triggered()), this, SLOT(deleteSensor()));
    deleteSensorAction->setEnabled(false);
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
doc->deleteSensor(tableview->selectionModel()->selectedIndexes());
}



QString ConfigWindow::getFile()
{

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

                    /* qt docs say call setCentralWidget() only once,
                       but we need to dump the old one to make for a new file
                     */
                setCentralWidget(0);
                show();
                }

            mainSplitter = new QSplitter(this);
            mainSplitter->setObjectName(QString("the horizontal splitter!!!"));

            buildSensorCatalog();
            setupModelView(mainSplitter);

            setCentralWidget(mainSplitter);

            show(); // XXX

            _winTitle.append(filename);
            setWindowTitle(_winTitle);  

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

    //resize(1000, 600);

        // jja dev screen
    resize(725, 400);

        // jja dev screen
    QList<int> sizes = mainSplitter->sizes();
    sizes[0] = 275;
    mainSplitter->setSizes(sizes);

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



void ConfigWindow::setupModelView(QSplitter *splitter)
{
model = new NidasModel(Project::getInstance(), doc->getDomDocument(), this);

treeview = new QTreeView(splitter);
treeview->setModel(model);
treeview->header()->hide();

tableview = new QTableView(splitter);
tableview->setModel( model );
tableview->setSelectionModel( treeview->selectionModel() );  /* common selection model */
tableview->setSelectionBehavior( QAbstractItemView::SelectRows );
tableview->setSelectionMode( QAbstractItemView::SingleSelection );

//connect(treeview, SIGNAL(pressed(const QModelIndex &)), this, SLOT(changeToIndex(const QModelIndex &)));
connect(treeview->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(changeToIndex(const QItemSelection &)));

treeview->setCurrentIndex(treeview->rootIndex().child(0,0));

splitter->addWidget(treeview);
splitter->addWidget(tableview);
}


/*!
 * \brief Display and setup the correct actions for the index in \a selections.
 *
 * This version is for selectionModel's selectionChanged signal.
 * We use only the first element of \a selections,
 * expecting that the view(s) allow only one selection at a time.
 */
void ConfigWindow::changeToIndex(const QItemSelection & selections)
{
QModelIndexList il = selections.indexes();
if (il.size())
 changeToIndex(il.at(0));
else throw InternalProcessingException("selectionChanged signal provided no selections");
}


/*!
 * \brief Display and setup the correct actions for the current \a index.
 *
 * Set the table view's root index to the parent of \a index
 * and tell the model same so it returns correct headerData.
 * En/dis-able appropriate actions, e.g. add or delete choices...
 */
void ConfigWindow::changeToIndex(const QModelIndex & index)
{
tableview->setRootIndex(index.parent());
tableview->scrollTo(index);

model->setCurrentRootIndex(index.parent());

NidasItem *parentItem = model->getItem(index.parent());

//parentItem->setupyouractions(ahelper);
  //ahelper->addSensor(true);

if (dynamic_cast<DSMItem*>(parentItem)) {
    addSensorAction->setEnabled(true);
    deleteSensorAction->setEnabled(true);
    }
else {
    addSensorAction->setEnabled(false);
    deleteSensorAction->setEnabled(false);
    }

// fiddle with context-dependent menus/toolbars
/*
XXX
ask model: what are you
(dis)able menu/toolbars

NidasItem *item = model->data(index)
qt::install(item->menus());
*/
}



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



