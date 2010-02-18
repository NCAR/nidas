
#include "SensorItem.h"

#include <iostream>
#include <fstream>

SensorItem::~SensorItem()
{
std::cerr<< "~SensorItem() called\n";
try {
NidasItem *parentItem = this->parent();
if (parentItem) {
    parentItem->removeChild(this);
    }

std::cerr << "~SensorItem about to delete sensor\n";

delete this->getDSMSensor();
//return reinterpret_cast<DSMSensor*>(this->nidasObject);

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

std::cerr << "  ...done\n";
} catch (...) {
    // ugh!?!
    std::cerr << "~SensorItem caught exception\n";
}
}
