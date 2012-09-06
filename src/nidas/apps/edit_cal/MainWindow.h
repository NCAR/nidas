#ifndef _MainWindow_h_
#define _MainWindow_h_

#include <QMainWindow>
#include <QSignalMapper>
#include <QLineEdit>
#include <QTableView>

#include <map>
#include <string>

#include <qwt_plot_curve.h>

#include "calTableHeaders.h"
#include "CalibrationPlot.h"
#include "CalibrationForm.h"

#include <QList>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QMenu;
class QMenuBar;
class QSqlTableModel;
class QItemSelectionModel;
class QStringList;
QT_END_NAMESPACE

class BackgroundColorDelegate;

/**
 * @class calib::MainWindow
 * Provides an editable QDataTable to display the main calibration SQL table.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

    bool openDatabase(QString hostname);

protected slots:

    /// Toggle the row's hidden state selected by cal type.
    void toggleRow(int id);

    /// Applies hidden states to all rows.
    void hideRows();

    /// Toggles column's hidden state.
    void toggleColumn(int id);

    /// Detects changes to the database.
    void dataChanged(const QModelIndex& old, const QModelIndex& now);

    void onQuit();

    /// Saves changes to the local database.
    /// @returns 0 on success.
    int saveButtonClicked();

    /// Open this calibration line entry in the form view
    void editCalButtonClicked();

    /// Plot this calibration line entry in the graph
    void plotCalButtonClicked();

    /// Unplot this calibration line entry in the graph
    void unplotCalButtonClicked();

    /// Create an entry in the corresponding NIDAS cal file.
    void exportCalButtonClicked();

    /// Create a CSV file of set_points and avererages for an entry.
    void exportCsvButtonClicked();

    /// View NIDAS cal file for an entry.
    void viewCalButtonClicked();

    /// View CSV file for an entry.
    void viewCsvButtonClicked();

    /// Clone an entry in the database.
    void cloneButtonClicked();

    /// Marks an entry as deleted in the database.
    void removeButtonClicked();

    /// Changes the polynominal fit
    void changeFitButtonClicked();

    /// Changes the polynominal fit
    void changeFitButtonClicked(int row, int degree);

    /// Creates a popup menu
    void contextMenu( const QPoint &pos );

    /// Remove a set point (and all of its associated values)
    void delThisSetPoint(int row, int index);

    /// Replot row
    void replot(int row);

private:

    /// Plot calibration for this row
    void plotCalButtonClicked(int row);

    /// Unplot calibration for this row
    void unplotCalButtonClicked(int row);

    QSqlTableModel*          _model;
    QTableView*              _table;
    CalibrationPlot*         _plot;
    CalibrationForm*         _form;
    BackgroundColorDelegate* _delegate;

    QList<CalibrationCurve *> plottedCurves;

    void setupDatabase();
    void setupModels();
    void setupDelegates();
    void setupTable();
    void setupViews();
    void setupMenus();

    void exportInstrument(int currentRow);
    void exportAnalog(int currentRow);

    void exportCalFile(QString filename, std::string contents);
    void exportCsvFile(QString filename, std::string contents);
    void viewFile(QString filename, QString title, QString prequel = "");

    /// Imports a remote calibration table into the master database.
    void importRemoteCalibTable(QString remote, bool showResults);

    bool changeDetected;

    /// This prevents annoying save requests.
    bool exportUsed;

    bool showAnalog;
    bool showInstrument;
    bool showCloned;
    bool showRemoved;
    bool showExported;

    QAction *addRowAction(QMenu *menu, const QString &text,
                          QActionGroup *group, QSignalMapper *mapper,
                          int id, bool checked);

    QAction *addColAction(QMenu *menu, const QString &text,
                          QActionGroup *group, QSignalMapper *mapper,
                          int id, bool checked);

    QAction *addAction(QMenu *menu, const QString &text,
                       QActionGroup *group, QSignalMapper *mapper,
                       int id, bool checked);

    /// Returns a string from the model
    QString modelData(int row, int col);

    /// Returns a list of extracted values set in braces
    QStringList extractListFromBracedCSV(int row, calTableHeaders key);

    /// Returns a list of extracted values set in braces
    QStringList extractListFromBracedCSV(QString string);

    QMenu *verticalMenu;

    static const QString DB_DRIVER;
    static const QString CALIB_DB_HOST;
    static const QString CALIB_DB_USER;
    static const QString CALIB_DB_NAME;
    static const int MAX_ORDER;

    QLineEdit calfile_dir;
    QLineEdit csvfile_dir;

    std::map<QString, QString> tailNum;

    std::map<int, QString> tailNumIdx;
    std::map<int, bool>    showTailNum;

    /// status flag indexes (i.e. stat['X'] = index;)
    std::map<char, char> statfi;
};

#endif
