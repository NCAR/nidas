
#ifndef _SENSOR_ITEM_H
#define _SENSOR_ITEM_H

#include "NidasItem.h"
#include "SampleItem.h"
#include <nidas/core/DSMSensor.h>

using namespace nidas::core;


class SensorItem : public NidasItem
{

public:
    SensorItem(DSMSensor *sensor, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~SensorItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string devicename() { return this->dataField(1).toStdString(); }

    const QVariant & childLabel(int column) const { return NidasItem::_Sample_Label; }
    int childColumnCount() const {return 1;}

    QString dataField(int column);

protected:
        // get/convert to the underlying model pointers
    DSMSensor *getDSMSensor() const { return _sensor; }

    std::string getSerialNumberString(DSMSensor *sensor);
    QString name();

private:
    DSMSensor * _sensor;

};

#endif
