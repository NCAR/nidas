
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

/*!
 * \brief remove the sample \a item from this Sensor's Nidas and DOM trees
 *
 * current implementation confused between returning bool and throwing exceptions
 * due to refactoring from Document
 *
 */
bool A2DSensorItem::removeChild(NidasItem *item)
{

cerr << "SensorItem::removeChild\n";

  A2DVariableItem *a2dVariableItem = dynamic_cast<A2DVariableItem*>(item);
  string deleteVariableName = a2dVariableItem->name().toStdString();

cerr << " deleting Variable" << deleteVariableName << "\n";

  SampleTag *sampleTag = a2dVariableItem->getSampleTag();
  if (!sampleTag)
    throw InternalProcessingException("SensorItem::removeChild - null SampleTag");

  // get the DOM node for this Variable's SampleTag
  xercesc::DOMNode *sampleNode = this->findSampleDOMNode(sampleTag);
  if (!sampleNode) {
    throw InternalProcessingException("Could not find sample DOM node");
  }

  // delete all the matching variable DOM nodes from this Sample's DOM node 
  //   (schema allows overrides/multiples)
  xercesc::DOMNode* child;
  xercesc::DOMNodeList* sampleChildren = sampleNode->getChildNodes();
  XMLSize_t numChildren, index;
  numChildren = sampleChildren->getLength();
  int numVarsInSample = 0;
  for (index = 0; index < numChildren; index++ )
  {
      if (!(child = sampleChildren->item(index))) continue;
      if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
      nidas::core::XDOMElement xchild((xercesc::DOMElement*) child);

      const string& elname = xchild.getNodeName();
      if (elname == "variable")
      {

        numVarsInSample += 1;
        const string & name = xchild.getAttributeValue("name");
        if (name == deleteVariableName)
        {
           xercesc::DOMNode* removableChld = sampleNode->removeChild(child);
           removableChld->release();
           numVarsInSample -= 1;
        }
      }
  }

  // delete variable from nidas model 
  for (VariableIterator vi = sampleTag->getVariableIterator(); vi.hasNext(); ) {
    const Variable* variable = vi.next();
    if (variable->getName() == deleteVariableName)  {
         sampleTag->removeVariable(variable); break; }
  }

  if (numVarsInSample == 0) {
    // Since this was the last variable for the Sample, delete the sample
    string deleteSampleIdStr = a2dVariableItem->sSampleId();
    cerr << " deleting SampleId" << deleteSampleIdStr << "\n";

    DSMSensor *sensor = this->getDSMSensor();
    if (!sensor)
      throw InternalProcessingException("null Sensor");

    // get the DOM node for this Sensor
    xercesc::DOMNode *sensorNode = this->getDOMNode();
    if (!sensorNode) {
      throw InternalProcessingException("null sensor DOM node");
    }
    cerr << "past getSensorNode()\n";

    // delete all the matching sample DOM nodes from this Sensor's DOM node 
    //   (schema allows overrides/multiples)
    xercesc::DOMNode* child;
    xercesc::DOMNodeList* sensorChildren = sensorNode->getChildNodes();
    XMLSize_t numChildren, index;
    numChildren = sensorChildren->getLength();
    for (index = 0; index < numChildren; index++ )
    {
        if (!(child = sensorChildren->item(index))) continue;
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        nidas::core::XDOMElement xchild((xercesc::DOMElement*) child);
  
        const string& elname = xchild.getNodeName();
        if (elname == "sample")
        {
  
          const string & id = xchild.getAttributeValue("id");
          cerr << "found node with name " << elname  << " and id: " << id << endl;
  
            if (id == deleteSampleIdStr) 
            {
               xercesc::DOMNode* removableChld = sensorNode->removeChild(child);
               removableChld->release();
            }
        }
    }

    // delete sample from nidas model : move into NidasModel?
    istringstream ist(deleteSampleIdStr);
    unsigned int iSelSampId;
    ist >> iSelSampId;
    for (SampleTagIterator si = sensor->getSampleTagIterator(); si.hasNext(); ) {
      const SampleTag* sampleTag = si.next();
      if (ist.fail())
         throw InternalProcessingException("selected sample id:" + deleteSampleIdStr + " is not an integer");
      if (sampleTag->getSampleId() == iSelSampId)  {
           cerr<<"Removing sample tag with sampleid:"<<iSelSampId<<"\n";
           sensor->removeSampleTag(sampleTag); break; }
    }
  }
  
  return true;
}
