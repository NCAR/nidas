
#ifndef _PMSSENSOR_ITEM_H
#define _PMSSENSOR_ITEM_H

#include "SensorItem.h"
#include <nidas/dynld/raf/DSMAnalogSensor.h>
#include <nidas/core/SensorCatalog.h>
#include <nidas/core/CalFile.h>

using namespace nidas::core;
using namespace nidas::dynld::raf;

class PMSSensorItem : public SensorItem
{

public:
    PMSSensorItem(DSMSensor *sensor, int row, NidasModel *theModel, 
                  NidasItem *parent) ;

    void updateDOMPMSSN(const std::string & pmsSN);

    std::string getSerialNumberString();

private:

};

#endif
