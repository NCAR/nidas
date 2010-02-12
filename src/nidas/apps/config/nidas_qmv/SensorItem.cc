
#include "SensorItem.h"


SensorItem::~SensorItem()
{
try {
NidasItem *parentItem = this->parent();
if (parentItem) {
    parentItem->removeChild(this);
    }
delete this->getDSMSensor();
} catch (...) {
    // ugh!?!
}
}


