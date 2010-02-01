
#ifndef _NIDAS_MODEL_H
#define _NIDAS_MODEL_H

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>


//class NidasItem;
//#include <nidas/core/Project.h>
#include "NidasItem.h"


class NidasModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    NidasModel(nidas::core::Project *project, QObject *parent = 0);
    ~NidasModel();

    QVariant data(const QModelIndex &index, int role) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role);

protected:
    NidasItem *getParentItem(const QModelIndex &parent) const;

private:
    NidasItem *rootItem;
    QHash<int,QVariant> columnHeaders;
};

#endif
