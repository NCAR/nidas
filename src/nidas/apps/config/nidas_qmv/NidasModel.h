
#ifndef _NIDAS_MODEL_H
#define _NIDAS_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>


class NidasItem;
class Project;


class NidasModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    NidasModel(Project *project, QObject *parent = 0);
    ~NidasModel();

    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

private:
    NidasItem *rootItem;
};

#endif
