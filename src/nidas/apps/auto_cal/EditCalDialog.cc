#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <nidas/util/UTime.h>
#include "EditCalDialog.h"
#include "ComboBoxDelegate.h"
#include "DisabledDelegate.h"

#include <QtGui/QMenuBar>
#include <QtGui/QMenu>

#include <QtGui/QMessageBox>
#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlError>

#include <QFileDialog>
#include <QDir>

#include <QHostInfo>
#include <QProcess>
#include <QRegExp>

namespace n_u = nidas::util;

const QString EditCalDialog::DB_DRIVER     = "QPSQL7";
const QString EditCalDialog::CALIB_DB_HOST = "merlot.eol.ucar.edu";
//const QString EditCalDialog::CALIB_DB_HOST = "localhost";
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";
const QString EditCalDialog::SCRATCH_DIR   = "/scr/raf/local_data/databases/";

/* -------------------------------------------------------------------- */

EditCalDialog::EditCalDialog() : changeDetected(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    siteList << "hyper.guest.ucar.edu" << "hercules.guest.ucar.edu";

    // deny editing local calibration database on the sites
    foreach(QString site, siteList)
        if (QHostInfo::localHostName() == site) {
            QMessageBox::information(0, tr("denied"),
              tr("cannot edit local calibration database on:\n") + site);
            exit(1);
        }

    QMessageBox::information(0, tr("initial syncing"),
      tr("pulling calibration databases from the sites..."));

    foreach(QString site, siteList)
        syncRemoteCalibTable(site, CALIB_DB_HOST);

    QMessageBox::information(0, tr("initial syncing"),
      tr("pushing merged calibration database to the sites..."));

    foreach(QString site, siteList)
        syncRemoteCalibTable(CALIB_DB_HOST, site);

//  calfile_dir.setText("/net/jlocal/projects/Configuration/raf/cal_files");
    calfile_dir.setText("/home/local/projects/Configuration/raf/cal_files");

    setupUi(this);

    createDatabaseConnection();
    openDatabase();

    _model = new QSqlTableModel;
    connect(_model, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)) ,
            this,     SLOT(dataChanged(const QModelIndex&,const QModelIndex&)));

    _model->setTable(CALIB_DB_NAME);
    _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    _model->select();

    _model->setHeaderData( 0, Qt::Horizontal, tr("Exported"));
    _model->setHeaderData( 1, Qt::Horizontal, tr("Date"));
    _model->setHeaderData( 2, Qt::Horizontal, tr("Platform"));
    _model->setHeaderData( 3, Qt::Horizontal, tr("Project"));
    _model->setHeaderData( 4, Qt::Horizontal, tr("User"));
    _model->setHeaderData( 5, Qt::Horizontal, tr("Sensor Type"));
    _model->setHeaderData( 6, Qt::Horizontal, tr("Serial #"));
    _model->setHeaderData( 7, Qt::Horizontal, tr("Variable"));
    _model->setHeaderData( 8, Qt::Horizontal, tr("DSM"));
    _model->setHeaderData( 9, Qt::Horizontal, tr("Cal Type"));
    _model->setHeaderData(10, Qt::Horizontal, tr("Channel"));
    _model->setHeaderData(11, Qt::Horizontal, tr("GainBplr"));
    _model->setHeaderData(12, Qt::Horizontal, tr("Set Points"));
    _model->setHeaderData(13, Qt::Horizontal, tr("Avg Values"));
    _model->setHeaderData(14, Qt::Horizontal, tr("StdDev Values"));
    _model->setHeaderData(15, Qt::Horizontal, tr("Calibration"));
    _model->setHeaderData(16, Qt::Horizontal, tr("Temperature"));
    _model->setHeaderData(17, Qt::Horizontal, tr("Comment"));

    _table->setModel(_model);

    QSqlDatabase database = _model->database();
    delegate["exported"]      = new DisabledDelegate;
    delegate["cal_date"]      = new DisabledDelegate;
    delegate["site"]          = new ComboBoxDelegate(database, "site");
    delegate["project_name"]  = new ComboBoxDelegate(database, "project_name");
    delegate["username"]      = new ComboBoxDelegate(database, "username");
    delegate["sensor_type"]   = new ComboBoxDelegate(database, "sensor_type");
    delegate["serial_number"] = new ComboBoxDelegate(database, "serial_number");
    delegate["var_name"]      = new ComboBoxDelegate(database, "var_name");
    delegate["dsm_name"]      = new ComboBoxDelegate(database, "dsm_name");
    delegate["cal_type"]      = new ComboBoxDelegate(database, "cal_type");
    delegate["channel"]       = new ComboBoxDelegate(database, "channel");
    delegate["gainbplr"]      = new ComboBoxDelegate(database, "gainbplr");
    delegate["set_points"]    = new DisabledDelegate;
    delegate["avg_volts"]     = new DisabledDelegate;
    delegate["stddev_volts"]  = new DisabledDelegate;
    delegate["cal"]           = new DisabledDelegate;
    delegate["temperature"]   = new DisabledDelegate;
    delegate["comment"]       = new DisabledDelegate;

    _table->setItemDelegateForColumn( 0, delegate["exported"]);
    _table->setItemDelegateForColumn( 1, delegate["cal_date"]);
    _table->setItemDelegateForColumn( 2, delegate["site"]);
    _table->setItemDelegateForColumn( 3, delegate["project_name"]);
    _table->setItemDelegateForColumn( 4, delegate["username"]);
    _table->setItemDelegateForColumn( 5, delegate["sensor_type"]);
    _table->setItemDelegateForColumn( 6, delegate["serial_number"]);
    _table->setItemDelegateForColumn( 7, delegate["var_name"]);
    _table->setItemDelegateForColumn( 8, delegate["dsm_name"]);
    _table->setItemDelegateForColumn( 9, delegate["cal_type"]);
    _table->setItemDelegateForColumn(10, delegate["channel"]);
    _table->setItemDelegateForColumn(11, delegate["gainbplr"]);
    _table->setItemDelegateForColumn(12, delegate["set_points"]);
    _table->setItemDelegateForColumn(13, delegate["avg_volts"]);
    _table->setItemDelegateForColumn(14, delegate["stddev_volts"]);
    _table->setItemDelegateForColumn(15, delegate["cal"]);
    _table->setItemDelegateForColumn(16, delegate["temperature"]);
    _table->setItemDelegateForColumn(17, delegate["comment"]);

    QHeaderView *verticalHeader = _table->verticalHeader();
    verticalHeader->setContextMenuPolicy( Qt::CustomContextMenu );

    connect(verticalHeader, SIGNAL( customContextMenuRequested( const QPoint & )),
            this,             SLOT( verticalHeaderMenu( const QPoint & )));

    _table->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    _table->verticalHeader()->setResizeMode(QHeaderView::Interactive);
    _table->horizontalHeader()->setStretchLastSection( true );

    for (int i=0; i<_model->columnCount(); i++)
        _table->resizeColumnToContents(i);

    QHeaderView *horizontalHeader = _table->horizontalHeader();
    horizontalHeader->setMovable(true);
    horizontalHeader->setClickable(true);
    horizontalHeader->setSortIndicator(1,Qt::DescendingOrder);
    horizontalHeader->setSortIndicatorShown(true);
    _table->setSortingEnabled(true);

    connect(horizontalHeader, SIGNAL( sortIndicatorChanged(int, Qt::SortOrder)),
            this,               SLOT( hideRows()));

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

