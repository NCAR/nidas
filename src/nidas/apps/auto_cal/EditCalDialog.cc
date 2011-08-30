#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <fstream>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

#include <nidas/util/UTime.h>
#include "EditCalDialog.h"
#include "ViewTextDialog.h"
#include "ComboBoxDelegate.h"
#include "DisabledDelegate.h"
#include "polyfitgsl.h"

#include <QtGui/QMenuBar>
#include <QtGui/QMenu>

#include <QtGui/QMessageBox>
#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlError>

#include <QTextStream>
#include <QFileDialog>
#include <QDir>

#include <QDateTime>
#include <QSqlQuery>
#include <QInputDialog>

#include <QHostInfo>
#include <QProcess>
#include <QRegExp>

namespace n_u = nidas::util;

const QString EditCalDialog::DB_DRIVER     = "QPSQL7";
const QString EditCalDialog::CALIB_DB_HOST = "merlot.eol.ucar.edu";
const QString EditCalDialog::CALIB_DB_USER = "ads";
const QString EditCalDialog::CALIB_DB_NAME = "calibrations";
const int EditCalDialog::MAX_ORDER = 4;

/* -------------------------------------------------------------------- */

EditCalDialog::EditCalDialog() : changeDetected(false), exportUsed(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // Ping the master DB server to see if it is active.
    QProcess process;
    QStringList params;
    params << CALIB_DB_HOST << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("pinging calibration database"),
          tr("cannot contact:\n") + CALIB_DB_HOST);
        return;
    }
    // List of remote sites that fill a calibration database.
    QStringList siteList;
    siteList << "tads.eol.ucar.edu"
             << "hyper.guest.ucar.edu"
             << "hercules.guest.ucar.edu";

    tailNum["tads.eol.ucar.edu"]       = "N600";
    tailNum["hyper.guest.ucar.edu"]    = "N677F";
    tailNum["hercules.guest.ucar.edu"] = "N130AR";

    tailNumIdx[0] = "N600";
    tailNumIdx[1] = "N677F";
    tailNumIdx[2] = "N130AR";

    // define character locations of the status flags
    statfi['C'] = 0;
    statfi['R'] = 1;
    statfi['E'] = 2;

    // deny editing local calibration database on the sites
    foreach(QString site, siteList)
        if (QHostInfo::localHostName() == site) {
            QMessageBox::information(0, tr("denied"),
              tr("cannot edit local calibration database on:\n") + site);
            exit(1);
        }

    // extract some environment variables
    calfile_dir.setText( QString::fromAscii(getenv("PROJ_DIR")) +
                         "/Configuration/raf/cal_files/");
    csvfile_dir.setText( QString::fromAscii(getenv("PWD")) +
                         "/");

    createDatabaseConnection();

    // prompt user if they want to pull data from the sites at start up
    QMessageBox::StandardButton reply = QMessageBox::question(0, tr("Pull"),
      tr("Pull calibration databases from the sites?\n"),
      QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
        foreach(QString site, siteList)
            importRemoteCalibTable(site);

    _calibDB.setHostName(CALIB_DB_HOST);
    openDatabase();

    setupUi(this);

    _model = new QSqlTableModel;

    proxyModel = new QSortFilterProxyModel;
    proxyModel->setSourceModel(_model);

    connect(proxyModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)) ,
            this,         SLOT(dataChanged(const QModelIndex&, const QModelIndex&)));

    _model->setTable(CALIB_DB_NAME);
    _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    _model->select();

    int c = 0;
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Row Id"));        // rid
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Parent Id"));     // pid
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Status"));        // status
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Date"));          // cal_date
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Project"));       // project_name
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("User"));          // username
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Sensor Type"));   // sensor_type
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Serial #"));      // serial_number
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Variable"));      // var_name
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("DSM"));           // dsm_name
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Cal Type"));      // cal_type
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Channel"));       // channel
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("GainBplr"));      // gainbplr
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("ADS file name")); // ads_file_name
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Set Times"));     // set_times
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Set Points"));    // set_points
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Avg Values"));    // averages
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("StdDev Values")); // stddevs
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Calibration"));   // cal
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Temperature"));   // temperature
    proxyModel->setHeaderData(c++, Qt::Horizontal, tr("Comment"));       // comment
    _table->setModel(proxyModel);

    QSqlDatabase database = _model->database();
    delegate["rid"]           = new DisabledDelegate;
    delegate["pid"]           = new DisabledDelegate;
    delegate["status"]        = new DisabledDelegate;
    delegate["cal_date"]      = new DisabledDelegate;
    delegate["project_name"]  = new ComboBoxDelegate(database, "project_name");
    delegate["username"]      = new ComboBoxDelegate(database, "username");
    delegate["sensor_type"]   = new ComboBoxDelegate(database, "sensor_type");
    delegate["serial_number"] = new ComboBoxDelegate(database, "serial_number");
    delegate["var_name"]      = new ComboBoxDelegate(database, "var_name");
    delegate["dsm_name"]      = new ComboBoxDelegate(database, "dsm_name");
    delegate["cal_type"]      = new ComboBoxDelegate(database, "cal_type");
    delegate["channel"]       = new ComboBoxDelegate(database, "channel");
    delegate["gainbplr"]      = new ComboBoxDelegate(database, "gainbplr");
    delegate["ads_file_name"] = new DisabledDelegate;
    delegate["set_times"]     = new DisabledDelegate;
    delegate["set_points"]    = new DisabledDelegate;
    delegate["averages"]      = new DisabledDelegate;
    delegate["stddevs"]       = new DisabledDelegate;
    delegate["cal"]           = new DisabledDelegate;
    delegate["temperature"]   = new DisabledDelegate;
    delegate["comment"]       = new DisabledDelegate;

    c = 0;
    _table->setItemDelegateForColumn(c++, delegate["rid"]);
    _table->setItemDelegateForColumn(c++, delegate["pid"]);
    _table->setItemDelegateForColumn(c++, delegate["status"]);
    _table->setItemDelegateForColumn(c++, delegate["cal_date"]);
    _table->setItemDelegateForColumn(c++, delegate["project_name"]);
    _table->setItemDelegateForColumn(c++, delegate["username"]);
    _table->setItemDelegateForColumn(c++, delegate["sensor_type"]);
    _table->setItemDelegateForColumn(c++, delegate["serial_number"]);
    _table->setItemDelegateForColumn(c++, delegate["var_name"]);
    _table->setItemDelegateForColumn(c++, delegate["dsm_name"]);
    _table->setItemDelegateForColumn(c++, delegate["cal_type"]);
    _table->setItemDelegateForColumn(c++, delegate["channel"]);
    _table->setItemDelegateForColumn(c++, delegate["gainbplr"]);
    _table->setItemDelegateForColumn(c++, delegate["ads_file_name"]);
    _table->setItemDelegateForColumn(c++, delegate["set_times"]);
    _table->setItemDelegateForColumn(c++, delegate["set_points"]);
    _table->setItemDelegateForColumn(c++, delegate["averages"]);
    _table->setItemDelegateForColumn(c++, delegate["stddevs"]);
    _table->setItemDelegateForColumn(c++, delegate["cal"]);
    _table->setItemDelegateForColumn(c++, delegate["temperature"]);
    _table->setItemDelegateForColumn(c++, delegate["comment"]);

    c = 0;
    col["rid"] = c++;
    col["pid"] = c++;
    col["status"] = c++;
    col["cal_date"] = c++;
    col["project_name"] = c++;
    col["username"] = c++;
    col["sensor_type"] = c++;
    col["serial_number"] = c++;
    col["var_name"] = c++;
    col["dsm_name"] = c++;
    col["cal_type"] = c++;
    col["channel"] = c++;
    col["gainbplr"] = c++;
    col["ads_file_name"] = c++;
    col["set_times"] = c++;
    col["set_points"] = c++;
    col["averages"] = c++;
    col["stddevs"] = c++;
    col["cal"] = c++;
    col["temperature"] = c++;
    col["comment"] = c++;

    _table->setContextMenuPolicy( Qt::CustomContextMenu );

    connect(_table, SIGNAL( customContextMenuRequested( const QPoint & )),
            this,     SLOT( contextMenu( const QPoint & )));

    _table->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    _table->verticalHeader()->setResizeMode(QHeaderView::Fixed);
    _table->verticalHeader()->hide();

    for (int i=0; i < proxyModel->columnCount(); i++)
        _table->resizeColumnToContents(i);

    QHeaderView *horizontalHeader = _table->horizontalHeader();
    horizontalHeader->setMovable(true);
    horizontalHeader->setClickable(true);
    horizontalHeader->setSortIndicator(col["cal_date"], Qt::DescendingOrder);
    _table->setSortingEnabled(true);

    createMenu();

    hideRows();

    _table->adjustSize();
    _table->show();
}

