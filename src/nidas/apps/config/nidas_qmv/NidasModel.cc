
// adapted from Qt examples:
// Simple Tree Model Example (4.x)
//  itemviews/simpletreemodel/treemodel.cpp
// Simple DOM Model Example (4.5)
//  itemviews/simpledommodel/dommodel.cpp

#include <QtGui>

#include "NidasModel.h"

NidasModel::NidasModel(Project *project, QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem = new NidasItem(project);
}

NidasModel::~NidasModel()
{
    delete rootItem;
}

Qt::ItemFlags NidasModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex NidasModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    NidasItem *childItem = static_cast<NidasItem*>(index.internalPointer());
    NidasItem *parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex NidasModel::index(int row, int column, const QModelIndex &parent)
            const
{
// copied from dommodel.cpp
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    NidasItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<NidasItem*>(parent.internalPointer());

    NidasItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QVariant NidasModel::data(const QModelIndex &index, int role) const
{
// adapted from dommodel.cpp

    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    NidasItem *item = static_cast<NidasItem*>(index.internalPointer());

    switch (index.column()) {
        case 0:
            //return item.XXX();
            return QString("name of $1").arg(index.column());
        case 1:
            //get something from item for each column
            //XXX
            //break/return;
            return QString("value");
        default:
            return QVariant();
    }
}

int NidasModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    NidasItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<NidasItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int NidasModel::columnCount(const QModelIndex &parent) const
{
return 2; // XXX
}
