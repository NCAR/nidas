
// adapted from Qt examples:
// Simple Tree Model Example (4.x)
//  itemviews/simpletreemodel/treemodel.cpp
// Simple DOM Model Example (4.5)
//  itemviews/simpledommodel/dommodel.cpp

#include <QtGui>

#include "NidasModel.h"
#include "NidasItem.h"



NidasModel::NidasModel(Project *project, QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem = new NidasItem(project, 0);
    /*
    this->setHeaderData(0,Qt::Horizontal,QString("ctor 0"),Qt::DisplayRole);
    this->setHeaderData(1,Qt::Horizontal,QString("ctor 1"),Qt::DisplayRole);
    this->setHeaderData(2,Qt::Horizontal,QString("ctor 2"),Qt::DisplayRole);
    this->setHeaderData(3,Qt::Horizontal,QString("ctor 3"),Qt::DisplayRole);
    */
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
    return item->dataField(index.column());
}

int NidasModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    NidasItem *parentItem = getParentItem(parent);

    return parentItem->childCount();
}

NidasItem *NidasModel::getParentItem(const QModelIndex &parent) const
{
if (!parent.isValid())
    return rootItem;

return static_cast<NidasItem*>(parent.internalPointer());
}

int NidasModel::columnCount(const QModelIndex &parent) const
{
NidasItem *parentItem = getParentItem(parent);
int cols = parentItem->childColumnCount();

//NidasModel* const localThis = const_cast<NidasModel* const>(this);
//for (int i=0; i<cols; i++)
//    localThis->setHeaderData(i,Qt::Horizontal,QString("foo %1").arg(i),Qt::DisplayRole);

return cols;
}

/*
bool NidasModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if ( (role == Qt::DisplayRole) &&
         (orientation == Qt::Horizontal) &&
         (section >= 0)
       ) {
        columnHeaders[section] = value;
        emit this->headerDataChanged ( orientation, section, section );
        return true;
        }

return false;
}
*/

QVariant NidasModel::headerData(int section, Qt::Orientation orientation, int role) const
{
return QAbstractItemModel::headerData(section,orientation,role);

return section * 2;

return QString("blah %1").arg(section);

if ( (role == Qt::DisplayRole) &&
     (orientation == Qt::Horizontal)
   )
        return QString("blah %1").arg(section);

/*
if ( (role == Qt::DisplayRole) &&
     (orientation == Qt::Horizontal) &&
     columnHeaders.contains(section)
   )
        return columnHeaders[section];
*/

return QVariant();
}
