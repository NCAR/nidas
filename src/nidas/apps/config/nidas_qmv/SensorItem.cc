
#include "SensorItem.h"

#include <iostream>
#include <fstream>


SensorItem::SensorItem(DSMSensor *sensor, int row, NidasModel *theModel, NidasItem *parent) 
{
    _sensor = sensor;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

SensorItem::~SensorItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

    delete this->getDSMSensor();

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
    std::cerr << "~SensorItem caught exception\n";
}
}

NidasItem * SensorItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    int j;

    SampleTagIterator it;
    for (j=0, it = _sensor->getSampleTagIterator(); it.hasNext(); j++) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        if (j<i) continue; // skip old cached items (after it.next())
        NidasItem *childItem = new SampleItem(sample, j, model, this);
        childItems.append( childItem);
    }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

QString SensorItem::dataField(int column)
{
  if (column == 0) return name();

    switch (column) {
      case 1:
        return QString::fromStdString(_sensor->getDeviceName());
      case 2:
        return QString::fromStdString(getSerialNumberString(_sensor));
      case 3:
        return QString("(%1,%2)").arg(_sensor->getDSMId()).arg(_sensor->getSensorId());
      /* default: fall thru */
    }

  return QString();
}

std::string SensorItem::getSerialNumberString(DSMSensor *sensor)
// maybe move this to a helper class
{
    const Parameter * parm = _sensor->getParameter("SerialNumber");
    if (parm) 
        return parm->getStringValue(0);

    CalFile *cf = _sensor->getCalFile();
    if (cf)
        return cf->getFile().substr(0,cf->getFile().find(".dat"));

return(std::string());
}


QString SensorItem::name()
{
    if (_sensor->getCatalogName().length() > 0)
        return(QString::fromStdString(_sensor->getCatalogName()+_sensor->getSuffix()));
    else return(QString::fromStdString(_sensor->getClassName()+_sensor->getSuffix()));
}
