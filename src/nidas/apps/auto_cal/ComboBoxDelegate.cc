#include <QtGui>
#include <QtSql/QSqlQuery>

#include "ComboBoxDelegate.h"
#include <iostream>

ComboBoxDelegate::ComboBoxDelegate(const QSqlDatabase& db, std::string sqlcol, QObject *parent)
   : QItemDelegate(parent), database(db), sql_column(sqlcol)
{
    std::cout << __PRETTY_FUNCTION__ << " : " << sql_column << std::endl;
/*
    std::cout << "database connectOptions : " << (database.connectOptions()).toStdString() << std::endl;
    std::cout << "database connectionName : " << (database.connectionName()).toStdString() << std::endl;
    std::cout << "database databaseName   : " <<  database.databaseName().toStdString() << std::endl;
    std::cout << "database driverName     : " << (database.driverName()).toStdString() << std::endl;
    std::cout << "database hostName       : " << (database.hostName()).toStdString() << std::endl;
    std::cout << "database userName       : " << (database.userName()).toStdString() << std::endl;
    std::cout << "database isOpen()       : " << (database.isOpen() ? "YES" : "NO") << std::endl;
*/
}

QWidget *ComboBoxDelegate::createEditor(QWidget *parent,
                                        const QStyleOptionViewItem &,
                                        const QModelIndex &) const
{
    std::cout << __PRETTY_FUNCTION__ << " : " << sql_column << std::endl;
    QComboBox *editor = new QComboBox(parent);
    editor->setEditable(true);

    // Extract a list of previously used values from the local database
    QString sql = QString("SELECT DISTINCT %1 FROM calibrations").arg( QString::fromStdString(sql_column) );
    std::cout << sql.toStdString() << std::endl;

    QSqlQuery query(sql, database);
    std::cout << "isSelect: " << query.isSelect() << std::endl;
    query.exec();
    while (query.next()) {
        std::cout << sql_column << ": " << query.value(0).toString().trimmed().toStdString() << "<" << std::endl;
        editor->addItem( query.value(0).toString().trimmed() );
    }
    std::cout << "isActive: " << query.isActive() << std::endl;
    return editor;
}

void ComboBoxDelegate::setEditorData(QWidget *editor,
                                     const QModelIndex &index) const
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    QString value = index.model()->data(index, Qt::EditRole).toString().trimmed();

    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    comboBox->setEditText(value);
}

void ComboBoxDelegate::setModelData(QWidget *editor,
                                    QAbstractItemModel *model,
                                    const QModelIndex &index) const
{
   std::cout << __PRETTY_FUNCTION__ << std::endl;
    QComboBox *comboBox = static_cast<QComboBox*>(editor);
    QString curValue = index.model()->data(index, Qt::EditRole).toString().trimmed();
    QString newValue = comboBox->currentText().trimmed();

    if (curValue != newValue)
        model->setData(index, newValue, Qt::EditRole);
}


void ComboBoxDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &) const
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    editor->setGeometry(option.rect);
}
