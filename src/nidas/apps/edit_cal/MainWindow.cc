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
#include "MainWindow.h"
#include "ViewTextDialog.h"
#include "polyfitgsl.h"

#include <QtGui/QMenuBar>
#include <QtGui/QMenu>

#include <QtGui/QMessageBox>
#include <QtGui/QHeaderView>
#include <QtSql/QSqlTableModel>
#include <QtSql/QSqlError>
#include <QSqlDatabase>

#include <QTextStream>
#include <QFileDialog>
#include <QDir>

#include <QDateTime>
#include <QSplitter>
#include <QSqlQuery>
#include <QInputDialog>

#include <QHostInfo>
#include <QProcess>
#include <QRegExp>

#include <qwt_plot.h>
#include <qwt_legend.h>
#include <qwt_symbol.h>
//#include <qwt_plot_curve.h>
#include <qwt_plot_zoomer.h>

#include <QtCore/QList>
#include "PolyEval.h"

#define SANDBOX

#ifdef SANDBOX
#define DB_HOST    "ruttles.eol.ucar.edu"
#define DB_TABLE   "sandbox"
#endif

namespace n_u = nidas::util;

/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
#include <QItemDelegate>

class BackgroundColorDelegate: public QItemDelegate
{
public:
    BackgroundColorDelegate(QSqlTableModel *model, QObject *parent = 0)
      : QItemDelegate(parent)
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        _model = model;
    }

public:
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        drawBackground(painter, option, index);
        QItemDelegate::paint(painter, option, index);
    }

    void highlightRow(QString rid, QColor color)
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
        std::cout << "rid: " << rid.toStdString();
        std::cout << " color: " << color.red() << " " << color.green() << " " << color.blue() << std::endl;
        m_colorMap[rid] = color;
    }
    void unhighlightRow(QString rid)
    {
        std::cout << "rid: " << rid.toStdString() << std::endl;
        m_colorMap.remove(rid);
    }

protected:
    void drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QString rid = _model->index(index.row(), 0).data().toString().trimmed();
        if (m_colorMap.contains(rid))
            painter->fillRect(option.rect, m_colorMap.value(rid));
    }
private:
    QSqlTableModel *_model;

    /// model's row id (rid) to color mapping; temporary model data. 
    QMap<QString, QColor> m_colorMap;
};
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */

MainWindow::MainWindow() : changeDetected(false)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    setupDatabase();

    setupModels();

    setupDelegates();

    setupTable();

    setupViews();

    setupMenus();

    setWindowTitle(tr("Edit Calibration Database"));
    resize(870, 550);

    hideRows();
}

/* -------------------------------------------------------------------- */

void MainWindow::setupDatabase()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // Ping the master DB server to see if it is active.
    QProcess process;
    QStringList params;
    params << DB_HOST << "-i" << "1" << "-w" << "1" <<"-c" << "1";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("pinging calibration database"),
          tr("cannot contact:\n") + DB_HOST);
        exit(1);
    }
    // List of remote sites that fill a calibration database.
    QStringList siteList;
    siteList << "thumper.eol.ucar.edu"
             << "hyper.guest.ucar.edu"
             << "hercules.guest.ucar.edu";

    tailNum["thumper.eol.ucar.edu"]    = "N600";
    tailNum["hyper.guest.ucar.edu"]    = "N677F";
    tailNum["hercules.guest.ucar.edu"] = "N130AR";

    tailNumIdx[0] = "N600";
    tailNumIdx[1] = "N677F";
    tailNumIdx[2] = "N130AR";

    // define character locations of the status flags
    statfi['C'] = 0;
    statfi['R'] = 1;
    statfi['E'] = 2;

    // deny editing local calibration databases
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

#ifdef SANDBOX
    // point sandboxed version to someplace benign
    calfile_dir.setText( QString::fromAscii(getenv("HOME")) + "/");
    csvfile_dir.setText( QString::fromAscii(getenv("HOME")) + "/");
#endif

    std::cout << "calfile_dir: " << calfile_dir.text().toStdString() << std::endl;
    std::cout << "csvfile_dir: " << csvfile_dir.text().toStdString() << std::endl;

    // prompt user if they want to pull data from the sites at start up
    QMessageBox msgBox(QMessageBox::Question, tr("Pull"),
      tr("Pull calibration databases from the sites?\n"),
      QMessageBox::Yes | QMessageBox::No, this);
    QCheckBox showResults(tr("Show Results"), &msgBox);
    showResults.blockSignals(true);
    msgBox.addButton(&showResults, QMessageBox::ActionRole);

    QMessageBox::StandardButton reply = (QMessageBox::StandardButton)msgBox.exec();

    bool sR = (showResults.checkState() == Qt::Checked);

    if (reply == QMessageBox::Yes)
        foreach(QString site, siteList)
            importRemoteCalibTable(site, sR);

    openDatabase(DB_HOST);
}

/* -------------------------------------------------------------------- */

void MainWindow::setupModels()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    _model = new QSqlTableModel;

    // detect changes to model
    connect(_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
            this,     SLOT(dataChanged(const QModelIndex&, const QModelIndex&)));

    _model->setTable(DB_TABLE);
    _model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    _model->select();

    int c = 0;
    _model->setHeaderData(c++, Qt::Horizontal, tr("Row Id"));        // rid
    _model->setHeaderData(c++, Qt::Horizontal, tr("Parent Id"));     // pid
    _model->setHeaderData(c++, Qt::Horizontal, tr("Status"));        // status
    _model->setHeaderData(c++, Qt::Horizontal, tr("Date"));          // cal_date
    _model->setHeaderData(c++, Qt::Horizontal, tr("Project"));       // project_name
    _model->setHeaderData(c++, Qt::Horizontal, tr("User"));          // username
    _model->setHeaderData(c++, Qt::Horizontal, tr("Sensor Type"));   // sensor_type
    _model->setHeaderData(c++, Qt::Horizontal, tr("Serial #"));      // serial_number
    _model->setHeaderData(c++, Qt::Horizontal, tr("Variable"));      // var_name
    _model->setHeaderData(c++, Qt::Horizontal, tr("DSM"));           // dsm_name
    _model->setHeaderData(c++, Qt::Horizontal, tr("Cal Type"));      // cal_type
    _model->setHeaderData(c++, Qt::Horizontal, tr("Channel"));       // channel
    _model->setHeaderData(c++, Qt::Horizontal, tr("GainBplr"));      // gainbplr
    _model->setHeaderData(c++, Qt::Horizontal, tr("ADS file name")); // ads_file_name
    _model->setHeaderData(c++, Qt::Horizontal, tr("Set Times"));     // set_times
    _model->setHeaderData(c++, Qt::Horizontal, tr("Set Points"));    // set_points
    _model->setHeaderData(c++, Qt::Horizontal, tr("Avg Values"));    // averages
    _model->setHeaderData(c++, Qt::Horizontal, tr("StdDev Values")); // stddevs
    _model->setHeaderData(c++, Qt::Horizontal, tr("Calibration"));   // cal
    _model->setHeaderData(c++, Qt::Horizontal, tr("Temperature"));   // temperature
    _model->setHeaderData(c++, Qt::Horizontal, tr("Comment"));       // comment

    _table = new QTableView;

    // disable editing of table
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);

