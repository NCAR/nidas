
#ifndef _SENSOR_ITEM_H
#define _SENSOR_ITEM_H

#include "NidasItem.h"
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SensorCatalog.h>
#include <nidas/core/CalFile.h>

using namespace nidas::core;


class SensorItem : public NidasItem
{

public:
    SensorItem(DSMSensor *sensor, int row, NidasModel *theModel, NidasItem *parent = 0) ;

    ~SensorItem();

    NidasItem * child(int i);

    bool removeChild(NidasItem *item);

    std::string devicename() { return this->dataField(1).toStdString(); }

    const QVariant & childLabel(int column) const { 
          if (column == 0) return NidasItem::_Variable_Label;
          if (column == 1) return NidasItem::_Channel_Label;
          if (column == 2) return NidasItem::_Sample_Label;
          if (column == 3) return NidasItem::_Rate_Label;
          if (column == 4) return NidasItem::_CalCoef_Label;
    }

    int childColumnCount() const {return 5;}

    QString dataField(int column);

    bool isAnalog() {return _isAnalog;}

    xercesc::DOMNode* getDOMNode() {
        if (domNode)
          return domNode;
        else return domNode=findDOMNode();
        }

// at some point this should be protected.
//protected:  
        // get/convert to the underlying model pointers
    DSMSensor *getDSMSensor() const { return _sensor; }
    xercesc::DOMNode * findSampleDOMNode(SampleTag * sampleTag);

protected:
    xercesc::DOMNode *findDOMNode(); 
    std::string getSerialNumberString(DSMSensor *sensor);
    QString name();

private:
    DSMSensor * _sensor;
    bool _isAnalog;

};

#endif
