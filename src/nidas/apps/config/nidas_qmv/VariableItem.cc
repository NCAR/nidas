
#include "VariableItem.h"

#include <iostream>
#include <fstream>



VariableItem::VariableItem(Variable *variable, int row, NidasModel *theModel, NidasItem *parent) 
{
    _variable = variable;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

VariableItem::~VariableItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

    delete this->getVariable();

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
    std::cerr << "~VariableItem caught exception\n";
}
}

QString VariableItem::dataField(int column)
{
  if (column == 0) return name();

  return QString();
}

QString VariableItem::name()
{
    return QString::fromStdString(_variable->getName());
}