/* TODO this doesn't show anything... yet the columns are still draggable.  why?  
    _table->setDragEnabled(true);
    _table->setAcceptDrops(true);
    _table->setDropIndicatorShown(true);
*/
    _table->setModel(_model); //YY
}

/* -------------------------------------------------------------------- */

void MainWindow::setupDelegates()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    _delegate = new BackgroundColorDelegate(_model);
    _table->setItemDelegate(_delegate);
}

/* -------------------------------------------------------------------- */

void MainWindow::setupTable()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    _table->setContextMenuPolicy( Qt::CustomContextMenu );

    connect(_table, SIGNAL( customContextMenuRequested( const QPoint & )),
            this,     SLOT( contextMenu( const QPoint & )));
}

/* -------------------------------------------------------------------- */

void MainWindow::setupViews()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    QSplitter *splitHorz = new QSplitter;
    QSplitter *splitVert = new QSplitter;

    splitHorz->setOrientation(Qt::Horizontal);
    splitVert->setOrientation(Qt::Vertical);

    QWidget *container = new QWidget;
    QVBoxLayout *layoutHorz = new QVBoxLayout;

    _plot = new CalibrationPlot(splitHorz);
    _form = new CalibrationForm(splitHorz);

    splitHorz->addWidget(_plot);
    splitHorz->addWidget(_form);

    layoutHorz->addWidget(splitHorz);
    container->setLayout(layoutHorz);

    splitVert->addWidget(container);
    splitVert->addWidget(_table);

    QVBoxLayout *layout = new QVBoxLayout;

    layout->addWidget(splitVert);

    QWidget *wnd = new QWidget;
    wnd->setLayout(layout);
    setCentralWidget(wnd);

//   _plot->setModel(_model); // not used
     _form->setModel(_model);

// TODO is this needed?
//   QItemSelectionModel *selectionModel = new QItemSelectionModel(_model);
//   _table->setSelectionModel(selectionModel);
//   _form->setSelectionModel( _table->selectionModel() );
//   _form->setSelectionModel(selectionModel);
//
    connect(_form, SIGNAL(delThisSetPoint(int ,int)),
            this,    SLOT(delThisSetPoint(int ,int)));

    connect(_form, SIGNAL(changeFitButtonClicked(int, int)),
            this,    SLOT(changeFitButtonClicked(int, int)));

    connect(_form, SIGNAL(replot(int)),
            this,    SLOT(replot(int)));

    for (int i=0; i < _model->columnCount(); i++)
        _table->resizeColumnToContents(i);

    QHeaderView *horizontalHeader = _table->horizontalHeader();
    horizontalHeader->setMovable(true);
    horizontalHeader->setClickable(true);
    horizontalHeader->setStretchLastSection(true);
    horizontalHeader->setResizeMode(QHeaderView::Interactive);
    horizontalHeader->setSortIndicator(clm_cal_date, Qt::DescendingOrder);

    std::cout << __PRETTY_FUNCTION__ << " 4" << std::endl;
    QHeaderView *verticalHeader = _table->verticalHeader();
    verticalHeader->setResizeMode(QHeaderView::Fixed);
    verticalHeader->hide();

    _table->setSortingEnabled(true);

    _table->adjustSize();

    _table->adjustSize();
    _table->show();
}

/* -------------------------------------------------------------------- */

MainWindow::~MainWindow()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    onQuit();

    delete _delegate;
    delete _table;
    delete _model;

    std::cout << __PRETTY_FUNCTION__ << " EXITING" << std::endl;
}

/* -------------------------------------------------------------------- */