void EditCalDialog::verticalHeaderMenu( const QPoint &pos )
{
    // clear any multiple selections made by user
    _table->selectionModel()->clearSelection();

    // select the row
    int row = _table->verticalHeader()->logicalIndexAt(pos);
    _table->selectionModel()->select(_model->index(row, 0),
      QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // show the popup menu
    verticalMenu->exec( _table->verticalHeader()->mapToGlobal(pos) );
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addRowAction(QMenu *menu, const QString &text,
                                     QActionGroup *group, QSignalMapper *mapper,
                                     int id, bool checked)
{
    if (id == 0)
        showAnalog     = checked;
    else if (id == 1)
        showInstrument = checked;

    return addAction(menu, text, group, mapper, id, checked);
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addColAction(QMenu *menu, const QString &text,
                                     QActionGroup *group, QSignalMapper *mapper,
                                     int id, bool checked)
{
    _table->setColumnHidden(id, !checked);

    return addAction(menu, text, group, mapper, id, checked);
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addAction(QMenu *menu, const QString &text,
                                  QActionGroup *group, QSignalMapper *mapper,
                                  int id, bool checked)
{
    QAction *result = menu->addAction(text);
    result->setCheckable(true);
    result->setChecked(checked);
    group->addAction(result);

    QObject::connect(result, SIGNAL(triggered()), mapper, SLOT(map()));
    mapper->setMapping(result, id);
    return result;
}

/* -------------------------------------------------------------------- */

inline QString EditCalDialog::modelData(int row, int col)
{
    return _model->index(row, col).data().toString().trimmed();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::toggleRow(int id)
{
    _table->selectionModel()->clearSelection();

    // Toggle the row's hidden state selected by cal type.
    if (id == 0)
        showAnalog     = !showAnalog;
    else if (id == 1)
        showInstrument = !showInstrument;
    else
        return;

    hideRows();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::hideRows()
{
    for (int row = 0; row < _model->rowCount(); row++) {

        // get the cal_type from the row
        QString cal_type = modelData(row, 9);

        // apply the new hidden state
        if (cal_type == "analog")
            _table->setRowHidden(row, !showAnalog);

        if (cal_type == "instrument")
            _table->setRowHidden(row, !showInstrument);
    }
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

    // Popup menu setup...
    verticalMenu = new QMenu;
    QAction *buttonExport = verticalMenu->addAction("Export");
    connect(buttonExport, SIGNAL(triggered()), this, SLOT(exportButtonClicked()));
    QAction *buttonRemove = verticalMenu->addAction("Remove");
    connect(buttonRemove, SIGNAL(triggered()), this, SLOT(removeButtonClicked()));

    menuBar = new QMenuBar;
    vboxLayout->setMenuBar(menuBar);

    // File menu setup...
    fileMenu = new QMenu(tr("File"));

    pathActn = new QAction(tr("Path"), this);
    connect(pathActn,   SIGNAL(triggered()), this, SLOT(pathButtonClicked()));
    fileMenu->addAction(pathActn);

    saveActn = new QAction(tr("Save"), this);
    connect(saveActn,   SIGNAL(triggered()), this, SLOT(saveButtonClicked()));
    fileMenu->addAction(saveActn);

    exitActn = new QAction(tr("Exit"), this);
    connect(exitActn,  SIGNAL(triggered()), this, SLOT(reject()));
    fileMenu->addAction(exitActn);

    menuBar->addMenu(fileMenu);

    viewMenu = new QMenu(tr("View"));

    // View->Rows menu setup...
    rowsMapper = new QSignalMapper(this);
    connect(rowsMapper, SIGNAL(mapped(int)), this, SLOT(toggleRow(int)));

    QActionGroup *rowsGrp = new QActionGroup(this);
    rowsGrp->setExclusive(false);

    rowsMenu = new QMenu(tr("Rows"));

    addRowAction(rowsMenu, tr("analog"),        rowsGrp, rowsMapper, 0, true);
    addRowAction(rowsMenu, tr("instrument"),    rowsGrp, rowsMapper, 1, true);

    viewMenu->addMenu(rowsMenu);

    // View->Columns menu setup...
    colsMapper = new QSignalMapper(this);
    connect(colsMapper, SIGNAL(mapped(int)), this, SLOT(toggleColumn(int)));

    QActionGroup *colsGrp = new QActionGroup(this);
    colsGrp->setExclusive(false);

    colsMenu = new QMenu(tr("Columns"));

    // true == unhidden
    addColAction(colsMenu, tr("Exported"),      colsGrp, colsMapper,  0, true);
    addColAction(colsMenu, tr("Date"),          colsGrp, colsMapper,  1, true);
    addColAction(colsMenu, tr("Platform"),      colsGrp, colsMapper,  2, true);
    addColAction(colsMenu, tr("Project"),       colsGrp, colsMapper,  3, true);
    addColAction(colsMenu, tr("User"),          colsGrp, colsMapper,  4, false);
    addColAction(colsMenu, tr("Sensor Type"),   colsGrp, colsMapper,  5, false);
    addColAction(colsMenu, tr("Serial #"),      colsGrp, colsMapper,  6, true);
    addColAction(colsMenu, tr("Variable"),      colsGrp, colsMapper,  7, true);
    addColAction(colsMenu, tr("DSM"),           colsGrp, colsMapper,  8, false);
    addColAction(colsMenu, tr("Cal Type"),      colsGrp, colsMapper,  9, false);
    addColAction(colsMenu, tr("Channel"),       colsGrp, colsMapper, 10, false);
    addColAction(colsMenu, tr("GainBplr"),      colsGrp, colsMapper, 11, false);
    addColAction(colsMenu, tr("Set Points"),    colsGrp, colsMapper, 12, false);
    addColAction(colsMenu, tr("Avg Values"),    colsGrp, colsMapper, 13, false);
    addColAction(colsMenu, tr("StdDev Values"), colsGrp, colsMapper, 14, false);
    addColAction(colsMenu, tr("Calibration"),   colsGrp, colsMapper, 15, true);
    addColAction(colsMenu, tr("Temperature"),   colsGrp, colsMapper, 16, false);
    addColAction(colsMenu, tr("Comment"),       colsGrp, colsMapper, 17, false);

    viewMenu->addMenu(colsMenu);

    menuBar->addMenu(viewMenu);
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
        QMessageBox::information(0, tr("syncing calibration database"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Backup the source's calibration database to a directory that is
    // regularly backed up by CIT.
    params.clear();
    if (source == CALIB_DB_HOST)
        params << "--clean";
    params << "-h" << source << "-U" << CALIB_DB_USER << "-d" << CALIB_DB_NAME;
    params << "-f" << SCRATCH_DIR + source + "_cal.sql";

    if (process.execute("pg_dump", params)) {
        QMessageBox::information(0, tr("syncing calibration database"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Ping the destination to see if it is active.
    params.clear();
    params << destination << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("syncing calibration database"),
          tr("cannot contact:\n") + destination);
        return;
    }

    // Insert the source's calibration database into the destination's.
    params.clear();
    params << "-h" << destination << "-U" << CALIB_DB_USER << "-d" << CALIB_DB_NAME;
    params << "-f" << SCRATCH_DIR + source + "_cal.sql";

    if (process.execute("psql", params)) {
        QMessageBox::information(0, tr("syncing calibration database"),
          tr("cannot contact:\n") + source);
        return;
    }
}

/* -------------------------------------------------------------------- */
 
void EditCalDialog::pathButtonClicked()
{
    QFileDialog dirDialog(this, tr("choose your path"), calfile_dir.text());
    dirDialog.setFileMode(QFileDialog::DirectoryOnly);

    connect(&dirDialog, SIGNAL(directoryEntered(QString)),
            &calfile_dir, SLOT(setText(QString)));

    dirDialog.exec();
    std::cout << "calfile_dir: " << calfile_dir.text().toStdString() << std::endl;
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
    // Re-apply hidden state, commiting the model causes the MVC to
    // re-display it's view.
    hideRows();

    // push database to the sites
    foreach(QString site, siteList)
        syncRemoteCalibTable(CALIB_DB_HOST, site);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    if (changeDetected) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(0, tr("Export"),
                    tr("Cannot export while the calibration table "
                       "is currently modified.\n\n"
                       "Save changes to database?\n"),
                    QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) return;
        saveButtonClicked();
    }

    // clear any multiple selections made by user
    _table->selectionModel()->clearSelection();

    // get selected row number
    int currentRow = _table->selectionModel()->currentIndex().row();
    std::cout << "currentRow: " << currentRow+1 << std::endl;

    // get the cal_type from the selected row
    QString cal_type = modelData(currentRow, 9);
    std::cout << "cal_type: " <<  cal_type.toStdString() << std::endl;

    if (cal_type == "instrument")
        exportInstrument(currentRow);

    if (cal_type == "analog")
        exportAnalog(currentRow);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportInstrument(int currentRow)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    QString var_name = modelData(currentRow, 7);
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;

    // select the row
    QModelIndex currentIdx = _model->index(currentRow, 0);
    _table->selectionModel()->select(currentIdx,
        QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // extract the site of the instrument from the current row
    QString site = modelData(currentRow, 2);
    std::cout << "site: " <<  site.toStdString() << std::endl;

    // extract the timestamp from the current row
    std::string timestamp;
    n_u::UTime ct;
    timestamp = modelData(currentRow, 1).toStdString();
    ct = n_u::UTime::parse(true, timestamp, "%Y-%m-%dT%H:%M:%S");

    // extract the calibration coefficients from the selected row
    QRegExp rxCoeffs("\\{(.*)\\}");
    QString calibration = modelData(currentRow, 15);
    if (rxCoeffs.indexIn(calibration) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a calibration matching\n\n'") + rxCoeffs.pattern() + 
          tr("'\n\nto export an instrument calibration."));
        return;
    }
    QStringList coeffList = rxCoeffs.cap(1).split(",");

    // record results to the device's CalFile
    std::ostringstream ostr;
    ostr << std::endl;

    ostr << ct.format(true,"%Y %b %d %H:%M:%S");

    foreach (QString coeff, coeffList)
        ostr << " " << std::setw(9) << coeff.toStdString();

    ostr << std::endl;

    QString aCalFile = calfile_dir.text() + "/Engineering/";
    aCalFile += site + "/" + var_name + ".dat";

    exportCalFile(aCalFile, ostr.str());
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportAnalog(int currentRow)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // extract the serial_number of the A2D card from the current row
    QString serial_number = modelData(currentRow, 6);
    std::cout << "serial_number: " <<  serial_number.toStdString() << std::endl;

    QString gainbplr = modelData(currentRow, 11);
    std::cout << "gainbplr: " <<  gainbplr.toStdString() << std::endl;

    // extract the timestamp from the current row
    std::string timestamp;
    n_u::UTime ut, ct;
    timestamp = modelData(currentRow, 1).toStdString();
    ct = n_u::UTime::parse(true, timestamp, "%Y-%m-%dT%H:%M:%S");

    // extract the calibration coefficients from the selected row
    QString offst[8];
    QString slope[8];
    QRegExp rxCoeff2("\\{([+-]?\\d+\\.\\d+),([+-]?\\d+\\.\\d+)\\}");

    QString calibration = modelData(currentRow, 15);
    if (rxCoeff2.indexIn(calibration) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
          tr("'\n\nto export an analog calibration."));
        return;
    }
    int channel = modelData(currentRow, 10).toInt();
    offst[channel] = rxCoeff2.cap(1);
    slope[channel] = rxCoeff2.cap(2);
    int chnMask = 1 << channel;

    // search for the other channels and continue extracting coefficients...
    int topRow = currentRow;
    do {
        if (--topRow < 0) break;
        if (serial_number != modelData(topRow, 6)) break;
        if ("analog" != modelData(topRow, 9)) break;
        if (gainbplr != modelData(topRow, 11)) break;
        timestamp = modelData(topRow, 1).toStdString();
        ut = n_u::UTime::parse(true, timestamp, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << abs(ut.toSecs()-ct.toSecs())
                  << " > " << 12*60*60 << std::endl;
        if (abs(ut.toSecs()-ct.toSecs()) > 12*60*60) break;

        QString calibration = modelData(topRow, 15);
        if (rxCoeff2.indexIn(calibration) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(topRow, 10).toInt();
        offst[channel] = rxCoeff2.cap(1);
        slope[channel] = rxCoeff2.cap(2);
        chnMask |= 1 << channel;
    } while (true);
    topRow++;

    int numRows = _model->rowCount() - 1;
    int btmRow = currentRow;
    do {
        if (++btmRow > numRows) break;
        if (serial_number != modelData(btmRow, 6)) break;
        if ("analog" != modelData(btmRow, 9)) break;
        if (gainbplr != modelData(btmRow, 11)) break;
        timestamp = modelData(btmRow, 1).toStdString();
        ut = n_u::UTime::parse(true, timestamp, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << abs(ut.toSecs()-ct.toSecs())
                  << " > " << 12*60*60 << std::endl;
        if (abs(ut.toSecs()-ct.toSecs()) > 12*60*60) break;

        QString calibration = modelData(btmRow, 15);
        if (rxCoeff2.indexIn(calibration) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(btmRow, 10).toInt();
        offst[channel] = rxCoeff2.cap(1);
        slope[channel] = rxCoeff2.cap(2);
        chnMask |= 1 << channel;
    } while (true);
    btmRow--;

    // select the rows of what's found
    QModelIndex topRowIdx = _model->index(topRow, 0);
    QModelIndex btmRowIdx = _model->index(btmRow, 0);
    QItemSelection rowSelection;
    rowSelection.select(topRowIdx, btmRowIdx);
    _table->selectionModel()->select(rowSelection,
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
    QString temperature = modelData(btmRow, 16);

    // extract timestamp from channel 0
    int chn0idx = -1;
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList) {
        chn0idx = rowIndex.row();
        if (modelData(chn0idx, 10) == "0")
            break;
    }
    timestamp = modelData(chn0idx, 1).toStdString();
    ut = n_u::UTime::parse(true, timestamp, "%Y-%m-%dT%H:%M:%S");

    // record results to the device's CalFile
    std::ostringstream ostr;
    ostr << std::endl;
    ostr << "# temperature: " << temperature.toStdString() << std::endl;
    ostr << "#  Date              Gain  Bipolar";
    for (uint ix=0; ix<8; ix++)
        ostr << "  CH" << ix << "-off   CH" << ix << "-slope";
    ostr << std::endl;

    ostr << ut.format(true,"%Y %b %d %H:%M:%S");

    std::map<QString, std::string> gainbplr_out;
    gainbplr_out["1T"] = "    1        1";
    gainbplr_out["2F"] = "    2        0";
    gainbplr_out["2T"] = "    2        1";
    gainbplr_out["4F"] = "    4        0";
    ostr << gainbplr_out[gainbplr];

    for (uint ix=0; ix<8; ix++) {
        ostr << "  " << std::setw(9) << offst[ix].toStdString()
             << " "  << std::setw(9) << slope[ix].toStdString();
    }
    ostr << std::endl;

    QString aCalFile = calfile_dir.text() + "/A2D/";
    aCalFile += "A2D" + serial_number + ".dat";

    exportCalFile(aCalFile, ostr.str());
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportCalFile(QString filename, std::string contents)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    std::cout << "Appending results to: ";
    std::cout << filename.toStdString() << std::endl;
    std::cout << contents << std::endl;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Export"),
                                  tr("Append to:\n") + filename,
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return;

    int fd = ::open( filename.toStdString().c_str(),
                     O_WRONLY | O_APPEND | O_CREAT, 0644);

    std::ostringstream ostr;
    if (fd == -1) {
        ostr << tr("failed to save results to:\n").toStdString();
        ostr << filename.toStdString() << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }
    write(fd, contents.c_str(), contents.length());
    ::close(fd);

    // mark what's exported
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList)
        _model->setData(_model->index(rowIndex.row(), 0), "yes", Qt::EditRole);

    saveButtonClicked();

    ostr << tr("saved results to: ").toStdString() << filename.toStdString();
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

    foreach (QModelIndex rowIndex, rowList)
        _model->removeRow(rowIndex.row(), rowIndex.parent());

    changeDetected = true;
}
