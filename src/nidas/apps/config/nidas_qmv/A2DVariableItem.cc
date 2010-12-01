
#include "A2DVariableItem.h"
#include "A2DSensorItem.h"

#include <iostream>
#include <fstream>

using namespace xercesc;


A2DVariableItem::A2DVariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent) 
{
    _variable = variable;
    _sampleTag = sampleTag;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

A2DVariableItem::~A2DVariableItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

   // delete this->getVariable();  // Already done by nidas!

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
    std::cerr << "~A2DVariableItem caught exception\n";
}
}

QString A2DVariableItem::dataField(int column)
{
  if (column == 0) return name();
  if (column == 1) return QString("%1").arg(_variable->getA2dChannel());
  if (column == 2) return QString("%1").arg(_sampleTag->getSampleId());
  if (column == 3) return QString("%1").arg(_sampleTag->getRate());
  if (column == 4) {
    A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
    DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
    int gain = a2dSensor->getGain(_variable->getA2dChannel());
    int bipolar = a2dSensor->getBipolar(_variable->getA2dChannel());
    if (gain == 4 && bipolar == 0) return QString(" 0-5 V");
    if (gain == 2 && bipolar == 0) return QString(" 0-10 V");
    if (gain == 2 && bipolar == 1) return QString("-5-5 V");
    if (gain == 1 && bipolar == 1) return QString("-10-10 V");
    return QString("?");
  }
  if (column == 5) {
    VariableConverter* varConv = _variable->getConverter();
    if (varConv) return QString::fromStdString(varConv->toString());
  }

  return QString();
}

QString A2DVariableItem::name()
{
    return QString::fromStdString(_variable->getName());
}

DOMNode* A2DVariableItem::findSampleDOMNode()
{
  DOMDocument *domdoc = model->getDOMDocument();
  if (!domdoc) return(0);

  // Get the DOM Node of the Sensor to which I belong
  SensorItem * sensorItem = dynamic_cast<SensorItem*>(parent());
  if (!sensorItem) return(0);
  DOMNode * sensorNode = sensorItem->getDOMNode();

  DOMNodeList * sampleNodes = sensorNode->getChildNodes();

  DOMNode * sampleNode = 0;
  unsigned int sampleId = _sampleTag->getSampleId();

  for (XMLSize_t i = 0; i < sampleNodes->getLength(); i++)
  {
     DOMNode * sensorChild = sampleNodes->item(i);
     if ( ((std::string)XMLStringConverter(sensorChild->getNodeName())).find("sample") == std::string::npos ) continue;

     XDOMElement xnode((DOMElement *)sampleNodes->item(i));
     const std::string& sSampleId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sSampleId.c_str()) == sampleId) {
       sampleNode = sampleNodes->item(i);
       break;
     }
  }

  _sampleDOMNode = sampleNode;
  return(sampleNode);
}

int A2DVariableItem::getGain()
{
  A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
  DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
  return (a2dSensor->getGain(_variable->getA2dChannel()));
}

int A2DVariableItem::getBipolar()
{
  A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
  DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
  return (a2dSensor->getBipolar(_variable->getA2dChannel()));
}