void MainWindow::contextMenu( const QPoint &pos )
{
    // clear any multiple selections made by user
    _table->selectionModel()->clearSelection();

    // select the row
    int row = _table->indexAt(pos).row();
    _table->selectionModel()->select(_model->index(row, 0),
      QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // show the popup menu
    verticalMenu->exec( _table->mapToGlobal(pos) + QPoint(20,0) );
}

/* -------------------------------------------------------------------- */

void MainWindow::delThisSetPoint(int row, int index)
{
    std::cout << __PRETTY_FUNCTION__ << " row: " << row << " index: " << index << std::endl;

    QStringList list_set_times  = extractListFromBracedCSV(row, clm_set_times);
    QStringList list_set_points = extractListFromBracedCSV(row, clm_set_points);
    QStringList list_averages   = extractListFromBracedCSV(row, clm_averages);
    QStringList list_stddevs    = extractListFromBracedCSV(row, clm_stddevs);

    list_set_times.replace(index, "");
    list_set_points.replace(index, "");
    list_averages.replace(index, "");
    list_stddevs.replace(index, "");

    QString set_times  = "{" + list_set_times.join(",") + "}";
    QString set_points = "{" + list_set_points.join(",") + "}";
    QString averages   = "{" + list_averages.join(",") + "}";
    QString stddevs    = "{" + list_stddevs.join(",") + "}";

    std::cout << "set_times:  " << set_times.toStdString() << std::endl;
    std::cout << "set_points: " << set_points.toStdString() << std::endl;
    std::cout << "averages:   " << averages.toStdString() << std::endl;
    std::cout << "stddevs:    " << stddevs.toStdString() << std::endl;

    _model->setData(_model->index(row, clm_set_times),     set_times);
    _model->setData(_model->index(row, clm_set_points),    set_points);
    _model->setData(_model->index(row, clm_averages),      averages);
    _model->setData(_model->index(row, clm_stddevs),       stddevs);
}

/* -------------------------------------------------------------------- */

void MainWindow::replot(int row)
{
    std::cout << __PRETTY_FUNCTION__ << " row: " << row << std::endl;
    unplotCalButtonClicked(row);
    plotCalButtonClicked(row);
}

/* -------------------------------------------------------------------- */

QAction *MainWindow::addRowAction(QMenu *menu, const QString &text,
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

QAction *MainWindow::addColAction(QMenu *menu, const QString &text,
                                     QActionGroup *group, QSignalMapper *mapper,
                                     int id, bool checked)
{
    _table->setColumnHidden(id, !checked);

    return addAction(menu, text, group, mapper, id, checked);
}

/* -------------------------------------------------------------------- */

QAction *MainWindow::addAction(QMenu *menu, const QString &text,
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

inline QString MainWindow::modelData(int row, int col)
{
    return _model->index(row, col).data().toString().trimmed();
}

/* -------------------------------------------------------------------- */

QStringList MainWindow::extractListFromBracedCSV(int row, calTableHeaders key)
{
    QString string = modelData(row, key);
    return extractListFromBracedCSV(string);
}

/* -------------------------------------------------------------------- */

QStringList MainWindow::extractListFromBracedCSV(QString string)
{
    QRegExp rxCSV("\\{(.*)\\}");
    QStringList list;

    if (rxCSV.indexIn(string) != -1)
        list = rxCSV.cap(1).split(",");

    return list;
}

/* -------------------------------------------------------------------- */

void MainWindow::toggleRow(int id)
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

void MainWindow::hideRows()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QRegExp rxSite("(.*)_");

    for (int row = 0; row < _model->rowCount(); row++) {

        QString status = modelData(row, clm_status);
        QString cal_type = modelData(row, clm_cal_type);
        QString rid = modelData(row, clm_rid);
        if (rxSite.indexIn(rid) == -1) {
            // TODO show entire row of data here in message box.
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

void MainWindow::toggleColumn(int id)
{
    _table->setColumnHidden(id, !_table->isColumnHidden(id));
    _table->resizeColumnToContents(id);

    // reduce the size of some of the larger columns
    if (id == clm_comment)
        _table->setColumnWidth(id, 300);
}

/* -------------------------------------------------------------------- */

void MainWindow::setupMenus()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // Popup menu setup... (cannot use keyboard shortcuts here)
    verticalMenu = new QMenu;
    verticalMenu->addAction(tr("Edit this Cal"),             this, SLOT(editCalButtonClicked()));
    verticalMenu->addAction(tr("Plot this Cal"),             this, SLOT(plotCalButtonClicked()));
    verticalMenu->addAction(tr("Unplot this Cal"),           this, SLOT(unplotCalButtonClicked()));
    verticalMenu->addAction(tr("Export to Cal File"),        this, SLOT(exportCalButtonClicked()));
    verticalMenu->addAction(tr("Export to CSV File"),        this, SLOT(exportCsvButtonClicked()));
    verticalMenu->addAction(tr("View Cal File"),             this, SLOT(viewCalButtonClicked()));
    verticalMenu->addAction(tr("View CSV File"),             this, SLOT(viewCsvButtonClicked()));
    verticalMenu->addAction(tr("Clone this Entry"),          this, SLOT(cloneButtonClicked()));
    verticalMenu->addAction(tr("Delete this Entry"),         this, SLOT(removeButtonClicked()));
    verticalMenu->addAction(tr("Change Polynominal Fit..."), this, SLOT(changeFitButtonClicked()));

    // File menu setup...
    QMenu *fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(tr("&Save"), this, SLOT(saveButtonClicked()), Qt::CTRL + Qt::Key_S);
    fileMenu->addAction(tr("&Quit"), this, SLOT(onQuit()),            Qt::CTRL + Qt::Key_Q);
//  connect(this, SIGNAL(aboutToQuit()), this, SLOT(onQuit()));
    menuBar()->addMenu(fileMenu);

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
    addRowAction(rowsMenu, tr("cloned"),        rowsGrp, rowsMapper, i++, true);
    addRowAction(rowsMenu, tr("removed"),       rowsGrp, rowsMapper, i++, false);
    addRowAction(rowsMenu, tr("exported"),      rowsGrp, rowsMapper, i++, true);
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
    addColAction(colsMenu, tr("Set Times"),     colsGrp, colsMapper, i++, false); ///set_times
    addColAction(colsMenu, tr("Set Points"),    colsGrp, colsMapper, i++, false); ///set_points
    addColAction(colsMenu, tr("Avg Values"),    colsGrp, colsMapper, i++, false); ///averages
    addColAction(colsMenu, tr("StdDev Values"), colsGrp, colsMapper, i++, false); ///stddevs
    addColAction(colsMenu, tr("Calibration"),   colsGrp, colsMapper, i++, true);  // cal
    addColAction(colsMenu, tr("Temperature"),   colsGrp, colsMapper, i++, false); // temperature
    addColAction(colsMenu, tr("Comment"),       colsGrp, colsMapper, i++, false); // comment

    viewMenu->addMenu(colsMenu);

    menuBar()->addMenu(viewMenu);
}

/* -------------------------------------------------------------------- */

bool MainWindow::openDatabase(QString hostname)
{
    std::cout << __PRETTY_FUNCTION__ << " hostname: " << hostname.toStdString() << std::endl;

    QSqlDatabase::database("qt_sql_default_connection", false).close();
    QSqlDatabase::removeDatabase("qt_sql_default_connection");

    foreach( QString driver, QSqlDatabase::drivers() )
        std::cout << "driver: " << driver.toStdString() << std::endl;

    // create the default database connection
    QSqlDatabase db = QSqlDatabase::addDatabase(DB_DRIVER); // GRR , DB_NAME);
//  QSqlDatabase db = QSqlDatabase::database(DB_DRIVER); // GRR , DB_NAME);
/*
    foreach( QString connectionName, QSqlDatabase::connectionNames() )
        std::cout << "aaaa connectionName: " << connectionName.toStdString() << std::endl;

    if (!db.isValid())
    {
        std::ostringstream ostr;
        ostr << tr("Unsupported database driver: ").toStdString();
        ostr << DB_DRIVER;

        std::cerr << ostr.str() << std::endl;
        QMessageBox::critical(0, tr("connect"), ostr.str().c_str());
        return false;
    }
*/
    db.setHostName(hostname);
    db.setDatabaseName(DB_NAME);
    db.setUserName(DB_USER);
    foreach( QString connectionName, QSqlDatabase::connectionNames() )
        std::cout << "bbbb connectionName: " << connectionName.toStdString() << std::endl;

    if (!db.open())
    {
        std::ostringstream ostr;
        ostr << tr("Failed to open calibration database.\n\n").toStdString();
        ostr << tr(db.lastError().text().toAscii().data()).toStdString();

        std::cerr << ostr.str() << std::endl;
        QMessageBox::critical(0, tr("open"), ostr.str().c_str());

        std::cout << __PRETTY_FUNCTION__ << " FAILED" << std::endl;
        return false;
    }
    std::cout << __PRETTY_FUNCTION__ << " SUCCESS" << std::endl;
    return true;
}

/* -------------------------------------------------------------------- */

void MainWindow::dataChanged(const QModelIndex& old, const QModelIndex& now)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    changeDetected = true;
}

/* -------------------------------------------------------------------- */

void MainWindow::onQuit()
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
    close();
}

/* -------------------------------------------------------------------- */

void MainWindow::importRemoteCalibTable(QString remote, bool showResults)
{
    std::cout << __PRETTY_FUNCTION__ << " remote: " << remote.toStdString() << std::endl;
    return;
    QProcess process;
    QStringList params;
    QString progress;
    QString remoteCalSql;
    QString cmd, lastCalDate("1970-01-01 00:00:00.00");
    QSqlQuery queryMaster(QSqlDatabase::database());
    QSqlQuery queryRemote(QSqlDatabase::database());
    QFileInfo olbpg("/opt/local/bin/pg_dump");  // network version
    QFileInfo ubpg("/usr/bin/pg_dump");         // local version
    std::string pg_dump_exec;
    std::stringstream pg_dump;

    QString data_dir = QString::fromAscii(getenv("DATA_DIR")) + "/databases/";
    bool eee, fff;

    // Dump the remote's calibration database to a directory that is
    // regularly backed up by CIT.
    remoteCalSql = data_dir + remote + "_cal.sql";

    // Ping the remote DB server to see if it is active.
    params << remote << "-i" << "1" << "-w" << "1" <<"-c" << "1";
    progress += "ping " + params.join(" ") + "\n";

    if (process.execute("ping", params)) {
        QMessageBox::information(0, tr("pinging calibration database"),
          tr("cannot contact:\n") + remote);
        goto showResults;
    }
    // Delete the previous '..._cal.sql' file
    progress += "deleting older sql file\n";
    cmd = "/bin/rm -f " + remoteCalSql;
    progress += cmd + "\n";
    if (system(cmd.toStdString().c_str())) {
        QMessageBox::information(0, tr("deleting older sql file"),
          tr("cannot delete:\n") + remoteCalSql);
        goto showResults;
    }   
    // Obtain the latest cal_date from the master DB.
    openDatabase(DB_HOST);
    cmd = "SELECT MAX(cal_date) FROM " DB_TABLE " WHERE "
          "pid='' AND rid ~* '^" + tailNum[remote] + "_'";
    progress += cmd + "\n";
    std::cout << "cmd: " << cmd.toStdString().c_str() << "\n";
    eee = queryMaster.exec(cmd.toStdString().c_str());  fff = queryMaster.first();
    std::cout << "eee: " << eee << " fff: " << fff << "\n";
//  if (queryMaster.exec(cmd) && queryMaster.first()) {
    if (eee && fff) {
        lastCalDate = queryMaster.value(0).toString();
        std::cout << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n";
    }
    else
        std::cout << "------------------------------------------------\n";
    progress += lastCalDate + " " + tailNum[remote] + "\n";
//  QSqlDatabase::database().close();

    // Build a temporary table of the newer rows in the remote DB.
    openDatabase(remote);
    queryRemote.exec("DROP TABLE imported");
    queryRemote.exec("CREATE TABLE imported (LIKE " DB_TABLE ")");
    queryRemote.exec("INSERT INTO imported SELECT * FROM " DB_TABLE
                     " WHERE cal_date > '" + lastCalDate + "'");
//  QSqlDatabase::database().close();

    // Use "...to_char(nextval(..." to ensure that new rid(s) are created 
    // in the master database.
    cmd = " | sed \"s/VALUES ('" + tailNum[remote] + \
          "_........', /VALUES (to_char(nextval('" + \
          tailNum[remote] + "_rid'),'\"" + \
          tailNum[remote] + "_\"FM00000000'), /\"";

    // test to see if the latest pg_dump is available on the network
    // otherwise use the locally installed version
    if ( olbpg.isReadable() && olbpg.isExecutable() )
        pg_dump_exec = olbpg.absoluteFilePath().toStdString();
    else if ( ubpg.isReadable() && ubpg.isExecutable() )
        pg_dump_exec = ubpg.absoluteFilePath().toStdString();
    else {
        QMessageBox::warning(0, tr("error"),
          "'pg_dump' " + tr("not installed!"));
        goto showResults;
    }
    // The dump is filtered to just the INSERT commands.
    pg_dump << pg_dump_exec << " --insert -h " << remote.toStdString()
            << " -U " << DB_USER
            << " " << DB_NAME << " -t imported"
            << " | grep INSERT"
            << " | sed 's/INSERT INTO imported VALUES /INSERT INTO " DB_TABLE " VALUES /'"
            << cmd.toStdString()
            << " > " << remoteCalSql.toStdString();

    progress += QString(pg_dump.str().c_str()) + "\n";
    if (system(pg_dump.str().c_str())) {
        QMessageBox::information(0, tr("dumping calibration database"),
          tr("cannot contact:\n") + remote);
        goto showResults;
    }
    // Insert the remote's calibration database into the master's.
    params.clear();
    params << "-h" << DB_HOST << "-U" << DB_USER << "-d" << DB_NAME;
    params << "-f" << remoteCalSql;

    progress += "psql " + params.join(" ") + "\n";

/*
    if (process.execute("psql", params)) {
        QMessageBox::information(0, tr("importing remote calibration database"),
          tr("psql command failed"));
    }
*/

showResults:
    progress += "\n\n" + remoteCalSql + ":\n";
    if (showResults)
        viewFile(remoteCalSql, "Imported Sql Results", progress);
}

/* -------------------------------------------------------------------- */

int MainWindow::saveButtonClicked()
{
    // TODO revert initial settings of the auto incremented counters on error?
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    int ret = 0;

    // get selected index and row number
    QModelIndex index = _table->selectionModel()->currentIndex();
    int row = index.row();
    std::cout << "row: " << row << std::endl;

    if (_model->submitAll()) {

        // calibration database successfully updated
        changeDetected = false;
    } else {
        QString lastError = _model->lastError().text();
        QSqlDatabase::database().rollback();
        QMessageBox::warning(0, tr("save"),
           tr("The database reported an error: %1") .arg(lastError));
        ret = 1;
    }
    // re-select last clicked row after saving the database
    _table->scrollTo(index, QAbstractItemView::PositionAtCenter);
    _table->selectRow(row);
    return ret;
}

/* -------------------------------------------------------------------- */

void MainWindow::editCalButtonClicked()
{
    // populate the form view with elements from this row
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    // set the form's data widget mapper to the selected row
    _form->setRow(row);

    // set the plot's data widget mapper to the selected row
//  _plot->setRow(row);  // TODO have the plot react to changes made by form

    // get results from previous calibration for this variable (if any)
    QString site     = modelData(row, clm_rid).split("_").at(0);
    QString var_name = modelData(row, clm_var_name);
    QString cal_date = modelData(row, clm_cal_date);

    std::cout << "site: " <<  site.toStdString() << std::endl;
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;
    std::cout << "cal_date: " <<  cal_date.toStdString() << std::endl;

    QSqlQuery query(QSqlDatabase::database());
    QString cmd("SELECT cal FROM " DB_TABLE " WHERE rid~'" + site +
                "' AND var_name='" + var_name + "' AND cal_date<'" + cal_date +
                "' ORDER BY cal_date DESC LIMIT 1");

    std::cout << cmd.toStdString() << std::endl;

    QString prevString;
    if (query.exec(cmd) == false ||
        query.first() == false) {
//      QMessageBox::warning(0, tr("notice"),
//        tr("No previous cal found!"));
    }
    else
        prevString = query.value(0).toString();

    QStringList list_set_times  = extractListFromBracedCSV(row, clm_set_times);
    QStringList list_set_points = extractListFromBracedCSV(row, clm_set_points);
    QStringList list_averages   = extractListFromBracedCSV(row, clm_averages);
    QStringList list_stddevs    = extractListFromBracedCSV(row, clm_stddevs);
    QStringList list_cal        = extractListFromBracedCSV(row, clm_cal);
    QStringList list_old        = extractListFromBracedCSV( prevString );

    int i = 0;
    QStringListIterator iC(list_cal);
    std::vector<double> _cals;
    while (iC.hasNext()) {
        QString coeff = iC.next();
        double iCd = coeff.toDouble();
        _cals.push_back(iCd);
        _form->_currCalCList[i]->setText(   coeff);
        i++;
    }
    QList<QAbstractButton *> curveFitButtons = _form->_curveFitGroup->buttons();
    foreach (QAbstractButton *button, curveFitButtons)
        button->setChecked(false);

    curveFitButtons.at(i-2)->setChecked(true);

    for (; i<4; i++)
        _form->_currCalCList[i]->setText(   "");

    i = 0;
    QStringListIterator iO(list_old);
    while (iO.hasNext()) {
        _form->_prevCalCList[i]->setText(   iO.next());
        i++;
    }
    for (; i<4; i++)
        _form->_prevCalCList[i]->setText(   "");

    QList<QAbstractButton *> delThisButtons = _form->_delThisGroup->buttons();

    i = 0;
    QStringListIterator iT(list_set_times);
    QStringListIterator iP(list_set_points);
    QStringListIterator iA(list_averages);
    QStringListIterator iD(list_stddevs);
    while (iP.hasNext())
    {
        // skip over removed set points
        QString sp = iP.next();
        if ( sp.isEmpty() ) {
            iT.next();
            iA.next();
            iD.next();
            _form->_setDateTimeList[i]->setText("");
            _form->_setPointList[i]->setText(   "");
            _form->_newVList[i]->setText(       "");
            _form->_new_sdList[i]->setText(     "");
            _form->_appliedList[i]->setText(    "");
            delThisButtons.at(i)->setEnabled(false);
            i++;
            continue;
        }
        // mitigate old data that predates the use of set_times
        QString dt;
        if (list_set_times.count() > 1)
            dt = iT.next();

        // calculate applied value
        QString average = iA.next();
        double voltage = average.toDouble();
        QString applied = QString::number(numeric::PolyEval(_cals, voltage));

        _form->_setDateTimeList[i]->setText(dt.remove(QChar('"')));
        _form->_setPointList[i]->setText(   sp);
        _form->_newVList[i]->setText(       average);
        _form->_new_sdList[i]->setText(     iD.next());
        _form->_appliedList[i]->setText(    applied);
        delThisButtons.at(i)->setEnabled(true);
        i++;
    }
    for (; i<9; i++) {
        _form->_setDateTimeList[i]->setText("");
        _form->_setPointList[i]->setText(   "");
        _form->_newVList[i]->setText(       "");
        _form->_new_sdList[i]->setText(     "");
        _form->_appliedList[i]->setText(    "");
        delThisButtons.at(i)->setEnabled(false);
    }
    _form->  _platformTxt->setText( site );
    _form->      _projTxt->setCurrentIndex( _form->      _projTxt->findText( modelData(row, clm_project_name ) ) );
    _form->      _userTxt->setCurrentIndex( _form->      _userTxt->findText( modelData(row, clm_username ) ) );
    _form->_sensorTypeTxt->setCurrentIndex( _form->_sensorTypeTxt->findText( modelData(row, clm_sensor_type ) ) );
    _form-> _serialNumTxt->setCurrentIndex( _form-> _serialNumTxt->findText( modelData(row, clm_serial_number ) ) );
    _form->       _dsmTxt->setCurrentIndex( _form->       _dsmTxt->findText( modelData(row, clm_dsm_name ) ) );
    _form->   _calTypeTxt->setCurrentIndex( _form->   _calTypeTxt->findText( modelData(row, clm_cal_type ) ) );
    _form->      _addrTxt->setCurrentIndex( _form->      _addrTxt->findText( modelData(row, clm_channel ) ) );
    _form->  _gainbplrTxt->setCurrentIndex( _form->  _gainbplrTxt->findText( modelData(row, clm_gainbplr ) ) );

    QString pid = modelData(row, clm_pid);
    if (pid.length()) {
        // edit the cloned entry in the form view

        _form->setEnabled(true);
    } else {
        QMessageBox::warning(0, tr("edit"),
           tr("Cannot edit content, clone it first then edit that line."));

        _form->setEnabled(false);
    }
}

/* -------------------------------------------------------------------- */

void MainWindow::plotCalButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();
    plotCalButtonClicked(row);
}

/* -------------------------------------------------------------------- */

void MainWindow::plotCalButtonClicked(int row)
{
    std::cout << __PRETTY_FUNCTION__ << " row: " << row << std::endl;

    // don't plot if already plotted
    QString rid = modelData(row, clm_rid);
    foreach (CalibrationCurve *curve, plottedCurves)
        if (curve->rid == rid) return;

    QString label = modelData(row, clm_var_name) + " " + modelData(row, clm_cal_date);
    std::cout << "label: " << label.toStdString() << std::endl;

    QwtLegend *legend = new QwtLegend;
    legend->setItemMode(QwtLegend::CheckableItem);
    _plot->qwtPlot->insertLegend(legend, QwtPlot::RightLegend);

    QStringList list_set_points = extractListFromBracedCSV(row, clm_set_points);
    if (list_set_points.isEmpty()) return;
    QStringList list_averages   = extractListFromBracedCSV(row, clm_averages);
    if (list_averages.isEmpty()) return;
    QStringList list_cal        = extractListFromBracedCSV(row, clm_cal);
    if (list_cal.isEmpty()) return;

    // fetch a new color to plot in
    if ( _plot->colors.empty()) {
        QMessageBox::information(0, tr("notice"),
          tr("Ran out of unique colors to plot in!\nCurrently only 12 colors are specified."));
        return;
    }
    QColor color = _plot->colors.front();
    std::cout << "pull color: " << color.red() << " " << color.green() << " " << color.blue() << std::endl;
    _plot->colors.pop_front();

    QColor actColor = color;
    QColor fitColor = color.lighter(150);
    QColor mrkColor = color.darker(150);

    std::cout << "actColor: " << actColor.red() << " " << actColor.green() << " " << actColor.blue() << std::endl;
    std::cout << "fitColor: " << fitColor.red() << " " << fitColor.green() << " " << fitColor.blue() << std::endl;
    std::cout << "mrkColor: " << mrkColor.red() << " " << mrkColor.green() << " " << mrkColor.blue() << std::endl;

    // highlight the row in the table to match the color
    _delegate->highlightRow(rid, actColor);

    QStringListIterator iC(list_cal);
    std::vector<double> _cals;
    while (iC.hasNext())
    {
        double iCd = iC.next().toDouble();
        std::cout << "coeff: " << iCd << std::endl;
        _cals.push_back(iCd);
    }
    // Run 80 points from -10 to 10 Vdc.
    int nPoints = 80;
/*
    double step = 20.0 / nPoints, vdc = -10.0;
*/
    QStringListIterator iX(list_averages);
    float max = -9999.9; 
    float min =  9999.9; 
    std::cout << "min: " << min << " max: " << max << std::endl;
    while (iX.hasNext()) {
        QString iPs = iX.next();
        if (!iPs.isEmpty()) {
            float iPf = iPs.toFloat();
            std::cout << "val: " << iPf << std::endl;
            if (max < iPf) max = iPf;
            if (min > iPf) min = iPf;
        }
    }
    std::cout << "min: " << min << " max: " << max << std::endl;
    double step = (max-min) / nPoints, vdc = min;

    std::vector<double> x, y;
    x.resize(nPoints);
    y.resize(nPoints);

    for (int i = 0; i < nPoints; ++i)
    {
      x[i] = vdc;
      y[i] = numeric::PolyEval(_cals, x[i]);
      vdc += step;
    }
    
    QVector<QPointF>* actual = new QVector<QPointF>;

    QStringListIterator iA(list_averages);
    QStringListIterator iP(list_set_points);
    while (iA.hasNext() && iP.hasNext())
    {
        // skip over removed set points
        QString sp = iP.next();
        if ( sp.isEmpty() ) {
            iA.next();
            continue;
        }
        float iAf = iA.next().toFloat();
        float iPf = sp       .toFloat();
        std::cout << iAf << " " << iPf << std::endl;
        actual->push_back(QPointF(iAf, iPf));
    }
    QwtText actLabel("actual "+label);
    QwtText fitLabel("fitted "+label);

    actLabel.setColor(actColor);
    fitLabel.setColor(fitColor);

    CalibrationCurve* curve = new CalibrationCurve;
    curve->rid = rid;
    curve->actual = new QwtPlotCurve(actLabel);
    curve->fitted = new QwtPlotCurve(fitLabel);

    QPen actPen(actColor);  actPen.setWidth(1);
    curve->actual->setPen(actPen);

    QPen fitPen(fitColor);  fitPen.setWidth(1);
    curve->fitted->setPen(fitPen);

    curve->actual->setSymbol( new QwtSymbol(QwtSymbol::Cross,
                              mrkColor, mrkColor, QSize(5, 5)));

    QwtPointSeriesData* actData = new QwtPointSeriesData;
    actData->setSamples(*actual);

    curve->actual->setData(actData);
    curve->fitted->setSamples(&x[0], &y[0], nPoints);
    curve->actual->attach(_plot->qwtPlot);
    curve->fitted->attach(_plot->qwtPlot);
    plottedCurves.append(curve);

//  TODO implement zooming via rubberband selection
//  QRectF region = actData->boundingRect();
    QwtPlotZoomer *zoomer = new QwtPlotZoomer(_plot->qwtPlot->canvas());
    zoomer->zoom(0);
//  _plot->qwtPlot->replot();
}

/* -------------------------------------------------------------------- */

void MainWindow::unplotCalButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();
    unplotCalButtonClicked(row);
}

