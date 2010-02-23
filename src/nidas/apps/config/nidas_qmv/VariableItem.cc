
#include "VariableItem.h"

#include <iostream>
#include <fstream>

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

QString VariableItem::name()
{
    Variable *var = reinterpret_cast<Variable*>(this->nidasObject);
    return QString::fromStdString(var->getName());
}
