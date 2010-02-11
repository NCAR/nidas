
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
    : QAbstractItemModel(parent), _currentRootIndex(QModelIndex())
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

    NidasItem *parentItem = getItem(parent);

    return parentItem->childCount();
}

NidasItem *NidasModel::getItem(const QModelIndex &index) const
{
if (!index.isValid())
    return rootItem;

return static_cast<NidasItem*>(index.internalPointer());
}

int NidasModel::columnCount(const QModelIndex &parent) const
{
  return getItem(parent)->childColumnCount();
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
  NidasItem *parentItem = getItem(parent);

  if ( (role == Qt::DisplayRole) &&
       (orientation == Qt::Horizontal)
     )
       return parentItem->childLabel(section);

  if (role == Qt::TextAlignmentRole)
      return Qt::AlignLeft;

  return QVariant();
}

/*!
 * \brief Create a QModelIndex for the NidasItem that points to \a nidasData
 * and is a descendant of startItem.
 *
 * Stoopid recursive search implemented to see what works.
 *
 */
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
    NidasItem *parentItem = getItem(parent);
    if (!parentItem->child(row)) // force NidasItem update
        cerr << "insertRows parentItem->child(" << row << ") failed!\n"; // exception?

    endInsertRows();
    return true;
}

bool NidasModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row+count-1);

    NidasItem *parentItem = getItem(parent);
    parentItem->removeChildren(row,row+count-1);

    endRemoveRows();
    return true;
}

/*!
 * try to get a DSMConfig from the current root index (i.e. current table view's root)
 *
 * Returns something useful only when a DSM is the current rootIndex
 * \return pointer to the DSMItem or NULL if current root is not a DSM
 *
DSMItem * NidasModel::getCurrentDSMItem()
{
DSMItem *dsmItem = dynamic_cast<DSMItem*>(getItem(_currentRootIndex));
if (!dsmItem)
    throw InternalProcessingException("Current root index is not a DSM.");
return dsmItem;
}
 */