/* -------------------------------------------------------------------- */

void MainWindow::unplotCalButtonClicked(int row)
{
    std::cout << __PRETTY_FUNCTION__ << " row: " << row << std::endl;

    QString rid = modelData(row, clm_rid);
    std::cout << "selected rid: " << rid.toStdString() << std::endl;

    foreach (CalibrationCurve *curve, plottedCurves) {
        std::cout << "iterated rid: " << curve->rid.toStdString() << std::endl;
        if (curve->rid == rid) {
            QColor color = curve->actual->pen().color();
            std::cout << "push color: " << color.red() << " " << color.green() << " " << color.blue() << std::endl;
            _plot->colors.push_front(color);
            curve->actual->detach();
            curve->fitted->detach();
            plottedCurves.removeOne(curve);
        } 
    } 
    _plot->qwtPlot->replot();
    _delegate->unhighlightRow(rid);
}

/* -------------------------------------------------------------------- */

void MainWindow::exportCalButtonClicked()
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
        if (saveButtonClicked()) return;
    }
    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    // don't export anything that was removed
    QString status = modelData(row, clm_status);
    if (status[statfi['R']] == 'R') {
        QMessageBox::information(0, tr("notice"),
          tr("You cannot export a calibration from a removed row."));
        return;
    }
    // get the cal_type from the selected row
    QString cal_type = modelData(row, clm_cal_type);
    std::cout << "cal_type: " <<  cal_type.toStdString() << std::endl;

    if (cal_type == "instrument")
        exportInstrument(row);

    if (cal_type == "analog")
        exportAnalog(row);
}