/* -------------------------------------------------------------------- */

EditCalDialog::~EditCalDialog()
{
    _calibDB.close();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::contextMenu( const QPoint &pos )
{
    // clear any multiple selections made by user
    _table->selectionModel()->clearSelection();

    // select the row
    int row = _table->indexAt(pos).row();
    _table->selectionModel()->select(proxyModel->index(row, 0),
      QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // show the popup menu
    verticalMenu->exec( _table->mapToGlobal(pos) + QPoint(20,0) );
}

/* -------------------------------------------------------------------- */

QAction *EditCalDialog::addRowAction(QMenu *menu, const QString &text,
                                     QActionGroup *group, QSignalMapper *mapper,
                                     int id, bool checked)
{
    int n = 0;
    if      (id == n++) showAnalog     = checked;
    else if (id == n++) showInstrument = checked;
    else if (id == n++) showTailNum[0] = checked;
    else if (id == n++) showTailNum[1] = checked;
    else if (id == n++) showTailNum[2] = checked;
    else if (id == n++) showCloned     = checked;
    else if (id == n++) showRemoved    = checked;
    else if (id == n++) showExported   = checked;

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
    int n = 0;
    if      (id == n++) showAnalog     = !showAnalog;
    else if (id == n++) showInstrument = !showInstrument;
    else if (id == n++) showTailNum[0] = !showTailNum[0];
    else if (id == n++) showTailNum[1] = !showTailNum[1];
    else if (id == n++) showTailNum[2] = !showTailNum[2];
    else if (id == n++) showCloned     = !showCloned;
    else if (id == n++) showRemoved    = !showRemoved;
    else if (id == n++) showExported   = !showExported;

    hideRows();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::hideRows()
{
    QRegExp rxSite("(.*)_");

    for (int row = 0; row < proxyModel->rowCount(); row++) {

        QString status = modelData(row, col["status"]);
        QString cal_type = modelData(row, col["cal_type"]);
        QString rid = modelData(row, col["rid"]);
        if (rxSite.indexIn(rid) == -1) {
            QMessageBox::warning(0, tr("error"),
              tr("Site name (tail number) not found in 'rid'!"));
            return;
        }
        QString site = rxSite.cap(1);

        bool shownType = false;
        shownType |= ((cal_type == "analog") && showAnalog);
        shownType |= ((cal_type == "instrument") && showInstrument);

        bool shownSite = false;
        shownSite |= ((site == tailNumIdx[0]) && showTailNum[0]);
        shownSite |= ((site == tailNumIdx[1]) && showTailNum[1]);
        shownSite |= ((site == tailNumIdx[2]) && showTailNum[2]);

        bool shownStatus = false;
        shownStatus |= ((status[statfi['C']] == 'C') && showCloned);
        shownStatus |= ((status[statfi['R']] == 'R') && showRemoved);
        shownStatus |= ((status[statfi['E']] == 'E') && showExported);
        shownStatus |= (status == "___");

        bool shown;
        shown = shownStatus & shownType & shownSite;
        _table->setRowHidden(row, !shown);
    }
}


/* -------------------------------------------------------------------- */

void EditCalDialog::toggleColumn(int id)
{
    _table->setColumnHidden(id, !_table->isColumnHidden(id));
    _table->resizeColumnToContents(id);

    // reduce the size of some of the larger columns
    if (id == col["comment"])
        _table->setColumnWidth(id, 300);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::createMenu()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // Popup menu setup... (cannot use keyboard shortcuts here)
    verticalMenu = new QMenu;
    verticalMenu->addAction(tr("Export to Cal File"), this, SLOT(exportCalButtonClicked()));
    verticalMenu->addAction(tr("Export to CSV File"), this, SLOT(exportCsvButtonClicked()));
    verticalMenu->addAction(tr("View Cal File"), this, SLOT(viewCalButtonClicked()));
    verticalMenu->addAction(tr("View CSV File"), this, SLOT(viewCsvButtonClicked()));
    verticalMenu->addAction(tr("Clone this Entry"), this, SLOT(cloneButtonClicked()));
    verticalMenu->addAction(tr("Delete this Entry"), this, SLOT(removeButtonClicked()));
    verticalMenu->addAction(tr("Change Polynominal Fit..."), this, SLOT(changeFitButtonClicked()));

    QMenuBar *menuBar = new QMenuBar;
    vboxLayout->setMenuBar(menuBar);

    // File menu setup...
    QMenu *fileMenu = new QMenu(tr("&File"));
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
    addRowAction(rowsMenu, tailNumIdx[0],       rowsGrp, rowsMapper, i++, true);
    addRowAction(rowsMenu, tailNumIdx[1],       rowsGrp, rowsMapper, i++, true);
    addRowAction(rowsMenu, tailNumIdx[2],       rowsGrp, rowsMapper, i++, true);
    rowsMenu->addSeparator();
    addRowAction(rowsMenu, tr("cloned"),        rowsGrp, rowsMapper, i++, false);
    addRowAction(rowsMenu, tr("removed"),       rowsGrp, rowsMapper, i++, false);
    addRowAction(rowsMenu, tr("exported"),      rowsGrp, rowsMapper, i++, false);
    rowsMenu->addSeparator();
    rowsMenu->addAction(tr("show only checked rows"), this, SLOT(hideRows()));

    viewMenu->addMenu(rowsMenu);

    // View->Columns menu setup...
    QSignalMapper *colsMapper = new QSignalMapper(this);
    connect(colsMapper, SIGNAL(mapped(int)), this, SLOT(toggleColumn(int)));

    QActionGroup *colsGrp = new QActionGroup(this);
    colsGrp->setExclusive(false);

    QMenu *colsMenu = new QMenu(tr("&Columns"));

    // true == unhidden
    i = 0;
    addColAction(colsMenu, tr("Row Id"),        colsGrp, colsMapper, i++, false); // rid
    addColAction(colsMenu, tr("Parent Id"),     colsGrp, colsMapper, i++, false); // pid
    addColAction(colsMenu, tr("Status"),        colsGrp, colsMapper, i++, true);  // status
    addColAction(colsMenu, tr("Date"),          colsGrp, colsMapper, i++, true);  // cal_date
    addColAction(colsMenu, tr("Project"),       colsGrp, colsMapper, i++, true);  // project_name
    addColAction(colsMenu, tr("User"),          colsGrp, colsMapper, i++, false); // username
    addColAction(colsMenu, tr("Sensor Type"),   colsGrp, colsMapper, i++, false); // sensor_type
    addColAction(colsMenu, tr("Serial #"),      colsGrp, colsMapper, i++, true);  // serial_number
    addColAction(colsMenu, tr("Variable"),      colsGrp, colsMapper, i++, true);  // var_name
    addColAction(colsMenu, tr("DSM"),           colsGrp, colsMapper, i++, false); // dsm_name
    addColAction(colsMenu, tr("Cal Type"),      colsGrp, colsMapper, i++, true);  // cal_type
    addColAction(colsMenu, tr("Channel"),       colsGrp, colsMapper, i++, false); // channel
    addColAction(colsMenu, tr("GainBplr"),      colsGrp, colsMapper, i++, false); // gainbplr
    addColAction(colsMenu, tr("ADS file name"), colsGrp, colsMapper, i++, false); // ads_file_name
    addColAction(colsMenu, tr("Set Times"),     colsGrp, colsMapper, i++, false); // set_times
    addColAction(colsMenu, tr("Set Points"),    colsGrp, colsMapper, i++, false); // set_points
    addColAction(colsMenu, tr("Avg Values"),    colsGrp, colsMapper, i++, false); // averages
    addColAction(colsMenu, tr("StdDev Values"), colsGrp, colsMapper, i++, false); // stddevs
    addColAction(colsMenu, tr("Calibration"),   colsGrp, colsMapper, i++, true);  // cal
    addColAction(colsMenu, tr("Temperature"),   colsGrp, colsMapper, i++, false); // temperature
    addColAction(colsMenu, tr("Comment"),       colsGrp, colsMapper, i++, false); // comment

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
        ostr << tr("Unsupported database driver: ").toStdString();
        ostr << DB_DRIVER.toStdString().c_str();

        std::cerr << ostr.str() << std::endl;
        QMessageBox::critical(0, tr("connect"), ostr.str().c_str());
        return;
    }
    _calibDB.setUserName(CALIB_DB_USER);
    _calibDB.setDatabaseName(CALIB_DB_NAME);
}

/* -------------------------------------------------------------------- */

bool EditCalDialog::openDatabase()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

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

void EditCalDialog::importRemoteCalibTable(QString remote)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QProcess process;
    QStringList params;

    QString data_dir = QString::fromAscii(getenv("DATA_DIR")) + "/databases/";

    // Ping the remote DB server to see if it is active.
    params << remote << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("pinging calibration database"),
          tr("cannot contact:\n") + remote);
        return;
    }
    // Dump the remote's calibration database to a directory that is
    // regularly backed up by CIT.
    std::string remoteCalSql;
    remoteCalSql = data_dir.toStdString() + remote.toStdString() + "_cal.sql";

    // Delete the previous '..._cal.sql' file
    std::cout << "deleting older sql file" << std::endl;
    std::stringstream rmCmd;
    rmCmd << "/bin/rm -f " << remoteCalSql;
    std::cout << rmCmd.str() << std::endl;
    if (system(rmCmd.str().c_str())) {
        QMessageBox::information(0, tr("deleting older sql file"),
          tr("cannot delete:\n") + remoteCalSql.c_str());
        return;
    }   
    // Obtain the latest cal_date from the master DB.
    _calibDB.setHostName(CALIB_DB_HOST);
    openDatabase();
    QString cmd, lastCalDate("1970-01-01 00:00:00.00");
    QSqlQuery queryMaster(_calibDB);
    cmd = "SELECT MAX(cal_date) FROM calibrations WHERE "
          "pid='' AND rid ~* '^" + tailNum[remote] + "_'";
    std::cout << cmd.toStdString() << std::endl;
    if (queryMaster.exec(cmd.toStdString().c_str()) && queryMaster.first())
        lastCalDate = queryMaster.value(0).toString();
    std::cout << lastCalDate.toStdString() << " " << tailNum[remote].toStdString() << std::endl;
    _calibDB.close();

    // Build a temporary table of the newer rows in the remote DB.
    _calibDB.setHostName(remote);
    openDatabase();
    QSqlQuery queryRemote(_calibDB);
    queryRemote.exec("DROP TABLE imported");
    queryRemote.exec("CREATE TABLE imported (LIKE calibrations)");
    queryRemote.exec("INSERT INTO imported SELECT * FROM calibrations"
                     " WHERE cal_date > '" + lastCalDate + "'");
    _calibDB.close();

    // Use "...to_char(nextval(..." to ensure that new rid(s) are created 
    // in the master database.
    QString setRid = " | sed \"s/VALUES ('" + tailNum[remote] + \
                     "_........', /VALUES (to_char(nextval('" + \
                     tailNum[remote] + "_rid'),'\"" + \
                     tailNum[remote] + "_\"FM00000000'), /\"";

    // The dump is filtered to just the INSERT commands.
    std::stringstream pg_dump;
    pg_dump << "/opt/local/bin/pg_dump --insert -h " << remote.toStdString()
            << " -U " << CALIB_DB_USER.toStdString()
            << " " << CALIB_DB_NAME.toStdString() << " -t imported"
            << " | grep INSERT"
            << " | sed 's/INSERT INTO imported VALUES /INSERT INTO calibrations VALUES /'"
            << setRid.toStdString()
            << " > " << remoteCalSql;

    std::cout << pg_dump.str() << std::endl;
    if (system(pg_dump.str().c_str())) {
        QMessageBox::information(0, tr("dumping calibration database"),
          tr("cannot contact:\n") + remote);
        return;
    }
    // Insert the remote's calibration database into the master's.
    params.clear();
    params << "-h" << CALIB_DB_HOST << "-U" << CALIB_DB_USER << "-d" << CALIB_DB_NAME;
    params << "-f" << remoteCalSql.c_str();

    if (process.execute("psql", params)) {
        QMessageBox::information(0, tr("importing remote calibration database"),
          tr("psql command failed"));
        return;
    }
}

