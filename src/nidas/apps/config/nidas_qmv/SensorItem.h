
#ifndef _SENSOR_ITEM_H
#define _SENSOR_ITEM_H

#include "NidasItem.h"
#include "SampleItem.h"
#include <nidas/core/DSMSensor.h>

using namespace nidas::core;


class SensorItem : public NidasItem
{

public:
    SensorItem(DSMSensor *sensor, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(sensor,row,model,parent) {}

    ~SensorItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item) { return false; } // XXX

    std::string devicename() { return this->dataField(1).toStdString(); }


protected:
        // get/convert to the underlying model pointers
    DSMSensor *getDSMSensor() const {
     if (nidasType == SENSOR)
         return reinterpret_cast<DSMSensor*>(this->nidasObject);
     else return 0;
     }

     QString name();

};

#endif