/* -------------------------------------------------------------------- */

void MainWindow::exportCsvButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    QStringList list_set_points = extractListFromBracedCSV(row, clm_set_points);
    if (list_set_points.isEmpty()) return;
    QStringList list_averages   = extractListFromBracedCSV(row, clm_averages);
    if (list_averages.isEmpty()) return;

    std::ostringstream ostr;
    ostr << "setPoint,Average\n";

    QStringListIterator iP(list_set_points);
    QStringListIterator iA(list_averages);
    while (iP.hasNext() && iA.hasNext())
        ostr << iP.next().toStdString() << ","
             << iA.next().toStdString() << "\n";

    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, clm_rid);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);
    QString var_name = modelData(row, clm_var_name);

    QString filename = csvfile_dir.text();
    filename += site + "_" + var_name + ".csv";

    exportCsvFile(filename, ostr.str());
}

/* -------------------------------------------------------------------- */

void MainWindow::viewCalButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    // get the cal_type from the selected row
    QString cal_type = modelData(row, clm_cal_type);

    QString filename = calfile_dir.text();

    if (cal_type == "instrument") {
        QString var_name = modelData(row, clm_var_name);

        // extract the site of the instrument from the current row
        QRegExp rxSite("(.*)_");
        QString rid = modelData(row, clm_rid);
        if (rxSite.indexIn(rid) == -1) {
            QMessageBox::warning(0, tr("error"),
              tr("Site name (tail number) not found in 'rid'!"));
            return;
        }
        QString site = rxSite.cap(1);

#ifndef SANDBOX
        filename += QString("Engineering/") + site + "/";
#endif
        filename += var_name + ".dat";
    }
    else if (cal_type == "analog") {
        // extract the serial_number of the A2D card from the current row
        QString serial_number = modelData(row, clm_serial_number);

#ifndef SANDBOX
        filename += QString("A2D/");
#endif
        filename += "A2D" + serial_number + ".dat";
    }
    else 
        return;

    viewFile(filename, "Calibration File Viewer");
}

