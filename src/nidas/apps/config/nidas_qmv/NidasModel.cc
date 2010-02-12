
// adapted from Qt examples:
// Simple Tree Model Example (4.x)
//  itemviews/simpletreemodel/treemodel.cpp
// Simple DOM Model Example (4.5)
//  itemviews/simpledommodel/dommodel.cpp

#include <QtGui>

#include "NidasModel.h"
#include "NidasItem.h"
#include "exceptions/InternalProcessingException.h"

#include <iostream>
#include <fstream>
using namespace std;



/*!
 * \brief Implements the Qt Model API for Nidas business model (Project tree and DOM tree)
 *
 * NidasModel fulfills the requirements to work in the Qt world and
 * uses NidasItem as a proxy for the business models.
 *
 */
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
        throw InternalProcessingException("Error inserting new item. Qt and Nidas models are out of sync. (NidasItem::child() returned NULL in NidasModel::insertRows)");

    endInsertRows();
    return true;
}

bool NidasModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row+count-1);

    // removeIndexes() deletes the item
    //NidasItem *parentItem = getItem(parent);
    //parentItem->removeChildren(row,row+count-1);

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

/*!
 * \brief Add a child to \a parentItem by updating the Qt model and thus
 *        also the NidasItem tree from Project.
 *
 *        Hard work done in insertRows()
 *
 * \sa insertRows()
 */
bool NidasModel::appendChild(NidasItem *parentItem)
{
   QModelIndex parentIndex = parentItem->createIndex();
   int newRow = rowCount(parentIndex);
   return insertRows(newRow,1,parentIndex);
}

/*!
 * \brief Remove children for the \a selectedRows from the \a parentItem.
 *
 *        Hard work done in removeRows()
 *
 * \sa removeRows()
 */
bool NidasModel::removeChildren(std::list <int> & selectedRows, NidasItem *parentItem)
{
 QModelIndex parentIndex = parentItem->createIndex();
 std::list <int>::iterator iit;
 for (iit=selectedRows.begin(); iit!=selectedRows.end(); iit++)
    if (!removeRows((*iit),1,parentIndex))
        return false;
return true;
}

bool NidasModel::removeIndexes(QModelIndexList indexList)
{
for (int i=0; i<indexList.size(); i++) {
    QModelIndex index = indexList[i];
    cerr << "removeIndexes i=" << i << " row=" << index.row() << " col=" << index.column() << "\n";

        // the NidasItem for the selected row resides in column 0
    if (index.column() != 0) continue;

    if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)

    NidasItem *item = getItem(index);

    if (!removeRows(item->row(),1,item->parent()->createIndex()))
        return false;

    cerr << "removeIndexes after removeRows before delete item\n";
    //delete item; // handles deletion of all business model data
    item->deleteLater(); // handles deletion of all business model data
    }
return true;
}


#if 0
NidasModel:remove(il)
{
foreach index in il
 NidasItem *item = getItem(index);
 item->parentItem(remove(item));
 item->removeYourself();
}

NidasItem::removeYourself or dtor()
{
parent->remove(this);
}

#endif
