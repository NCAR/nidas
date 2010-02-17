
#ifndef _SENSOR_ITEM_H
#define _SENSOR_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMSensor.h>

using namespace nidas::core;


class SensorItem : public NidasItem
{

public:
    SensorItem(DSMSensor *dsm, int row, NidasModel *model, NidasItem *parent = 0) :
        NidasItem(dsm,row,model,parent) {}

    ~SensorItem();

    bool removeChild(NidasItem *item) { return false; } // XXX

protected:

        // get/convert to the underlying model pointers
    DSMSensor *getDSMSensor() const {
     if (nidasType == SENSOR)
         return reinterpret_cast<DSMSensor*>(this->nidasObject);
     else return 0;
     }

};

#endif
