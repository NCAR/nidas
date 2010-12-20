#ifndef _EditorPage_h_
#define _EditorPage_h_

#include "ComboBoxDelegate.h"

#include <QWizardPage>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QTableView>
#include <string>
#include <map>

//class QSqlTableModel;

/**
 * @class calib::EditorPage
 * Simple dialog with a QDataTable to display the main calibration table.
 */
class EditorPage : public QWizardPage
{
  Q_OBJECT

public:
    EditorPage(QWidget *parent = 0);
    ~EditorPage();

  /// Create database connection.                                                                                             
  virtual void
  createDatabaseConnection();

  /// Open calibration database to read or write.
  virtual bool
  openDatabase();

  /// Close calibration database.
  virtual void
  closeDatabase();

  QSqlDatabase _calibDB;

    void initializePage();
    int nextId() const { return -1; }
    void setVisible(bool visible);
  /**
   * @fn void EditorPage::view()
   * static convenience function to popup dialog and display history.
  static void view(QWidget* parent);
   */

protected slots:
  void refresh();

protected:
  // Database driver for Qt.                                                                                                  
  static const QString DB_DRIVER;
  static const QString CALIB_DB_NAME;

  QSqlTableModel* _model;
  QTableView *_calibTable;

  std::map<std::string, ComboBoxDelegate*> delegate;
};

#endif