/* -------------------------------------------------------------------- */

void MainWindow::viewCsvButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // get selected row number
    int row = _table->selectionModel()->currentIndex().row();

    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, clm_rid);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);
    QString var_name = modelData(row, clm_var_name);

    QString filename = csvfile_dir.text();
    filename += site + "_" + var_name + ".csv";

    viewFile(filename, "CSV File Viewer");
}

/* -------------------------------------------------------------------- */

void MainWindow::viewFile(QString filename, QString title, QString prequel)
{
    std::cout << "filename: " <<  filename.toStdString() << std::endl;
    QFile file(filename);
    if (file.open(QFile::ReadOnly)) {
        QTextStream in(&file);
        const QString data = prequel + in.readAll();
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

void MainWindow::exportInstrument(int row)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    QString var_name = modelData(row, clm_var_name);
    std::cout << "var_name: " <<  var_name.toStdString() << std::endl;

    // extract the site of the instrument from the current row
    QRegExp rxSite("(.*)_");
    QString rid = modelData(row, clm_rid);
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
    cal_date = modelData(row, clm_cal_date).toStdString();
    ct = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");

    // extract the cal coefficients from the selected row
    QStringList list_cal        = extractListFromBracedCSV(row, clm_cal);
    if (list_cal.isEmpty()) return;

    // record results to the device's CalFile
    std::ostringstream ostr;
    ostr << std::endl;

    ostr << ct.format(true,"%Y %b %d %H:%M:%S");

    foreach (QString coeff, list_cal)
        ostr << " " << std::setw(9) << coeff.toStdString();

    ostr << std::endl;

    QString filename = calfile_dir.text();

#ifndef SANDBOX
    filename += QString("Engineering/") + site + "/";
#endif
    filename += var_name + ".dat";

    exportCalFile(filename, ostr.str());
}

/* -------------------------------------------------------------------- */

void MainWindow::exportAnalog(int row)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    // extract the serial_number of the A2D card from the current row
    QString serial_number = modelData(row, clm_serial_number);
    std::cout << "serial_number: " <<  serial_number.toStdString() << std::endl;

    QString gainbplr = modelData(row, clm_gainbplr);
    std::cout << "gainbplr: " <<  gainbplr.toStdString() << std::endl;

    // extract the cal_date from the current row
    std::string cal_date;
    n_u::UTime ut, ct;
    cal_date = modelData(row, clm_cal_date).toStdString();
    ct = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");

    // extract the cal coefficients from the selected row
    QString offst[8];
    QString slope[8];
    QRegExp rxCoeff2("\\{([+-]?\\d+\\.\\d+),([+-]?\\d+\\.\\d+)\\}");

    QString cal = modelData(row, clm_cal);
    if (rxCoeff2.indexIn(cal) == -1) {
        QMessageBox::information(0, tr("notice"),
          tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
          tr("'\n\nto export an analog calibration."));
        return;
    }
    int channel = modelData(row, clm_channel).toInt();
    offst[channel] = rxCoeff2.cap(1);
    slope[channel] = rxCoeff2.cap(2);
    int chnMask = 1 << channel;

    // search for the other channels and continue extracting coefficients...
    int topRow = row;
    do {
        if (--topRow < 0) break;
        QString status = modelData(topRow, clm_status);
        if (status[statfi['R']] == 'R') {
            QMessageBox::information(0, tr("notice"),
              tr("You cannot export a calibration with a removed row."));
            return;
        }
        if (serial_number != modelData(topRow, clm_serial_number)) break;
        if      ("analog" != modelData(topRow, clm_cal_type)) break;
        if      (gainbplr != modelData(topRow, clm_gainbplr)) break;

        cal_date = modelData(topRow, clm_cal_date).toStdString();
        ut = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << std::abs(double(ut.toSecs()-ct.toSecs()))
                  << " > " << 12*60*60 << std::endl;
        if (std::abs(double(ut.toSecs()-ct.toSecs())) > 12*60*60) break;

        cal = modelData(topRow, clm_cal);
        if (rxCoeff2.indexIn(cal) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(topRow, clm_channel).toInt();
        offst[channel] = rxCoeff2.cap(1);
        slope[channel] = rxCoeff2.cap(2);
        chnMask |= 1 << channel;
    } while (true);
    topRow++;

    int numRows = _model->rowCount() - 1;
    int btmRow = row;
    do {
        if (++btmRow > numRows) break;
        QString status = modelData(btmRow, clm_status);
        if (status[statfi['R']] == 'R') {
            QMessageBox::information(0, tr("notice"),
              tr("You cannot export a calibration with a removed row."));
            return;
        }
        if (serial_number != modelData(btmRow, clm_serial_number)) break;
        if      ("analog" != modelData(btmRow, clm_cal_type)) break;
        if      (gainbplr != modelData(btmRow, clm_gainbplr)) break;

        cal_date = modelData(btmRow, clm_cal_date).toStdString();
        ut = n_u::UTime::parse(true, cal_date, "%Y-%m-%dT%H:%M:%S");
        std::cout << "| " << ut << " - " << ct << " | = "
                  << std::abs(double(ut.toSecs()-ct.toSecs()))
                  << " > " << 12*60*60 << std::endl;
        if (std::abs(double(ut.toSecs()-ct.toSecs())) > 12*60*60) break;

        cal = modelData(btmRow, clm_cal);
        if (rxCoeff2.indexIn(cal) == -1) {
            QMessageBox::information(0, tr("notice"),
              tr("You must select a calibration matching\n\n'") + rxCoeff2.pattern() + 
              tr("'\n\nto export an analog calibration."));
            return;
        }
        channel = modelData(btmRow, clm_channel).toInt();
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
    QString temperature = modelData(btmRow, clm_temperature);

    // extract cal_date from channel 0
    int chn0idx = -1;
    QModelIndexList rowList = _table->selectionModel()->selectedRows();
    foreach (QModelIndex rowIndex, rowList) {
        chn0idx = rowIndex.row();
        if (modelData(chn0idx, clm_channel) == "0")
            break;
    }
    cal_date = modelData(chn0idx, clm_cal_date).toStdString();
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
    gainbplr_out["1T"] = "    1       1     ";
    gainbplr_out["2F"] = "    2       0     ";
    gainbplr_out["2T"] = "    2       1     ";
    gainbplr_out["4F"] = "    4       0     ";
    ostr << gainbplr_out[gainbplr];

    for (uint ix=0; ix<8; ix++) {
        ostr << "  " << offst[ix].leftJustified(9).toStdString()
             << " "  << slope[ix].leftJustified(8).toStdString();
    }
    ostr << std::endl;

    QString filename = calfile_dir.text();

#ifndef SANDBOX
    filename += QString("A2D/");
#endif
    filename += "A2D" + serial_number + ".dat";

    exportCalFile(filename, ostr.str());
}

/* -------------------------------------------------------------------- */

void MainWindow::exportCalFile(QString filename, std::string contents)
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
    QRegExp rxDateTime("^([12][0-9][0-9][0-9] ... [ 0-3][0-9] "
                       "[ 0-2][0-9]:[0-5][0-9]:[0-5][0-9])");

    QDateTime bt, ct, et(QDate(2999,1,1),QTime(0,0,0));

    // Find datetime stamp in the new calibration entry.
    QString qstr(contents.c_str());
    QStringList qsl = qstr.split("\n");
    QString calLine = qsl[qsl.size()-2];
    std::cout << "calLine: " << calLine.toStdString() << std::endl;
    if (rxDateTime.indexIn( calLine ) == -1)
        return;

    ct = QDateTime::fromString(rxDateTime.cap(1), "yyyy MMM dd HH:mm:ss");
//  std::cout << "ct: " << ct.toString().toStdString() << " (" << rxDateTime.cap(1).toStdString() << ")" << std::endl;

    // Open the selected calfile (it's ok if it doesn't exist yet).
    std::ifstream calfile ( filename.toStdString().c_str() );

    // Open a unique temporary file.
    char tmpfilename[] = "/tmp/MainWindow_XXXXXX";
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
//      std::cout << "-- buffer -------------------------------------------------------------\n"
//                << buffer.str().c_str()
//                << "-- ------ -------------------------------------------------------------\n";

        // If line matches a previously inserted line then prompt before re-inserting it.
        if ( QString(line.c_str()) == calLine) {
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(0, tr("Duplicate?"),
                        tr("Calibration was already exported to this file\n\nCreate a duplicate entry in file?\n"),
                        QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::No)
                ct = et;
        }
        // If successfully inserted then dump remainder of calfile into tmpfile.
        if ( ct == et) {
//          std::cout << "APPENDING REMAINDER | " << buffer.str().c_str();
            write(tmpfile, buffer.str().c_str(), buffer.str().length());
            buffer.str("");
            continue;
        }
        // Find datetime stamp from the latest line in the buffer.
        if (rxDateTime.indexIn( QString(line.c_str()) ) == -1)
            continue;

        // insert leading zeros to day and hour fields
        // 01234567890123456789
        // yyyy MMM dd HH:mm:ss
        QString aTime = rxDateTime.cap(1);
        if (aTime[9]  == ' ') aTime[9]  = '0';
        if (aTime[12] == ' ') aTime[12] = '0';
        bt = QDateTime::fromString(aTime, "yyyy MMM dd HH:mm:ss");
//      std::cout << "  bt: " << bt.toString().toStdString() << " (" << rxDateTime.cap(1).toStdString() << ")" << std::endl;
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
        QString status = modelData(rowIndex.row(), clm_status);
        status[statfi['E']] = 'E';

        _model->setData(_model->index(rowIndex.row(), clm_status),
                             status);
    }
    // save database
    saveButtonClicked();
}

