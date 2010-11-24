
#include "DSMItem.h"
#include "A2DSensorItem.h"
#include "A2DVariableItem.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;

A2DSensorItem::A2DSensorItem(DSMAnalogSensor *sensor, int row, 
                  NidasModel *theModel, NidasItem *parent) :
      SensorItem(sensor, row, theModel, parent) {}

NidasItem * A2DSensorItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    // Because children are A2D variables, and adding of new variables
    // could be anywhere in the list of variables (and sample ids) , it is 
    // necessary to recreate the list for new child items.
    while (!childItems.empty()) childItems.pop_front();
    int j;
    SampleTagIterator it;
    DSMAnalogSensor * a2dsensor = dynamic_cast<DSMAnalogSensor*>(_sensor);
    for (j=0, it = a2dsensor->getSampleTagIterator(); it.hasNext();) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        for (VariableIterator vt = sample->getVariableIterator(); 
             vt.hasNext(); j++) {
          Variable* variable = (Variable*)vt.next(); // XXX cast from const
          NidasItem *childItem = new A2DVariableItem(variable, sample, j, 
                                                     model, this);
          childItems.append( childItem);
        }
    }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