/* -------------------------------------------------------------------- */

int EditCalDialog::saveButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    if (_model->database().transaction() &&
        _model->submitAll() &&
        _model->database().commit()) {

        // calibration database successfully updated
        changeDetected = false;
    } else {
        QString lastError = _model->lastError().text();
        _model->database().rollback();
        QMessageBox::warning(0, tr("save"),
           tr("The database reported an error: %1") .arg(lastError));
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportCalButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    if (changeDetected && !exportUsed) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(0, tr("Export"),
                    tr("Cannot export while the calibration table "
                       "is currently modified.\n\n"
                       "Save changes to database?\n"),
                    QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) return;
        if (saveButtonClicked()) return;
    }
    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    // don't export anything that was removed
    QString status = modelData(row, col["status"]);
    if (status[statfi['R']] == 'R') {
        QMessageBox::information(0, tr("notice"),
          tr("You cannot export a calibration from a removed row."));
        return;
    }
    // get the cal_type from the selected row
    QString cal_type = modelData(row, col["cal_type"]);
    std::cout << "cal_type: " <<  cal_type.toStdString() << std::endl;

    if (cal_type == "instrument")
        exportInstrument(row);

    if (cal_type == "analog")
        exportAnalog(row);
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportCsvButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    QRegExp rxCSV("\\{(.*)\\}");

    // extract the set_points from the selected row
    QString set_points = modelData(row, col["set_points"]);
    if (rxCSV.indexIn(set_points) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("No set_points found!"));
        return;
    }
    QStringList setPoints = rxCSV.cap(1).split(",");

    // extract the averages from the selected row
    QString averages = modelData(row, col["averages"]);
    if (rxCSV.indexIn(averages) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("No averages found!"));
        return;
    }
    QStringList Averages = rxCSV.cap(1).split(",");

    std::ostringstream ostr;
    ostr << "setPoint,Average\n";

    QStringListIterator iP(setPoints);
    QStringListIterator iA(Averages);
    while (iP.hasNext() && iA.hasNext())
        ostr << iP.next().toStdString() << ","
             << iA.next().toStdString() << "\n";

    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, col["rid"]);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);
    QString var_name = modelData(row, col["var_name"]);

    QString filename = csvfile_dir.text();
    filename += site + "_" + var_name + ".csv";

    exportCsvFile(filename, ostr.str());
}

