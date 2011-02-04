#ifndef _EditCalDialog_h_
#define _EditCalDialog_h_

#include <QDialog>
#include <QSignalMapper>
#include <QSqlDatabase>
#include <QItemDelegate>
#include <QLineEdit>

#include <map>
#include <string>

#include "ui_EditCalDialog.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QSqlTableModel;
QT_END_NAMESPACE

/**
 * @class calib::EditCalDialog
 * Provides an editable QDataTable to display the main calibration SQL table.
 */
class EditCalDialog : public QDialog, public Ui::Ui_EditCalDialog
{
    Q_OBJECT

public:
    EditCalDialog();
    ~EditCalDialog();

    void createDatabaseConnection();
    bool openDatabase();
    void closeDatabase();

protected slots:

    /// Toggle the row's hidden state selected by cal type.
    void toggleRow(int id);

    /// Applies hidden states to all rows.
    void hideRows();

    /// Toggles column's hidden state.
    void toggleColumn(int id);

    /// Detects changes to the database.
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    /// Closes the dialog.
    void reject();

    /** \brief Provides a file dialog for choosing where to store the exported '.dat' files.
        The choosen path needs to contain these folders:
        <table>
        <tr><td>Engineering</td><td><b>instruments</b></td></tr>
        <tr><td>A2D/</td><td><b>analogs</b></td></tr>
        </table>
    */
    void pathButtonClicked();

    /// Saves changes to the local database.
    void saveButtonClicked();

    /// Generates a cal .dat file used by DSM server.
    void exportButtonClicked();

    /// Removes a calibration entry row.
    void removeButtonClicked();

    /// creates a vertical popup menu
    void verticalHeaderMenu( const QPoint &pos );

protected:
    QSqlDatabase _calibDB;
    QSqlTableModel* _model;

private:

    void exportInstrument(int currentRow);
    void exportAnalog(int currentRow);
    void exportCalFile(QString filename, std::string contents);

    /// List of remote sites that fill a calibration database.
    QStringList siteList;

    /// Copies the calibration table from 'source' to 'destination'.
    void syncRemoteCalibTable(QString source, QString destination);

    bool changeDetected;

    QAction *addRowAction(QMenu *menu, const QString &text,
                          QActionGroup *group, QSignalMapper *mapper,
                          int id, bool checked);

    QAction *addColAction(QMenu *menu, const QString &text,
                          QActionGroup *group, QSignalMapper *mapper,
                          int id, bool checked);

    QAction *addAction(QMenu *menu, const QString &text,
                       QActionGroup *group, QSignalMapper *mapper,
                       int id, bool checked);

    QString modelData(int row, int col);

    void createMenu();
    QMenuBar *menuBar;

    QMenu *verticalMenu;

    QMenu *fileMenu;
    QAction *pathActn;
    QAction *saveActn;
    QAction *exitActn;

    QMenu *viewMenu;
    QMenu  *rowsMenu;
    QMenu  *colsMenu;

    QSignalMapper *rowsMapper;
    QSignalMapper *colsMapper;

    bool showAnalog;
    bool showInstrument;

    static const QString DB_DRIVER;
    static const QString CALIB_DB_HOST;
    static const QString CALIB_DB_USER;
    static const QString CALIB_DB_NAME;
    static const QString SCRATCH_DIR;
    QLineEdit calfile_dir;

    std::map<std::string, QItemDelegate*> delegate;
};

#endif