/* -------------------------------------------------------------------- */

void MainWindow::exportCsvFile(QString filename, std::string contents)
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

void MainWindow::cloneButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QModelIndex insertIndex = _table->selectionModel()->currentIndex();
    int row = insertIndex.row();

    // extract the site of the instrument from the current row
    QRegExp rxSite("(.*)[-_]");
    QString rid = modelData(row, clm_rid);
    if (rxSite.indexIn(rid) == -1) {
        QMessageBox::warning(0, tr("error"),
          tr("Site name (tail number) not found in 'rid'!"));
        return;
    }
    QString site = rxSite.cap(1);

    // set clone's parent ID
    QString pid           = rid;

    // set clone's new child ID
    QSqlQuery query(QSqlDatabase::database());
    QString cmd("SELECT to_char(nextval('" + site + "_rid'),'\"" + site + "_\"FM00000000')");
    if (query.exec(cmd) == false ||
        query.first() == false) {
        QMessageBox::warning(0, tr("error"),
          tr("Failed to obtain next id!"));
        return;
    }
    rid = query.value(0).toString();

    // copy data from parent row
    QString status        = modelData(row, clm_status);
    QDateTime cal_date    = _model->index(row, clm_cal_date).data().toDateTime();
    QString project_name  = modelData(row, clm_project_name);
    QString username      = modelData(row, clm_username);
    QString sensor_type   = modelData(row, clm_sensor_type);
    QString serial_number = modelData(row, clm_serial_number);
    QString var_name      = modelData(row, clm_var_name);
    QString dsm_name      = modelData(row, clm_dsm_name);
    QString cal_type      = modelData(row, clm_cal_type);
    QString channel       = modelData(row, clm_channel);
    QString gainbplr      = modelData(row, clm_gainbplr);
    QString ads_file_name = modelData(row, clm_ads_file_name);
    QString set_times     = modelData(row, clm_set_times);
    QString set_points    = modelData(row, clm_set_points);
    QString averages      = modelData(row, clm_averages);
    QString stddevs       = modelData(row, clm_stddevs);
    QString cal           = modelData(row, clm_cal);
    QString temperature   = modelData(row, clm_temperature);
    QString comment       = modelData(row, clm_comment);

    // advance the clone's timestamp to be one second past the parent's
    cal_date = cal_date.addSecs(1);

    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;
    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;
    // create a new row
    int newRow = row + 1;
    _model->insertRow(newRow);
    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;
    std::cout << "_model->rowCount() = " << _model->rowCount() << std::endl;

    // paste the parent's data into its clone
    _model->setData(_model->index(newRow, clm_rid),           rid);
    _model->setData(_model->index(newRow, clm_pid),           pid);
    _model->setData(_model->index(newRow, clm_status),        status);
    _model->setData(_model->index(newRow, clm_cal_date),      cal_date);
    _model->setData(_model->index(newRow, clm_project_name),  project_name);
    _model->setData(_model->index(newRow, clm_username),      username);
    _model->setData(_model->index(newRow, clm_sensor_type),   sensor_type);
    _model->setData(_model->index(newRow, clm_serial_number), serial_number);
    _model->setData(_model->index(newRow, clm_var_name),      var_name);
    _model->setData(_model->index(newRow, clm_dsm_name),      dsm_name);
    _model->setData(_model->index(newRow, clm_cal_type),      cal_type);
    _model->setData(_model->index(newRow, clm_channel),       channel);
    _model->setData(_model->index(newRow, clm_gainbplr),      gainbplr);
    _model->setData(_model->index(newRow, clm_ads_file_name), ads_file_name);
    _model->setData(_model->index(newRow, clm_set_times),     set_times);
    _model->setData(_model->index(newRow, clm_set_points),    set_points);
    _model->setData(_model->index(newRow, clm_averages),      averages);
    _model->setData(_model->index(newRow, clm_stddevs),       stddevs);
    _model->setData(_model->index(newRow, clm_cal),           cal);
    _model->setData(_model->index(newRow, clm_temperature),   temperature);
    _model->setData(_model->index(newRow, clm_comment),       comment);

    // mark child as a clone
    status[statfi['C']] = 'c';
    _model->setData(_model->index(newRow, clm_status), status);

    // mark parent as cloned
    status[statfi['C']] = 'C';
    _model->setData(_model->index(row, clm_status), status);

    insertIndex = _model->index(newRow, 1);
    _table->setCurrentIndex(insertIndex);
    _table->selectRow(newRow);
}