/* -------------------------------------------------------------------- */

void EditCalDialog::viewCalButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    // get the cal_type from the selected row
    QString cal_type = modelData(row, col["cal_type"]);

    QString filename = calfile_dir.text();

    if (cal_type == "instrument") {
        QString var_name = modelData(row, col["var_name"]);

        // extract the site of the instrument from the current row
        QRegExp rxSite("(.*)_");
        QString rid = modelData(row, col["rid"]);
        if (rxSite.indexIn(rid) == -1) {
            QMessageBox::warning(0, tr("error"),
              tr("Site name (tail number) not found in 'rid'!"));
            return;
        }
        QString site = rxSite.cap(1);

        filename += QString("Engineering/");
        filename += site + "/" + var_name + ".dat";
    }
    else if (cal_type == "analog") {
        // extract the serial_number of the A2D card from the current row
        QString serial_number = modelData(row, col["serial_number"]);

        filename += QString("A2D/");
        filename += "A2D" + serial_number + ".dat";
    }
    else 
        return;

    viewFile(filename, "Calibration File Viewer");
}

/* -------------------------------------------------------------------- */

void EditCalDialog::viewCsvButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, col["rid"]);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);
    QString var_name = modelData(row, col["var_name"]);

    QString filename = csvfile_dir.text();
    filename += site + "_" + var_name + ".csv";

    viewFile(filename, "CSV File Viewer");
}

