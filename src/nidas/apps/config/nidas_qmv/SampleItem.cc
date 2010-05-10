
#include "SampleItem.h"

#include <iostream>
#include <fstream>


SampleItem::SampleItem(SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent) 
{
    _sampleTag = sampleTag;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}


SampleItem::~SampleItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

    delete this->getSampleTag();

/*
 * unparent the children and schedule them for deletion
 * prevents parentItem->removeChild() from being called for an already gone parentItem
 *
 * can't be done because children() is const
 * also, subclasses will have invalid pointers to nidasObjects
 *   that were already recursively deleted by nidas code
 *
for (int i=0; i<children().size(); i++) {
  QObject *obj = children()[i];
  obj->setParent(0);
  obj->deleteLater();
  }
 */

} catch (...) {
    // ugh!?!
    std::cerr << "~SampleItem caught exception\n";
}
}

NidasItem * SampleItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    int j;

    VariableIterator it = _sampleTag->getVariableIterator();
    for (j=0; it.hasNext(); j++) {
        Variable* var = (Variable*)it.next(); // XXX cast from const
        if (j<i) continue; // skip old cached items (after it.next())
        NidasItem *childItem = new VariableItem(var, j, model, this);
        childItems.append( childItem);
        }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

QString SampleItem::dataField(int column)
{
  if (column == 0) return QString("%1").arg(_sampleTag->getSampleId());
  if (column == 1) return QString("%1").arg(_sampleTag->getRate());

  return QString();
}

QString SampleItem::name()
{
    return QString("Sample %1").arg(_sampleTag->getSampleId());
}
