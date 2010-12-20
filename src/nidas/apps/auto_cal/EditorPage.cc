#include <iostream>
#include "EditorPage.h"
#include "ComboBoxDelegate.h"

#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QMessageBox>

using namespace std;

const QString EditorPage::DB_DRIVER = "QPSQL7";                                                                        
const QString EditorPage::CALIB_DB_NAME = "calibrations";

//#include "logx/Logging.h"

//LOGGING("nidas.auto_cal.EditorPage");

EditorPage::EditorPage(QWidget* parent)
    : QWizardPage(parent)
{
//DLOG << "ctor";

  setTitle(tr("Calibration Database Editor"));
  setSubTitle(tr("This page provides a table for editing the calibration Database."));
  setFinalPage(true);

//connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
//connect(buttonRefresh, SIGNAL(clicked()), this, SLOT(refresh()));
}

/* -------------------------------------------------------------------- */

void EditorPage::createDatabaseConnection()
{
//DLOG << "createDatabaseConnection";

  // create the default database connection
  _calibDB = QSqlDatabase::addDatabase(DB_DRIVER);

  if (!_calibDB.isValid())
  {
//  ELOG << "Failed to connect to PostGreSQL driver.";
    QMessageBox::critical(0,
	tr("AEROS."), tr("Failed to connect Calibration DataBase."));
    return;
  }
  _calibDB.setHostName("localhost");
  _calibDB.setUserName("ads");
  _calibDB.setDatabaseName(CALIB_DB_NAME);
}

/* -------------------------------------------------------------------- */

bool EditorPage::openDatabase()
{
//DLOG << "openDatabase";

  if (!_calibDB.isValid())
  {
//  ELOG << "Shouldn't get here, no DB connection.";
    return false;
  }

  if (!_calibDB.open())
  {
//  ELOG << "Failed to open calibrations database: " <<
//    _calibDB.lastError().driverText().toAscii().data() ;
//  ELOG << _calibDB.lastError().databaseText().toAscii().data();

    QMessageBox::critical(0,
	tr("AEROS."), tr("Failed to open Calibration DataBase."));

    return false;
  }
  return true;
}

/* -------------------------------------------------------------------- */

void EditorPage::closeDatabase()
{
//DLOG << "closeDatabase";

  _calibDB.close();
}

/* -------------------------------------------------------------------- */

void EditorPage::initializePage()
{
  cout << "EditorPage::initializePage" << endl;
  createDatabaseConnection();
  openDatabase();

  _calibTable = new QTableView(this);
  _calibTable->setObjectName(QString::fromUtf8("_calibTable"));

  _model = new QSqlTableModel(this);
  _model->setTable("calibrations");
  _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
  _model->select();

  _model->setHeaderData( 0, Qt::Horizontal, tr("Platform"));
  _model->setHeaderData( 1, Qt::Horizontal, tr("Project"));
  _model->setHeaderData( 2, Qt::Horizontal, tr("Username"));
  _model->setHeaderData( 3, Qt::Horizontal, tr("Sensor Type"));
  _model->setHeaderData( 4, Qt::Horizontal, tr("Serial #"));
  _model->setHeaderData( 5, Qt::Horizontal, tr("Variable"));
  _model->setHeaderData( 6, Qt::Horizontal, tr("DSM"));
  _model->setHeaderData( 7, Qt::Horizontal, tr("Cal Type"));
  _model->setHeaderData( 8, Qt::Horizontal, tr("Analog Ch"));
  _model->setHeaderData( 9, Qt::Horizontal, tr("Gain"));
  _model->setHeaderData(10, Qt::Horizontal, tr("Set Points"));
  _model->setHeaderData(11, Qt::Horizontal, tr("Avg Values"));
  _model->setHeaderData(12, Qt::Horizontal, tr("StdDev Values"));
  _model->setHeaderData(13, Qt::Horizontal, tr("Calibration"));
  _model->setHeaderData(14, Qt::Horizontal, tr("Temperature"));
  _model->setHeaderData(15, Qt::Horizontal, tr("Comment"));
  _model->setHeaderData(16, Qt::Horizontal, tr("Date"));

  _calibTable->setModel(_model);

//      ("CREATE TABLE calibrations (site char(16), project_name char(32), username char(32), sensor_type char(20), serial_number char(20), var_name char(20), dsm_name char(16), cal_type char(16), analog_addr int, gain int, set_points float[], avg_volts float[], stddev_volts float[], cal float[], temperature float, comment char(256), cal_date timestamp, UNIQUE (site, cal_date) )");

  QSqlDatabase database = _model->database();
  delegate["site"]          = new ComboBoxDelegate(database, "site");
  delegate["project_name"]  = new ComboBoxDelegate(database, "project_name");
  delegate["username"]      = new ComboBoxDelegate(database, "username");
  delegate["sensor_type"]   = new ComboBoxDelegate(database, "sensor_type");
  delegate["serial_number"] = new ComboBoxDelegate(database, "serial_number");
  delegate["var_name"]      = new ComboBoxDelegate(database, "var_name");
  delegate["dsm_name"]      = new ComboBoxDelegate(database, "dsm_name");
  delegate["cal_type"]      = new ComboBoxDelegate(database, "cal_type");
  delegate["analog_addr"]   = new ComboBoxDelegate(database, "analog_addr");
  delegate["gain"]          = new ComboBoxDelegate(database, "gain");

  _calibTable->setItemDelegateForColumn(0, delegate["site"]);
  _calibTable->setItemDelegateForColumn(1, delegate["project_name"]);
  _calibTable->setItemDelegateForColumn(2, delegate["username"]);
  _calibTable->setItemDelegateForColumn(3, delegate["sensor_type"]);
  _calibTable->setItemDelegateForColumn(4, delegate["serial_number"]);
  _calibTable->setItemDelegateForColumn(5, delegate["var_name"]);
  _calibTable->setItemDelegateForColumn(6, delegate["dsm_name"]);
  _calibTable->setItemDelegateForColumn(7, delegate["cal_type"]);
  _calibTable->setItemDelegateForColumn(8, delegate["analog_addr"]);
  _calibTable->setItemDelegateForColumn(9, delegate["gain"]);

  _calibTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
  _calibTable->verticalHeader()->setResizeMode(QHeaderView::Interactive);
  _calibTable->horizontalHeader()->setStretchLastSection( true );

//for (int i=0; i<_model->columnCount(); i++)
//  _calibTable->resizeColumnToContents(i);

  _calibTable->adjustSize();
  _calibTable->show();
  
}

/* -------------------------------------------------------------------- */

void EditorPage::setVisible(bool visible)
{   
    QWizardPage::setVisible(visible);
}   

/* -------------------------------------------------------------------- */

void EditorPage::refresh()
{
  _calibTable->update();
}

/* -------------------------------------------------------------------- */

EditorPage::~EditorPage()
{
  delete _model;
  closeDatabase();
}
