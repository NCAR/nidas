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
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";

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

    // extract some environment variables
    calfile_dir.setText( QString::fromAscii(getenv("PROJ_DIR")) +
                         "/Configuration/raf/cal_files");

    scratch_dir = QString::fromAscii(getenv("DATA_DIR")) + "/databases/";

    // prompt user if they want to pull data from the sites at start up
    QMessageBox::StandardButton reply = QMessageBox::question(0, tr("Pull"),
      tr("Pull calibration databases from the sites?\n"),
      QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
        foreach(QString site, siteList)
            syncRemoteCalibTable(site, CALIB_DB_HOST);

    setupUi(this);

    createDatabaseConnection();
    openDatabase();

    _model = new QSqlTableModel;

    proxyModel = new QSortFilterProxyModel;
    proxyModel->setSourceModel(_model);

    connect(proxyModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)) ,
            this,         SLOT(dataChanged(const QModelIndex&, const QModelIndex&)));

    _model->setTable(CALIB_DB_NAME);
    _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    _model->select();

    int c = 0;
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Removed"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Exported"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Date"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Platform"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Project"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("User"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Sensor Type"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Serial #"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Variable"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("DSM"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Cal Type"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Channel"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("GainBplr"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Set Points"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Avg Values"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("StdDev Values"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Calibration"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Temperature"));
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Comment"));
    _table->setModel(proxyModel);

    QSqlDatabase database = _model->database();
    delegate["removed"]       = new DisabledDelegate;
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

    c = 0;
    _table->setItemDelegateForColumn(c++, delegate["removed"]);
    _table->setItemDelegateForColumn(c++, delegate["exported"]);
    _table->setItemDelegateForColumn(c++, delegate["cal_date"]);
    _table->setItemDelegateForColumn(c++, delegate["site"]);
    _table->setItemDelegateForColumn(c++, delegate["project_name"]);
    _table->setItemDelegateForColumn(c++, delegate["username"]);
    _table->setItemDelegateForColumn(c++, delegate["sensor_type"]);
    _table->setItemDelegateForColumn(c++, delegate["serial_number"]);
    _table->setItemDelegateForColumn(c++, delegate["var_name"]);
    _table->setItemDelegateForColumn(c++, delegate["dsm_name"]);
    _table->setItemDelegateForColumn(c++, delegate["cal_type"]);
    _table->setItemDelegateForColumn(c++, delegate["channel"]);
    _table->setItemDelegateForColumn(c++, delegate["gainbplr"]);
    _table->setItemDelegateForColumn(c++, delegate["set_points"]);
    _table->setItemDelegateForColumn(c++, delegate["avg_volts"]);
    _table->setItemDelegateForColumn(c++, delegate["stddev_volts"]);
    _table->setItemDelegateForColumn(c++, delegate["cal"]);
    _table->setItemDelegateForColumn(c++, delegate["temperature"]);
    _table->setItemDelegateForColumn(c++, delegate["comment"]);

    c = 0;
    col["removed"] = c++;
    col["exported"] = c++;
    col["cal_date"] = c++;
    col["site"] = c++;
    col["project_name"] = c++;
    col["username"] = c++;
    col["sensor_type"] = c++;
    col["serial_number"] = c++;
    col["var_name"] = c++;
    col["dsm_name"] = c++;
    col["cal_type"] = c++;
    col["channel"] = c++;
    col["gainbplr"] = c++;
    col["set_points"] = c++;
    col["avg_volts"] = c++;
    col["stddev_volts"] = c++;
    col["cal"] = c++;
    col["temperature"] = c++;
    col["comment"] = c++;

    QHeaderView *verticalHeader = _table->verticalHeader();
    verticalHeader->setContextMenuPolicy( Qt::CustomContextMenu );

    connect(verticalHeader, SIGNAL( customContextMenuRequested( const QPoint & )),
            this,             SLOT( verticalHeaderMenu( const QPoint & )));

    _table->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    _table->verticalHeader()->setResizeMode(QHeaderView::Fixed);
    _table->horizontalHeader()->setStretchLastSection( true );

    for (int i=0; i < proxyModel->columnCount(); i++)
        _table->resizeColumnToContents(i);

    QHeaderView *horizontalHeader = _table->horizontalHeader();
    horizontalHeader->setMovable(true);
    horizontalHeader->setClickable(true);
    horizontalHeader->setSortIndicator(col["cal_date"], Qt::DescendingOrder);
    _table->setSortingEnabled(true);

    connect(horizontalHeader, SIGNAL( sortIndicatorChanged(int, Qt::SortOrder)),
            this,               SLOT( hideRows()));

    createMenu();

    hideRows();

    _table->adjustSize();
    _table->show();
}

/* -------------------------------------------------------------------- */

