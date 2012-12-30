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
#include "SortFilterProxyModel.h"

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

    /// Set a column filter pattern for a given column in the table view.
    void setFilterFixedString(int column, const QString &pattern);

signals:
  void submitForm();
  void revertForm();

protected slots:

    /// Remember what the last item selected was.
    void tableItemPressed(const QModelIndex &index);

    /// Toggle the row's hidden state selected by cal type.
    void toggleRow(int id);

    /// Applies hidden states to all rows.
    void hideRows();

    /// Toggles column's hidden state.
    void toggleColumn(int id);

    /// Detects changes to the database.
    void dataChanged(const QModelIndex& old, const QModelIndex& now);

    void onQuit();

    /// Imports a calibration from an ISFF file.
    int importButtonClicked();

    /// Saves changes to the local database.
    /// @returns 0 on success.
    int saveButtonClicked();

    /// Scroll to the last selected item.
    void scrollToLastClicked();

    /// Scroll to the currently edited row
    void scrollToEditedRow();

    /// Open this calibration line entry in the form view
    void editCalButtonClicked();

    /// Plot this calibration line entry in the graph
    void plotCalButtonClicked();

    /// Unplot this calibration line entry in the graph
    void unplotCalButtonClicked();

    /// Unplots all calibrations and resets the color pallette sequence
    void unplotAllButtonClicked();

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
    void changeFitButtonClicked(int row, int order);

    /// Creates a popup menu for the table's header
    void showHeaderMenu( const QPoint &pos );

    /// Filters a selected column by a user entered string
    void filterBy();

    /// Unfilters all columns
    void unfilterAll();

    /// hides a selected column
    void hideColumn();

    /// shows a selected column choosen by the user
    void showColumn();

    /// Creates a popup menu for the table
    void showTableMenu( const QPoint &pos );

    /// Remove a set point (and all of its associated values)
    void removeSetPoint(int row, int index);

    /// Replot row
    void replot(int row);

    /// Initialize form entries for row
    void initializeForm(int row);

    /// Submit form entries for row to model
    void submitForm(int row);

private:

    /// Ask user to submit the current Form before editing a new line one or quiting
    void unsubmittedFormQuery(QString title);

    /// restrict allowable polynominal order radio buttons
    void restrictOrderChoice(QStringList list_set_points);

    /// Plot calibration for this row
    void plotCalButtonClicked(int row);

    /// Unplot calibration for this row
    void unplotCalButtonClicked(int row);

    QSqlTableModel*          _model;
    SortFilterProxyModel*    _proxy;
    QTableView*              _table;
    CalibrationPlot*         _plot;
    CalibrationForm*         _form;
    BackgroundColorDelegate* _delegate;
    QModelIndex              _lastIndex;
    QModelIndex              _editIndex;

    QActionGroup *colsGrp;

    /// Selected column when the table header is right clicked on for its context menu.
    int _column;

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
    void viewFile(QString filename, QString title);

    /// Imports a remote calibration table into the master database.
    void importRemoteCalibTable(QString remote);

    bool changeDetected;

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

    /// Returns a list of extracted values set within braces from the table
    QStringList extractListFromBracedCSV(int row, calTableHeaders key);

    /// Returns a list of extracted values set within braces
    QStringList extractListFromBracedCSV(QString string);

    /// Returns a string set within in braces
    QString extractStringWithinBraced(QString string);

    QMenu *headerMenu;
    QMenu *tableMenu;

    QLineEdit calfile_dir;
    QLineEdit csvfile_dir;

    std::map<int, QString> tailNumIdx;
    std::map<int, bool>    showTailNum;

    /// Temporary values from an edited line in the model that allows initial
    /// changes in the form to be seen in the plot.
    QString form_set_times;
    QString form_set_points;
    QString form_averages;
    QString form_stddevs;
    QString form_cal;
};

#endif
