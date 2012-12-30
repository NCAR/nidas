#ifndef SORTFILTERPROXYMODEL_H
#define SORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class SortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit SortFilterProxyModel(QObject *parent = 0);
    void setFilterFixedString(int column, const QString &pattern);
    void clearFilters();
    void refreshFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

private:
    QMap<int, QString> m_columnPatterns;
};

#endif // SORTFILTERPROXYMODEL_H
