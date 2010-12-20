#ifndef _plotlib_ComboBoxDelegate_h_
#define _plotlib_ComboBoxDelegate_h_

#include <QSqlDatabase>
#include <QItemDelegate>
#include <string>

/**
 * @class calib::ComboBoxDelegate
 * Provides a QComboBox editor widget for altering cells in the table view.
 */
class ComboBoxDelegate : public QItemDelegate
{
  Q_OBJECT

  public:
    ComboBoxDelegate(const QSqlDatabase& db, std::string sqlcol, QObject *parent = 0);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;

    void setEditorData(QWidget *editor, const QModelIndex &index) const;

    /**
     * When the user has finished editing the value in the box, the view asks
     * the delegate to store the edited value in the model by calling the
     * setModelData() function.
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const;

  protected:
    QSqlDatabase database;
    std::string sql_column;
};

#endif
