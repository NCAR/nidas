#ifndef _EditCalDialog_h_
#define _EditCalDialog_h_

#include <QDialog>
#include <QSignalMapper>

#include <map>
#include <string>

#include "ui_EditCalDialog.h"
#include "ComboBoxDelegate.h"

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

    /// Toggles column's hidden state.
    void toggleColumn(int id);

    /// Detects changes to the database.
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    /// Closes the dialog.
    void reject();

    /// Pulls then pushes database to the master server.
    void syncButtonClicked();

    /// Saves changes to the local database.
    void saveButtonClicked();

    /// Generates a cal .dat file used by DSM server.
    void exportButtonClicked();

    /// Removes a calibration entry row.
    void removeButtonClicked();

protected:
    QSqlDatabase _calibDB;
    QSqlTableModel* _model;

private:
    bool noChangeDetected;
    QAction *addAction(QMenu *menu, const QString &text,
                       QActionGroup *group, QSignalMapper *mapper,
                       int id, bool checked);

    void createMenu();
    QMenuBar *menuBar;
    QMenu *columnsMenu;

    static const QString DB_DRIVER;
    static const QString CALIB_DB_HOST;
    static const QString CALIB_DB_USER;
    static const QString CALIB_DB_NAME;

    QSignalMapper *mapper;

    std::map<std::string, ComboBoxDelegate*> delegate;
};

#endif
