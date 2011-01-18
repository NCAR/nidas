#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <nidas/util/UTime.h>
#include "EditCalDialog.h"

#include <QtGui/QMenuBar>
#include <QtGui/QMenu>

#include <QtGui/QMessageBox>
#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlError>

#include <QProcess>
#include <QRegExp>

namespace n_u = nidas::util;

const QString EditCalDialog::DB_DRIVER     = "QPSQL7";
const QString EditCalDialog::CALIB_DB_HOST = "localhost";
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";

/* -------------------------------------------------------------------- */

EditCalDialog::EditCalDialog() : changeDetected(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    setupUi(this);

    connect(buttonSync,   SIGNAL(clicked()), this, SLOT(syncButtonClicked()));
    connect(buttonSave,   SIGNAL(clicked()), this, SLOT(saveButtonClicked()));
    connect(buttonExport, SIGNAL(clicked()), this, SLOT(exportButtonClicked()));
    connect(buttonRemove, SIGNAL(clicked()), this, SLOT(removeButtonClicked()));
    connect(buttonClose,  SIGNAL(clicked()), this, SLOT(reject()));

    createDatabaseConnection();
    openDatabase();

    _model = new QSqlTableModel;
    connect(_model, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)) ,
            this,     SLOT(dataChanged(const QModelIndex&,const QModelIndex&)));

    _model->setTable("calibrations");
    _model->setSort(16, Qt::DescendingOrder);
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

//_table->setEditTriggers(QAbstractItemView::SelectedClicked);

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

    createMenu();

    _table->adjustSize();
    _table->show();
}

/* -------------------------------------------------------------------- */