/* -------------------------------------------------------------------- */

void EditCalDialog::viewFile(QString filename, QString title)
{
    std::cout << "filename: " <<  filename.toStdString() << std::endl;
    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        QTextStream in(&file);
        const QString data = in.readAll();
        ViewTextDialog viewTextDialog;
        viewTextDialog.setWindowTitle(QApplication::translate("Ui::ViewTextDialog",
          title.toStdString().c_str(), 0, QApplication::UnicodeUTF8));
        viewTextDialog.setContents(&data);
        viewTextDialog.exec();
    }
    else
        QMessageBox::information(0, tr("notice"),
          tr("missing:\n") + filename + tr("\n\nNot found."));
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportInstrument(int row)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    QString var_name = modelData(row, col["var_name"]);
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;

    // extract the site of the instrument from the current row
    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, col["rid"]);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);
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

    QString filename = calfile_dir.text() + "Engineering/";
    filename += site + "/" + var_name + ".dat";

    exportCalFile(filename, ostr.str());
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
        QString status = modelData(topRow, col["status"]);
        if (status[statfi['R']] == 'R') {
            QMessageBox::information(0, tr("notice"),
              tr("You cannot export a calibration with a removed row."));
            return;
        }
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
        QString status = modelData(btmRow, col["status"]);
        if (status[statfi['R']] == 'R') {
            QMessageBox::information(0, tr("notice"),
              tr("You cannot export a calibration with a removed row."));
            return;
        }
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

    QString filename = calfile_dir.text() + "A2D/";
    filename += "A2D" + serial_number + ".dat";

    exportCalFile(filename, ostr.str());
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportCalFile(QString filename, std::string contents)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // provide a dialog for the user to select what to save as...
    filename = QFileDialog::getSaveFileName(this, tr("Export Into File"),
                 filename, tr("Cal files (*.dat)"), 0,
                 QFileDialog::DontConfirmOverwrite);

    if (filename.isEmpty()) return;

    std::cout << "saving results to: ";
    std::cout << filename.toStdString() << std::endl;

    // this matches: 2008 Jun 09 19:47:05
    QRegExp rxDateTime("^([12][0-9][0-9][0-9] ... [0-3][0-9] "
                       "[0-2][0-9]:[0-5][0-9]:[0-5][0-9]) ");

    QDateTime bt, ct, et(QDate(2999,1,1),QTime(0,0,0));

    // Find datetime stamp in the new calibration entry.
    QString qstr(contents.c_str());
    QStringList qsl = qstr.split("\n");
    qsl.removeLast();
    if (rxDateTime.indexIn( qsl[qsl.size()-1] ) == -1)
        return;
    ct = QDateTime::fromString(rxDateTime.cap(1), "yyyy MMM dd HH:mm:ss");

    // Open the selected calfile (it's ok if it doesn't exist yet).
    std::ifstream calfile ( filename.toStdString().c_str() );

    // Open a unique temporary file.
    char tmpfilename[] = "/tmp/EditCalDialog_XXXXXX";
    int tmpfile = mkstemp(tmpfilename);
    if (tmpfile == -1) {
        if (calfile) calfile.close();
        std::ostringstream ostr;
        ostr << tr("failed to open temporary file:\n").toStdString();
        ostr << tmpfilename << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }
    // Read the selected calfile and look for a place, chronologically, to insert
    // the new calibration entry.
    std::ostringstream buffer;
    std::string line;
    while ( std::getline(calfile, line) ) {
        buffer << line << std::endl;

        // If successfully inserted then dump remainder of calfile into tmpfile.
        if ( ct == et) {
            write(tmpfile, buffer.str().c_str(), buffer.str().length());
            buffer.str("");
            continue;
        }
        // Find datetime stamp from the latest line in the buffer.
        if (rxDateTime.indexIn( QString(line.c_str()) ) == -1)
            continue;

        bt = QDateTime::fromString(rxDateTime.cap(1), "yyyy MMM dd HH:mm:ss");
        if ( ct < bt) {
            ct = et;
            write(tmpfile, contents.c_str(), contents.length());
        }
        write(tmpfile, buffer.str().c_str(), buffer.str().length());
        buffer.str("");
    }
    // Insert the contents if not already done yet.
    if ( ct != et)
        write(tmpfile, contents.c_str(), contents.length());

    if (calfile) calfile.close();
    ::close(tmpfile);

    chmod(tmpfilename, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

    // Replace calfile with tmpfile.
    QFile::remove(filename);
    if ( !QFile::rename(QString(tmpfilename), filename) ) {
        std::ostringstream ostr;
        ostr << tr("failed to create cal file:\n").toStdString();
        ostr << filename.toStdString() << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }
    // mark what's exported
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList) {
        QString status = modelData(rowIndex.row(), col["status"]);
        status[statfi['E']] = 'E';

        proxyModel->setData(proxyModel->index(rowIndex.row(), col["status"]),
                        status);
    }
    changeDetected = true;
    exportUsed = true;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::exportCsvFile(QString filename, std::string contents)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    
    // provide a dialog for the user to select what to save as...
    filename = QFileDialog::getSaveFileName(this, tr("Save File"),
                 filename, tr("CSV files (*.dat)"));
                 
    if (filename.isEmpty()) return;
    
    std::cout << "Writing results to: ";
    std::cout << filename.toStdString() << std::endl;
    std::cout << contents << std::endl; 
    
    int fd = ::open( filename.toStdString().c_str(),
                     O_WRONLY | O_CREAT, 0664);
                     
    if (fd == -1) {  
        std::ostringstream ostr;
        ostr << tr("failed to save results to:\n").toStdString();
        ostr << filename.toStdString() << std::endl;
        ostr << tr(strerror(errno)).toStdString();
        QMessageBox::warning(0, tr("error"), ostr.str().c_str());
        return;
    }   
    write(fd, contents.c_str(), contents.length());
    ::close(fd);
}   

