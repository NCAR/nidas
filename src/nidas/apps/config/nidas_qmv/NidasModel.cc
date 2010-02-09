
// adapted from Qt examples:
// Simple Tree Model Example (4.x)
//  itemviews/simpletreemodel/treemodel.cpp
// Simple DOM Model Example (4.5)
//  itemviews/simpledommodel/dommodel.cpp

#include <QtGui>

#include "NidasModel.h"
#include "NidasItem.h"

#include <iostream>
#include <fstream>
using namespace std;



NidasModel::NidasModel(nidas::core::Project *project, xercesc::DOMDocument *doc, QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem = new NidasItem(project, 0, this);
    domDoc = doc;
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

/*
 * ugh: change this object's header data when somebody asks for columnCount
 *  seems like only place to know that columns are changing and we have a QModelIndex
 *  headerData() seems like it should have a QModelIndex instead
 *  may be overkill- don't know how often or when columnCount() is called
 *
 * must const_cast this since we're in a const method
 */
NidasModel* const localThis = const_cast<NidasModel* const>(this);
for (int i=0; i<cols; i++)
    //localThis->setHeaderData(i,Qt::Horizontal,QString("foo %1").arg(i),Qt::DisplayRole);
    localThis->setHeaderData(i,Qt::Horizontal,parentItem->childLabel(i),Qt::DisplayRole);

return cols;
}

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

QVariant NidasModel::headerData(int section, Qt::Orientation orientation, int role) const
{
if ( (role == Qt::DisplayRole) &&
     (orientation == Qt::Horizontal) &&
     columnHeaders.contains(section)
   )
        return columnHeaders[section];

if (role == Qt::TextAlignmentRole)
    return Qt::AlignLeft;

return QVariant();
}


QVariant NidasModel::headerData(int section, Qt::Orientation orientation, int role,
    const QModelIndex &parent) const
/*
 * our own headerData that is given a parent index for child-based columns and headers
 * (how we think headerData() should have been implemented anyways)
 *
 * a custom view will wire this to it's HeaderView via setRootIndex()
 */
{
NidasItem *parentItem = getParentItem(parent);

if ( (role == Qt::DisplayRole) &&
     (orientation == Qt::Horizontal)
   )
       return parentItem->childLabel(section);

if (role == Qt::TextAlignmentRole)
    return Qt::AlignLeft;

return QVariant();
}

QModelIndex NidasModel::findIndex(void *nidasData, NidasItem *startItem) const
{
if (startItem == 0) startItem=rootItem;
if (startItem == 0) return QModelIndex();

int ct = startItem->childCount();

for (int i=0; i<ct; i++) {
  NidasItem *item = startItem->child(i);
  if (item->pointsTo(nidasData))
    return createIndex(i,0,item);
  else {
    QModelIndex index = findIndex(nidasData,item);
    if (index.isValid()) return(index);
    }
  }

return QModelIndex();
}

bool NidasModel::insertRows(int row, int count, const QModelIndex &parent)
{
if (!parent.isValid()) return false; // rather than default to root, which is a valid parent

    beginInsertRows(parent, row, row+count-1);

    // insertion into actual model here
    // (already done by Document::addSensor() for old implementation -- move to here?)
    NidasItem *parentItem = getParentItem(parent);
    if (!parentItem->child(row)) // force NidasItem update
        cerr << "insertRows parentItem->child(" << row << ") failed!\n"; // exception?

    endInsertRows();
    return true;
}

bool NidasModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row+count-1);

    NidasItem *parentItem = getParentItem(parent);
    parentItem->clearChildItems();
    if (!parentItem->child(0)) // force NidasItem update
        cerr << "removeRows parentItem->child(" << row << ") failed!\n"; // exception?

    endRemoveRows();
    return true;
}
