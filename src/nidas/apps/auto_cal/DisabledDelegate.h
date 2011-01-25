#ifndef _plotlib_DisabledDelegate_h_
#define _plotlib_DisabledDelegate_h_

#include <QItemDelegate>

/**
 * @class calib::DisabledDelegate
 * Provides a disabled editor widget for NOT altering cells in the table view.
 */
class DisabledDelegate : public QItemDelegate
{
  public:
    DisabledDelegate(QObject *parent = 0) : QItemDelegate(parent) { };

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const { return 0; };
};

#endif