/* -------------------------------------------------------------------- */

void EditCalDialog::cloneButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    int row = _table->selectionModel()->currentIndex().row();

    // extract the site of the instrument from the current row
    QRegExp rxSite("(.*)[-_]");
    QString rid = modelData(row, col["rid"]);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);

    // set clone's parent ID
    QString pid           = rid;

    // set clone's new child ID
    QSqlQuery query(_calibDB);
    QString cmd("SELECT to_char(nextval('" + site + "_rid'),'\"" + site + "_\"FM00000000')");
    if (query.exec(cmd.toStdString().c_str()) == false ||
        query.first() == false) {
        QMessageBox::warning(0, tr("error"),
          tr("Failed to obtain next id!"));
        return;
    }
    rid = query.value(0).toString();

    // copy data from parent row
    QString status        = modelData(row, col["status"]);
    QDateTime cal_date    = proxyModel->index(row, col["cal_date"]).data().toDateTime();
    QString project_name  = modelData(row, col["project_name"]);
    QString username      = modelData(row, col["username"]);
    QString sensor_type   = modelData(row, col["sensor_type"]);
    QString serial_number = modelData(row, col["serial_number"]);
    QString var_name      = modelData(row, col["var_name"]);
    QString dsm_name      = modelData(row, col["dsm_name"]);
    QString cal_type      = modelData(row, col["cal_type"]);
    QString channel       = modelData(row, col["channel"]);
    QString gainbplr      = modelData(row, col["gainbplr"]);
    QString ads_file_name = modelData(row, col["ads_file_name"]);
    QString set_times     = modelData(row, col["set_times"]);
    QString set_points    = modelData(row, col["set_points"]);
    QString averages      = modelData(row, col["averages"]);
    QString stddevs       = modelData(row, col["stddevs"]);
    QString cal           = modelData(row, col["cal"]);
    QString temperature   = modelData(row, col["temperature"]);
    QString comment       = modelData(row, col["comment"]);

    // advance the clone's timestamp to be one second past the parent's
    cal_date = cal_date.addSecs(1);

    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;
    // create a new row
    int newRow = 0;
    _model->insertRows(newRow, 1);
    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;

    // paste the parent's data into its clone
    _model->setData(_model->index(newRow, col["rid"]),           rid);
    _model->setData(_model->index(newRow, col["pid"]),           pid);
    _model->setData(_model->index(newRow, col["status"]),        status);
    _model->setData(_model->index(newRow, col["cal_date"]),      cal_date);
    _model->setData(_model->index(newRow, col["project_name"]),  project_name);
    _model->setData(_model->index(newRow, col["username"]),      username);
    _model->setData(_model->index(newRow, col["sensor_type"]),   sensor_type);
    _model->setData(_model->index(newRow, col["serial_number"]), serial_number);
    _model->setData(_model->index(newRow, col["var_name"]),      var_name);
    _model->setData(_model->index(newRow, col["dsm_name"]),      dsm_name);
    _model->setData(_model->index(newRow, col["cal_type"]),      cal_type);
    _model->setData(_model->index(newRow, col["channel"]),       channel);
    _model->setData(_model->index(newRow, col["gainbplr"]),      gainbplr);
    _model->setData(_model->index(newRow, col["ads_file_name"]), ads_file_name);
    _model->setData(_model->index(newRow, col["set_times"]),     set_times);
    _model->setData(_model->index(newRow, col["set_points"]),    set_points);
    _model->setData(_model->index(newRow, col["averages"]),      averages);
    _model->setData(_model->index(newRow, col["stddevs"]),       stddevs);
    _model->setData(_model->index(newRow, col["cal"]),           cal);
    _model->setData(_model->index(newRow, col["temperature"]),   temperature);
    _model->setData(_model->index(newRow, col["comment"]),       comment);

    // mark child as a clone
    status[statfi['C']] = 'c';
    _model->setData(_model->index(newRow, col["status"]), status);

    // mark parent as cloned
    status[statfi['C']] = 'C';
    proxyModel->setData(proxyModel->index(row, col["status"]), status);

    saveButtonClicked();

    // re-apply row filter
    hideRows();
}