EditCalDialog::~EditCalDialog()
{
    delete _model;
    closeDatabase();
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addAction(QMenu *menu, const QString &text,
                                  QActionGroup *group, QSignalMapper *mapper,
                                  int id, bool checked)
{
    QAction *result = menu->addAction(text);
    result->setCheckable(true);
    result->setChecked(checked);
    _table->setColumnHidden(id, !checked);
    group->addAction(result);

    QObject::connect(result, SIGNAL(triggered()), mapper, SLOT(map()));
    mapper->setMapping(result, id);
    return result;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::toggleColumn(int id)
{
    _table->setColumnHidden(id, !_table->isColumnHidden(id));
}

/* -------------------------------------------------------------------- */

void EditCalDialog::createMenu()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    menuBar = new QMenuBar;
    vboxLayout->setMenuBar(menuBar);

    mapper = new QSignalMapper(this);
    connect(mapper, SIGNAL(mapped(int)), this, SLOT(toggleColumn(int)));

    QActionGroup *group = new QActionGroup(this);
    group->setExclusive(false);

    columnsMenu = new QMenu(tr("Columns"));

    addAction(columnsMenu, tr("Platform"),      group, mapper,  0, true);
    addAction(columnsMenu, tr("Project"),       group, mapper,  1, true);
    addAction(columnsMenu, tr("User"),          group, mapper,  2, true);
    addAction(columnsMenu, tr("Sensor Type"),   group, mapper,  3, false);
    addAction(columnsMenu, tr("Serial #"),      group, mapper,  4, true);
    addAction(columnsMenu, tr("Variable"),      group, mapper,  5, true);
    addAction(columnsMenu, tr("DSM"),           group, mapper,  6, false);
    addAction(columnsMenu, tr("Cal Type"),      group, mapper,  7, false);
    addAction(columnsMenu, tr("Analog Ch"),     group, mapper,  8, false);
    addAction(columnsMenu, tr("Gain"),          group, mapper,  9, false);
    addAction(columnsMenu, tr("Set Points"),    group, mapper, 10, false);
    addAction(columnsMenu, tr("Avg Values"),    group, mapper, 11, false);
    addAction(columnsMenu, tr("StdDev Values"), group, mapper, 12, false);
    addAction(columnsMenu, tr("Calibration"),   group, mapper, 13, true);
    addAction(columnsMenu, tr("Temperature"),   group, mapper, 14, false);
    addAction(columnsMenu, tr("Comment"),       group, mapper, 15, false);
    addAction(columnsMenu, tr("Date"),          group, mapper, 16, true);

    menuBar->addMenu(columnsMenu);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::createDatabaseConnection()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // create the default database connection
    _calibDB = QSqlDatabase::addDatabase(DB_DRIVER);

    if (!_calibDB.isValid())
    {
        std::ostringstream ostr;
        ostr << tr("Failed to connect to calibration database.\n").toStdString();

        std::cerr << ostr.str() << std::endl;
        QMessageBox::critical(0, tr("connect"), ostr.str().c_str());
        return;
    }
    _calibDB.setHostName(CALIB_DB_HOST);
    _calibDB.setUserName(CALIB_DB_USER);
    _calibDB.setDatabaseName(CALIB_DB_NAME);
}

/* -------------------------------------------------------------------- */

bool EditCalDialog::openDatabase()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    if (!_calibDB.isValid())
    {
        std::cerr << "Shouldn't get here, no DB connection." << std::endl;
        return false;
    }
    if (!_calibDB.open())
    {
        std::ostringstream ostr;
        ostr << tr("Failed to open calibration database.\n\n").toStdString();
        ostr << tr(_calibDB.lastError().text().toAscii().data()).toStdString();

        std::cerr << ostr.str() << std::endl;
        QMessageBox::critical(0, tr("open"), ostr.str().c_str());

        return false;
    }
    return true;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::closeDatabase()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    _calibDB.close();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::dataChanged(const QModelIndex &topLeft,
                                const QModelIndex &bottomRight)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    changeDetected = true;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::reject()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (changeDetected) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(0, tr("Close"),
                    tr("Save changes to database?\n"),
                    QMessageBox::Yes | QMessageBox::Discard);

        if (reply == QMessageBox::Yes)
            saveButtonClicked();
    }
    QDialog::reject();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::syncRemoteCalibTable(QString source, QString destination)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QProcess process;
    QStringList params;

    // Ping the source to see if it is active.
    params << source << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("notice"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Backup the source's calibration database to a directory that is
    // regularly backed up by CIT.
    params.clear();
    params << "-h" << source << "-U" << "ads" << "-d" << "calibrations";
    params << "-f" << "/scr/raf/local_data/databases/" + source + "_cal.sql";

    if (process.execute("pg_dump", params)) {
        QMessageBox::information(0, tr("notice"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Ping the destination to see if it is active.
    params.clear();
    params << destination << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("notice"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Insert the source's calibration database into the destination's.
    params.clear();
    params << "-h" << destination << "-U" << "ads" << "-d" << "calibrations";
    params << "-f" << "/scr/raf/local_data/databases/" + source + "_cal.sql";

    if (process.execute("psql", params)) {
        QMessageBox::information(0, tr("notice"),
          tr("cannot contact:\n") + source);
        return;
    }
}

/* -------------------------------------------------------------------- */

void EditCalDialog::syncButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (changeDetected) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(0, tr("Sync"),
                    tr("Cannot synchronize while the calibration table "
                       "is currently modified.\n\n"
                       "Save changes to database?\n"),
                    QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) return;
    }
    saveButtonClicked();

    syncRemoteCalibTable("hyper.guest.ucar.edu",    "ruttles.eol.ucar.edu");
    syncRemoteCalibTable("hercules.guest.ucar.edu", "ruttles.eol.ucar.edu");

    _table->update();
    std::cout << __PRETTY_FUNCTION__ << " exiting" << std::endl;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::saveButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    _model->database().transaction();

    if (_model->submitAll()) {
        _model->database().commit();
        changeDetected = false;
    } else {
        _model->database().rollback();
        QMessageBox::warning(0, tr("save"),
                             tr("The database reported an error: %1")
                             .arg(_model->lastError().text()));
    }
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    QItemSelectionModel *selectionModel = _table->selectionModel();
    selectionModel->clearSelection();
    int currentRow = selectionModel->currentIndex().row();
    std::cout << "currentRow: " << currentRow+1 << std::endl;

    //get the serial_number from the selected row
    QString serial_number = _model->index(currentRow, 4, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
    std::cout << "serial_number: " <<  serial_number.toStdString() << std::endl;

    //get the var_name from the selected row
    QString var_name = _model->index(currentRow, 5, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;

    // verify that the var_name indicates that this is an analog calibration
    QRegExp rx0("BIGBLU_CH([0-7])_(1T|2F|2T|4F)");
    if (rx0.indexIn(var_name) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a variable matching\n\n'") + rx0.pattern() +
          tr("'\n\nto export an analog calibration."));
        return;
    }
    int chnMask = 1 << rx0.cap(1).toInt();

    // extract the calibration coefficients from the selected row
    QString offst[8];
    QString slope[8];
    QRegExp rxCoeff2("\\{([+-]?\\d+\\.\\d+),([+-]?\\d+\\.\\d+)\\}");

    QString calibration = _model->index(currentRow, 13, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
    if (rxCoeff2.indexIn(calibration) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
          tr("'\n\nto export an analog calibration."));
        return;
    }
    offst[rx0.cap(1).toInt()] = rxCoeff2.cap(1);
    slope[rx0.cap(1).toInt()] = rxCoeff2.cap(2);

    // search for the other channels and continue extracting coefficients...
    QRegExp rx1("BIGBLU_CH([0-7])_" + rx0.cap(2));

    int topRow = currentRow;
    do {
        if (--topRow < 0) break;
        if (serial_number.compare(_model->index(topRow, 4, QModelIndex()).data(Qt::DisplayRole).toString().trimmed()) != 0) break;
        var_name = _model->index(topRow, 5, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
        if (rx1.indexIn(var_name) == -1) break;

        QString calibration = _model->index(topRow, 13, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
        if (rxCoeff2.indexIn(calibration) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        offst[rx1.cap(1).toInt()] = rxCoeff2.cap(1);
        slope[rx1.cap(1).toInt()] = rxCoeff2.cap(2);
        chnMask |= 1 << rx1.cap(1).toInt();
    } while (true);
    topRow++;

    int numRows = _model->rowCount() - 1;
    int btmRow = currentRow;
    do {
        if (++btmRow > numRows) break;
        if (serial_number.compare(_model->index(btmRow, 4, QModelIndex()).data(Qt::DisplayRole).toString().trimmed()) != 0) break;
        var_name = _model->index(btmRow, 5, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
        if (rx1.indexIn(var_name) == -1) break;

        QString calibration = _model->index(btmRow, 13, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
        if (rxCoeff2.indexIn(calibration) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        offst[rx1.cap(1).toInt()] = rxCoeff2.cap(1);
        slope[rx1.cap(1).toInt()] = rxCoeff2.cap(2);
        chnMask |= 1 << rx1.cap(1).toInt();
    } while (true);
    btmRow--;

    // highlight what's found
    QModelIndex topRowIdx = _model->index(topRow, 0, QModelIndex());
    QModelIndex btmRowIdx = _model->index(btmRow, 0, QModelIndex());
    QItemSelection rowSelection;
    rowSelection.select(topRowIdx, btmRowIdx);
    selectionModel->select(rowSelection,
        QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // complain if the found selection is discontiguous or an undistinct set of 8
    int numFound  = btmRow - topRow + 1;
    std::cout << "chnMask: 0x" << std::hex << chnMask << std::dec << std::endl;
    std::cout << "numFound: " << numFound << std::endl;
    if ((chnMask != 0xff) || (numFound != 8)) {
        QMessageBox::information(0, tr("notice"),
          tr("Discontiguous or an undistinct selection found.\n\n") +
          tr("You need 8 channels selected to generate a calibration dat file!"));
        return;
    }
    // extract temperature from the btmRow
    QString temperature = _model->index(btmRow, 14, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
    std::cout << "temperature: " << temperature.toStdString() << std::endl;

    // extract timestamp from the btmRow
    QString timestamp = _model->index(btmRow, 16, QModelIndex()).data(Qt::DisplayRole).toString().trimmed();
    std::cout << "timestamp: " << timestamp.toStdString() << std::endl;

    // extract gain and bipolar characters
    QRegExp rx2("(.)(.)");
    rx2.indexIn(rx0.cap(2));
    QString G = rx2.cap(1);
    QString B = rx2.cap(2);
    std::cout << "G: " << G.toStdString() << std::endl;
    std::cout << "B: " << B.toStdString() << std::endl;

    // record results to the device's CalFile
    std::ostringstream ostr;
    ostr << std::endl;
    ostr << "# temperature: " << temperature.toStdString() << std::endl;
    ostr << "#  Date              Gain  Bipolar";
    for (uint ix=0; ix<8; ix++)
        ostr << "  CH" << ix << "-off   CH" << ix << "-slope";
    ostr << std::endl;

//  ostr << n_u::UTime(timestamp.toStdString()).format(true,"%Y %b %d %H:%M:%S");
    ostr << timestamp.toStdString();
    ostr << std::setw(6) << G.toStdString();
    ostr << std::setw(9) << B.toStdString();

    for (uint ix=0; ix<8; ix++) {
        ostr << "  " << std::setw(9) << offst[ix].toStdString()
             << " "  << std::setw(9) << slope[ix].toStdString();
    }
    ostr << std::endl;

    std::string aCalFile = "/home/local/projects/Configuration/raf/cal_files/A2D/A2D"
       + serial_number.toStdString() + ".dat";

    std::cout << "Appending results to: ";
    std::cout << aCalFile << std::endl;
    std::cout << ostr.str() << std::endl;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Export"),
                                  tr("Append to:\n") + QString(aCalFile.c_str()),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return;

    int fd = ::open( aCalFile.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        ostr.str("");
        ostr << tr("failed to save results to:\n").toStdString();
        ostr << aCalFile << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }
    write(fd, ostr.str().c_str(),
              ostr.str().length());
    ::close(fd);
    ostr.str("");
    ostr << tr("saved results to: ").toStdString() << aCalFile;
    QMessageBox::information(0, tr("notice"), ostr.str().c_str());
}

/* -------------------------------------------------------------------- */

void EditCalDialog::removeButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QModelIndexList rowList = _table->selectionModel()->selectedRows();

    if (rowList.isEmpty()) return;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Delete"),
                                  tr("Remove selected rows"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return;

    foreach (QModelIndex rowIndex, rowList) {
        _model->removeRow(rowIndex.row(), rowIndex.parent());
        _table->hideRow(rowIndex.row());
    }
    changeDetected = true;
}