EditCalDialog::~EditCalDialog()
{
    closeDatabase();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::verticalHeaderMenu( const QPoint &pos )
{
    // clear any multiple selections made by user
    _table->selectionModel()->clearSelection();

    // select the row
    int row = _table->verticalHeader()->logicalIndexAt(pos);
    _table->selectionModel()->select(proxyModel->index(row, 0),
      QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // show the popup menu
    verticalMenu->exec( _table->verticalHeader()->mapToGlobal(pos) );
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addRowAction(QMenu *menu, const QString &text,
                                     QActionGroup *group, QSignalMapper *mapper,
                                     int id, bool checked)
{
    if      (id == 0) showAnalog     = checked;
    else if (id == 1) showInstrument = checked;
    else if (id == 2) showRemoved    = checked;
    else if (id == 3) showExported   = checked;

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
    return proxyModel->index(row, col).data().toString().trimmed();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::toggleRow(int id)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    _table->selectionModel()->clearSelection();

    // Toggle the row's hidden state
    if      (id == 0) showAnalog     = !showAnalog;
    else if (id == 1) showInstrument = !showInstrument;
    else if (id == 2) showRemoved    = !showRemoved;
    else if (id == 3) showExported   = !showExported;

    hideRows();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::hideRows()
{
    for (int row = 0; row < proxyModel->rowCount(); row++) {

        // get the cal_type from the row
        QString cal_type = modelData(row, col["cal_type"]);

        // apply the new hidden state
        if (cal_type == "analog")
            _table->setRowHidden(row, !showAnalog);

        if (cal_type == "instrument")
            _table->setRowHidden(row, !showInstrument);

        QString removed = modelData(row, col["removed"]);
        if (removed == "true")
            _table->setRowHidden(row, !showRemoved);

        QString exported = modelData(row, col["exported"]);
        if (exported == "true")
            _table->setRowHidden(row, !showExported);
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

    // Popup menu setup... (cannot use keyboard shortcuts here)
    verticalMenu = new QMenu;
    verticalMenu->addAction(tr("Export"), this, SLOT(exportButtonClicked()));
    verticalMenu->addAction(tr("Remove"), this, SLOT(removeButtonClicked()));

    QMenuBar *menuBar = new QMenuBar;
    vboxLayout->setMenuBar(menuBar);

    // File menu setup...
    QMenu *fileMenu = new QMenu(tr("&File"));
    fileMenu->addAction(tr("&Path"), this, SLOT(pathButtonClicked()),
          Qt::CTRL + Qt::Key_P);
    fileMenu->addAction(tr("&Save"), this, SLOT(saveButtonClicked()),
          Qt::CTRL + Qt::Key_S);
    fileMenu->addAction(tr("&Quit"), this, SLOT(reject()),
          Qt::CTRL + Qt::Key_Q);
    menuBar->addMenu(fileMenu);

    QMenu *viewMenu = new QMenu(tr("&View"));

    // View->Rows menu setup...
    QSignalMapper *rowsMapper = new QSignalMapper(this);
    connect(rowsMapper, SIGNAL(mapped(int)), this, SLOT(toggleRow(int)));

    QActionGroup *rowsGrp = new QActionGroup(this);
    rowsGrp->setExclusive(false);

    QMenu *rowsMenu = new QMenu(tr("&Rows"));

    // true == unhidden
    int i = 0;
    addRowAction(rowsMenu, tr("analog"),        rowsGrp, rowsMapper, i++, true);
    addRowAction(rowsMenu, tr("instrument"),    rowsGrp, rowsMapper, i++, true);
    rowsMenu->addSeparator();
    addRowAction(rowsMenu, tr("removed"),       rowsGrp, rowsMapper, i++, false);
    addRowAction(rowsMenu, tr("exported"),      rowsGrp, rowsMapper, i++, false);

    viewMenu->addMenu(rowsMenu);

    // View->Columns menu setup...
    QSignalMapper *colsMapper = new QSignalMapper(this);
    connect(colsMapper, SIGNAL(mapped(int)), this, SLOT(toggleColumn(int)));

    QActionGroup *colsGrp = new QActionGroup(this);
    colsGrp->setExclusive(false);

    QMenu *colsMenu = new QMenu(tr("&Columns"));

    // true == unhidden
    i = 0;
    addColAction(colsMenu, tr("Removed"),       colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Exported"),      colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Date"),          colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Platform"),      colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Project"),       colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("User"),          colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Sensor Type"),   colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Serial #"),      colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Variable"),      colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("DSM"),           colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Cal Type"),      colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Channel"),       colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("GainBplr"),      colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Set Points"),    colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Avg Values"),    colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("StdDev Values"), colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Calibration"),   colsGrp, colsMapper, i++, true);
    addColAction(colsMenu, tr("Temperature"),   colsGrp, colsMapper, i++, false);
    addColAction(colsMenu, tr("Comment"),       colsGrp, colsMapper, i++, false);

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
    params << "-f" << scratch_dir + source + "_cal.sql";

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
    params << "-f" << scratch_dir + source + "_cal.sql";

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
    // Re-apply hidden states, commiting the model causes the MVC to
    // re-display it's view.
    hideRows();
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
    int row = _table->selectionModel()->currentIndex().row();
    std::cout << "row: " << row+1 << std::endl;

    // get the cal_type from the selected row
    QString cal_type = modelData(row, col["cal_type"]);
    std::cout << "cal_type: " <<  cal_type.toStdString() << std::endl;

    if (cal_type == "instrument")
        exportInstrument(row);

    if (cal_type == "analog")
        exportAnalog(row);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportInstrument(int row)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    QString var_name = modelData(row, col["var_name"]);
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;

    // select the row
    QModelIndex currentIdx = proxyModel->index(row, 0);
    _table->selectionModel()->select(currentIdx,
        QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // extract the site of the instrument from the current row
    QString site = modelData(row, col["site"]);
    std::cout << "site: " <<  site.toStdString() << std::endl;

    // extract the cal_date from the current row
    std::string cal_date;
    n_u::UTime ct;
    cal_date = modelData(row, col["cal_date"]).toStdString();
    ct = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");

    // extract the cal coefficients from the selected row
    QRegExp rxCoeffs("\\{(.*)\\}");
    QString cal = modelData(row, col["cal"]);
    if (rxCoeffs.indexIn(cal) == -1) {
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

void EditCalDialog::exportAnalog(int row)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // extract the serial_number of the A2D card from the current row
    QString serial_number = modelData(row, col["serial_number"]);
    std::cout << "serial_number: " <<  serial_number.toStdString() << std::endl;

    QString gainbplr = modelData(row, col["gainbplr"]);
    std::cout << "gainbplr: " <<  gainbplr.toStdString() << std::endl;

    // extract the cal_date from the current row
    std::string cal_date;
    n_u::UTime ut, ct;
    cal_date = modelData(row, col["cal_date"]).toStdString();
    ct = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");

    // extract the cal coefficients from the selected row
    QString offst[8];
    QString slope[8];
    QRegExp rxCoeff2("\\{([+-]?\\d+\\.\\d+),([+-]?\\d+\\.\\d+)\\}");

    QString cal = modelData(row, col["cal"]);
    if (rxCoeff2.indexIn(cal) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
          tr("'\n\nto export an analog calibration."));
        return;
    }
    int channel = modelData(row, col["channel"]).toInt();
    offst[channel] = rxCoeff2.cap(1);
    slope[channel] = rxCoeff2.cap(2);
    int chnMask = 1 << channel;

    // search for the other channels and continue extracting coefficients...
    int topRow = row;
    do {
        if (--topRow < 0) break;
        if (serial_number != modelData(topRow, col["serial_number"])) break;
        if      ("analog" != modelData(topRow, col["cal_type"])) break;
        if      (gainbplr != modelData(topRow, col["gainbplr"])) break;

        cal_date = modelData(topRow, col["cal_date"]).toStdString();
        ut = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << abs(ut.toSecs()-ct.toSecs())
                  << " > " << 12*60*60 << std::endl;
        if (abs(ut.toSecs()-ct.toSecs()) > 12*60*60) break;

        cal = modelData(topRow, col["cal"]);
        if (rxCoeff2.indexIn(cal) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(topRow, col["channel"]).toInt();
        offst[channel] = rxCoeff2.cap(1);
        slope[channel] = rxCoeff2.cap(2);
        chnMask |= 1 << channel;
    } while (true);
    topRow++;

    int numRows = proxyModel->rowCount() - 1;
    int btmRow = row;
    do {
        if (++btmRow > numRows) break;
        if (serial_number != modelData(btmRow, col["serial_number"])) break;
        if      ("analog" != modelData(btmRow, col["cal_type"])) break;
        if      (gainbplr != modelData(btmRow, col["gainbplr"])) break;

        cal_date = modelData(btmRow, col["cal_date"]).toStdString();
        ut = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << abs(ut.toSecs()-ct.toSecs())
                  << " > " << 12*60*60 << std::endl;
        if (abs(ut.toSecs()-ct.toSecs()) > 12*60*60) break;

        cal = modelData(btmRow, col["cal"]);
        if (rxCoeff2.indexIn(cal) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(btmRow, col["channel"]).toInt();
        offst[channel] = rxCoeff2.cap(1);
        slope[channel] = rxCoeff2.cap(2);
        chnMask |= 1 << channel;
    } while (true);
    btmRow--;

    // select the rows of what's found
    QModelIndex topRowIdx = proxyModel->index(topRow, 0);
    QModelIndex btmRowIdx = proxyModel->index(btmRow, 0);
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
    QString temperature = modelData(btmRow, col["temperature"]);

    // extract cal_date from channel 0
    int chn0idx = -1;
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList) {
        chn0idx = rowIndex.row();
        if (modelData(chn0idx, col["channel"]) == "0")
            break;
    }
    cal_date = modelData(chn0idx, col["cal_date"]).toStdString();
    ut = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");

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
    foreach (QModelIndex rowIndex, rowList) {
        proxyModel->setData(proxyModel->index(rowIndex.row(), col["exported"]),
                        "true", Qt::EditRole);
    }
    changeDetected = true;

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

    // mark what's removed
    foreach (QModelIndex rowIndex, rowList)
        proxyModel->setData(proxyModel->index(rowIndex.row(), col["removed"]),
                        "true", Qt::EditRole);

    changeDetected = true;
}