/* -------------------------------------------------------------------- */

void EditCalDialog::removeButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QModelIndexList rowList = _table->selectionModel()->selectedRows();

    if (rowList.isEmpty()) return;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(0, tr("Delete"),
                                  tr("Remove selected row?"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No) return;

    // mark what's removed
    foreach (QModelIndex rowIndex, rowList) {
        QString status = modelData(rowIndex.row(), col["status"]);
        status[statfi['R']] = 'R';

        proxyModel->setData(proxyModel->index(rowIndex.row(), col["status"]),
                        status);
    }
    changeDetected = true;
}

/* -------------------------------------------------------------------- */

void EditCalDialog::changeFitButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    int row = _table->selectionModel()->currentIndex().row();

    QRegExp rxCSV("\\{(.*)\\}");

    // extract the cal coefficients from the selected row
    QString cal = modelData(row, col["cal"]);
    if (rxCSV.indexIn(cal) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("No cal found!"));
        return;
    }
    QStringList coeffList = rxCSV.cap(1).split(",");
    int degree = coeffList.size();

    bool ok;
    degree = QInputDialog::getInt(this, "",
               tr("Set Polynominal Order:"), degree, 2, MAX_ORDER, 1, &ok);

    // exit if no change or cancel is selected
    if (degree == coeffList.size() || !ok)
        return;

    // extract the averages from the selected row
    QString averages = modelData(row, col["averages"]);
    if (rxCSV.indexIn(averages) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("No averages found!"));
        return;
    }
    std::vector<double> x;
    foreach (QString average, rxCSV.cap(1).split(","))
        x.push_back( average.toDouble() );

    // extract the set_points from the selected row
    QString set_points = modelData(row, col["set_points"]);
    if (rxCSV.indexIn(set_points) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("No set_points found!"));
        return;
    }
    std::vector<double> y;
    foreach (QString setPoint, rxCSV.cap(1).split(","))
        y.push_back( setPoint.toDouble() );

    // exit if array sizes don't match
    if (x.size() != y.size()) {
        QMessageBox::warning(0, tr("error"),
          tr("sizes of 'averages' and 'set_points' arrays don't match!"));
        return;
    }

    double coeff[MAX_ORDER];

    polynomialfit(x.size(), degree, &x[0], &y[0], coeff);

    std::stringstream cals;
    cals << "{";
    for(int i=0; i < degree; i++) {
        cals << coeff[i];
        if (i < degree - 1)
            cals << ",";
    }
    cals << "}";

    std::cout << "old cal: " << cal.toStdString() << std::endl;
    std::cout << "new cal: " << cals.str() << std::endl;

    // change cal data in the model
    proxyModel->setData(proxyModel->index(row, col["cal"]),
                        QString(cals.str().c_str()));
}
