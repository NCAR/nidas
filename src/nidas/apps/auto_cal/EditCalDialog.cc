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

#include <QProcess>
#include <QRegExp>

namespace n_u = nidas::util;

const QString EditCalDialog::DB_DRIVER     = "QPSQL7";
//const QString EditCalDialog::CALIB_DB_HOST = "merlot.eol.ucar.edu";
const QString EditCalDialog::CALIB_DB_HOST = "localhost";
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";
const QString EditCalDialog::SCRATCH_DIR   = "/scr/raf/local_data/databases/";

/* -------------------------------------------------------------------- */

EditCalDialog::EditCalDialog() : changeDetected(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

//  calfile_dir.setText("/net/jlocal/projects/Configuration/raf/cal_files");
    calfile_dir.setText("/home/local/projects/Configuration/raf/cal_files");

    setupUi(this);

    createDatabaseConnection();
    openDatabase();

    _model = new QSqlTableModel;
    connect(_model, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)) ,
            this,     SLOT(dataChanged(const QModelIndex&,const QModelIndex&)));

    _model->setTable(CALIB_DB_NAME);
    _model->setSort(16, Qt::DescendingOrder);
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

    _table->setSortingEnabled(true);

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
    QPoint globalPos = this->mapToGlobal(pos);
    verticalMenu->exec( globalPos );
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

void EditCalDialog::toggleRow(int id)
{
    QItemSelectionModel *selectionModel = _table->selectionModel();
    selectionModel->clearSelection();

    // Toggle the row's hidden state selected by cal type.
    if (id == 0)
        showAnalog     = !showAnalog;
    else if (id == 1)
        showInstrument = !showInstrument;
    else
        return;

    // This regexp will match var_name's that are analog calibrations.
    QRegExp rx0("BIGBLU_CH([0-7])_(1T|2F|2T|4F)");

    for (int row = 0; row < _model->rowCount(); row++) {

        // get the var_name from the row
        QString var_name = _model->index(row, 5).data().toString().trimmed();

        // apply the new hidden state
        if (rx0.indexIn(var_name) == 0)
            _table->setRowHidden(row, !showAnalog);
        else
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

    syncActn = new QAction(tr("Sync"), this);
    connect(syncActn,   SIGNAL(triggered()), this, SLOT(syncButtonClicked()));
    fileMenu->addAction(syncActn);

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
    addColAction(colsMenu, tr("User"),          colsGrp, colsMapper,  4, true);
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
        QMessageBox::information(0, tr("notice"),
          tr("cannot contact:\n") + source);
        return;
    }

    // Backup the source's calibration database to a directory that is
    // regularly backed up by CIT.
    params.clear();
    params << "-h" << source << "-U" << CALIB_DB_USER << "-d" << CALIB_DB_NAME;
    params << "-f" << SCRATCH_DIR + source + "_cal.sql";

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
    params << "-h" << destination << "-U" << CALIB_DB_USER << "-d" << CALIB_DB_NAME;
    params << "-f" << SCRATCH_DIR + source + "_cal.sql";

    if (process.execute("psql", params)) {
        QMessageBox::information(0, tr("notice"),
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

    syncRemoteCalibTable("hyper.guest.ucar.edu",    CALIB_DB_HOST);
    syncRemoteCalibTable("hercules.guest.ucar.edu", CALIB_DB_HOST);

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

    if (changeDetected) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(0, tr("Sync"),
                    tr("Cannot export while the calibration table "
                       "is currently modified.\n\n"
                       "Save changes to database?\n"),
                    QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) return;
    }
    saveButtonClicked();

    // get selected row number
    QItemSelectionModel *selectionModel = _table->selectionModel();
    selectionModel->clearSelection();
    int currentRow = selectionModel->currentIndex().row();
    std::cout << "currentRow: " << currentRow+1 << std::endl;

    // get the serial_number from the selected row
    QString serial_number = _model->index(currentRow, 4).data().toString().trimmed();
    std::cout << "serial_number: " <<  serial_number.toStdString() << std::endl;

    // get the var_name from the selected row
    QString var_name = _model->index(currentRow, 5).data().toString().trimmed();
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

    QString calibration = _model->index(currentRow, 13).data().toString().trimmed();
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
        if (serial_number.compare(_model->index(topRow, 4).data().toString().trimmed()) != 0) break;
        var_name = _model->index(topRow, 5).data().toString().trimmed();
        if (rx1.indexIn(var_name) == -1) break;

        QString calibration = _model->index(topRow, 13).data().toString().trimmed();
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
        if (serial_number.compare(_model->index(btmRow, 4).data().toString().trimmed()) != 0) break;
        var_name = _model->index(btmRow, 5).data().toString().trimmed();
        if (rx1.indexIn(var_name) == -1) break;

        QString calibration = _model->index(btmRow, 13).data().toString().trimmed();
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
    QModelIndex topRowIdx = _model->index(topRow, 0);
    QModelIndex btmRowIdx = _model->index(btmRow, 0);
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
    QString temperature = _model->index(btmRow, 14).data().toString().trimmed();
    std::cout << "temperature: " << temperature.toStdString() << std::endl;

    // extract timestamp from the btmRow
    QString timestamp = _model->index(btmRow, 16).data().toString().trimmed();
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

    QString aCalFile = calfile_dir.text() + "/A2D/";
    aCalFile += "A2D" + serial_number + ".dat";

    std::cout << "Appending results to: ";
    std::cout << aCalFile.toStdString() << std::endl;
    std::cout << ostr.str() << std::endl;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Export"),
                                  tr("Append to:\n") + aCalFile,
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return;

    int fd = ::open( aCalFile.toStdString().c_str(),
                     O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        ostr.str("");
        ostr << tr("failed to save results to:\n").toStdString();
        ostr << aCalFile.toStdString() << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }
    write(fd, ostr.str().c_str(),
              ostr.str().length());
    ::close(fd);

    // mark what's exported
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList)
        _model->setData(_model->index(rowIndex.row(), 0), "yes", Qt::EditRole);

    saveButtonClicked();

    ostr.str("");
    ostr << tr("saved results to: ").toStdString() << aCalFile.toStdString();
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
