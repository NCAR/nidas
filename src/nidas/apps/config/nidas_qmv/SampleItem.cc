
#include "SampleItem.h"

#include <iostream>
#include <fstream>

SampleItem::~SampleItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

    delete this->getSample();

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

QString SampleItem::name()
{
    SampleTag *sampleTag = reinterpret_cast<SampleTag*>(this->nidasObject);
    return QString("Sample %1").arg(sampleTag->getSampleId());
}