/* -------------------------------------------------------------------- */

void MainWindow::removeButtonClicked()
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
        QString status = modelData(rowIndex.row(), clm_status);
        status[statfi['R']] = 'R';

        _model->setData(_model->index(rowIndex.row(), clm_status),
                        status);
    }
    // save database
    saveButtonClicked();
}

/* -------------------------------------------------------------------- */

void MainWindow::changeFitButtonClicked()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    int row = _table->selectionModel()->currentIndex().row();

    QStringList list_cal        = extractListFromBracedCSV(row, clm_cal);
    if (list_cal.isEmpty()) return;

    int degree = list_cal.size();

    bool ok;
    degree = QInputDialog::getInt(this, "",
               tr("Set Polynominal Order:"), degree, 2, MAX_ORDER, 1, &ok);

    // exit if no change or cancel is selected
    if (degree == list_cal.size() || !ok)
        return;

    changeFitButtonClicked(row, degree);
}

/* -------------------------------------------------------------------- */

void MainWindow::changeFitButtonClicked(int row, int degree)
{
    std::cout << __PRETTY_FUNCTION__ << " row: " << row << " degree: " << degree << std::endl;

    QStringList list_averages   = extractListFromBracedCSV(row, clm_averages);
    if (list_averages.isEmpty()) return;
    QStringList list_set_points = extractListFromBracedCSV(row, clm_set_points);
    if (list_set_points.isEmpty()) return;

    std::vector<double> x;
    foreach (QString average, list_averages)
        if (!average.isEmpty())
            x.push_back( average.toDouble() );

    std::vector<double> y;
    foreach (QString setPoint, list_set_points)
        if (!setPoint.isEmpty())
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

    // change cal data in the form
    int i = 0;
    for(; i < degree; i++) {
        _form->_currCalCList[i]->setText( QString::number(coeff[i]) );
        cals << coeff[i];
        if (i < degree - 1)
            cals << ",";
    }
    for (; i<4; i++)
        _form->_currCalCList[i]->setText("");

    cals << "}";

    std::cout << "old cal: " << modelData(row, clm_cal).toStdString() << std::endl;
    std::cout << "new cal: " << cals.str() << std::endl;

    // change cal data in the model
    _model->setData(_model->index(row, clm_cal),
                        QString(cals.str().c_str()));
}
