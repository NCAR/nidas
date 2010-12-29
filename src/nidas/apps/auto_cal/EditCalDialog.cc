#include <iostream>

#include "EditCalDialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlError>

const QString EditCalDialog::DB_DRIVER = "QPSQL7";                                                                        
const QString EditCalDialog::CALIB_DB_HOST = "localhost";
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";

#include "logx/Logging.h"

LOGGING("nidas.auto_cal.EditCalDialog");

/* -------------------------------------------------------------------- */

EditCalDialog::EditCalDialog()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  setupUi(this);

  connect(buttonSync,    SIGNAL(clicked()), this, SLOT(syncButtonClicked()));
  connect(buttonSave,    SIGNAL(clicked()), this, SLOT(saveButtonClicked()));
  connect(buttonExport,  SIGNAL(clicked()), this, SLOT(exportButtonClicked()));
  connect(buttonRemove,  SIGNAL(clicked()), this, SLOT(removeButtonClicked()));
  connect(buttonClose,   SIGNAL(clicked()), this, SLOT(closeButtonClicked()));    // reject()

  createDatabaseConnection();
  openDatabase();

  _model = new QSqlTableModel;
  _model->setTable("calibrations");
  _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
  _model->select();

  _model->setHeaderData( 0, Qt::Horizontal, tr("Platform"));
  _model->setHeaderData( 1, Qt::Horizontal, tr("Project"));
  _model->setHeaderData( 2, Qt::Horizontal, tr("User"));
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

  _table->setModel(_model);

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

  _table->setItemDelegateForColumn(0, delegate["site"]);
  _table->setItemDelegateForColumn(1, delegate["project_name"]);
  _table->setItemDelegateForColumn(2, delegate["username"]);
  _table->setItemDelegateForColumn(3, delegate["sensor_type"]);
  _table->setItemDelegateForColumn(4, delegate["serial_number"]);
  _table->setItemDelegateForColumn(5, delegate["var_name"]);
  _table->setItemDelegateForColumn(6, delegate["dsm_name"]);
  _table->setItemDelegateForColumn(7, delegate["cal_type"]);
  _table->setItemDelegateForColumn(8, delegate["analog_addr"]);
  _table->setItemDelegateForColumn(9, delegate["gain"]);

  _table->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
  _table->verticalHeader()->setResizeMode(QHeaderView::Interactive);
  _table->horizontalHeader()->setStretchLastSection( true );

  for (int i=0; i<_model->columnCount(); i++)
    _table->resizeColumnToContents(i);

  _table->adjustSize();
  _table->show();
  
  std::cout << __PRETTY_FUNCTION__ << " EOF" << std::endl;
}

/* -------------------------------------------------------------------- */

EditCalDialog::~EditCalDialog()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  delete _model;
  closeDatabase();
  std::cout << __PRETTY_FUNCTION__ << " EOF" << std::endl;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::createDatabaseConnection()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  // create the default database connection
  _calibDB = QSqlDatabase::addDatabase(DB_DRIVER);

  if (!_calibDB.isValid())
  {
    ELOG << "Failed to connect to PostGreSQL driver.";
    QMessageBox::critical(0,
	tr("AUTO CAL."), tr("Failed to connect Calibration DataBase."));
    return;
  }
  _calibDB.setHostName(CALIB_DB_HOST);
  _calibDB.setUserName(CALIB_DB_USER);
  _calibDB.setDatabaseName(CALIB_DB_NAME);
  std::cout << __PRETTY_FUNCTION__ << " EOF" << std::endl;
}

/* -------------------------------------------------------------------- */

bool EditCalDialog::openDatabase()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  if (!_calibDB.isValid())
  {
    ELOG << "Shouldn't get here, no DB connection.";
    return false;
  }

  if (!_calibDB.open())
  {
    ELOG << "Failed to open calibrations database: " <<
      _calibDB.lastError().driverText().toAscii().data() ;
    ELOG << _calibDB.lastError().databaseText().toAscii().data();

    QMessageBox::critical(0,
	tr("AUTO CAL."), tr("Failed to open Calibration DataBase."));

    return false;
  }
  std::cout << __PRETTY_FUNCTION__ << " EOF" << std::endl;
  return true;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::closeDatabase()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  _calibDB.close();
  std::cout << __PRETTY_FUNCTION__ << " EOF" << std::endl;
}

/** Pulls then pushes database to the master server.
 */
void EditCalDialog::syncButtonClicked()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  _table->update();
}


/** Saves changes to the local database.
 */
void EditCalDialog::saveButtonClicked()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
}


/** Generates a cal .dat file used by DSM server.
 */
void EditCalDialog::exportButtonClicked()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
}


/** Removes a calibration entry row.
 */
void EditCalDialog::removeButtonClicked()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
}


/** Discards any unsaved changes and exits.
 */
void EditCalDialog::closeButtonClicked()
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;
}
