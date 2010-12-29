#ifndef _plotlib_EditCalDialog_h_
#define _plotlib_EditCalDialog_h_

#include <map>
#include <string>

#include "ui_EditCalDialog.h"
#include "ComboBoxDelegate.h"

class QSqlTableModel;

/**
 * @class calib::EditCalDialog
 * Simple dialog with a QDataTable to display the main calibration table.
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
    void syncButtonClicked();
    void saveButtonClicked();
    void exportButtonClicked();
    void removeButtonClicked();
    void closeButtonClicked();

protected:
  QSqlDatabase _calibDB;
  QSqlTableModel* _model;

private:
    static const QString DB_DRIVER;
    static const QString CALIB_DB_HOST;
    static const QString CALIB_DB_USER;
    static const QString CALIB_DB_NAME;

    std::map<std::string, ComboBoxDelegate*> delegate;
};

#endif
