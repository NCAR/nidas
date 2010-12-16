
#include "VariableItem.h"
#include "SensorItem.h"

#include <exceptions/InternalProcessingException.h>

#include <iostream>
#include <fstream>

using namespace xercesc;


VariableItem::VariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent) 
{
    _variable = variable;
    _sampleTag = sampleTag;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

VariableItem::~VariableItem()
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
    std::cerr << "~VariableItem caught exception\n";
}
}

QString VariableItem::dataField(int column)
{
  if (column == 0) return name();
  if (column == 1) return QString("%1").arg(_sampleTag->getSampleId());
  if (column == 2) return QString("%1").arg(_sampleTag->getRate());

  if (column == 3) {
    VariableConverter* varConv = _variable->getConverter();
    if (varConv) return QString::fromStdString(varConv->toString());
  }

  return QString();
}

QString VariableItem::name()
{
    return QString::fromStdString(_variable->getName());
}

DOMNode* VariableItem::findVariableDOMNode(QString name)
{
  DOMNode * sampleNode = getSampleDOMNode();
std::cerr<<"A2DVariableItem::findVariableDOMNode - sampleNode = " << sampleNode << "\n";

if (!sampleNode) std::cerr<<"Did not find sample node in a2d variable item";

  DOMNodeList * variableNodes = sampleNode->getChildNodes();
  if (variableNodes == 0) {
    std::cerr << "getChildNodes returns 0 \n";
    throw InternalProcessingException("A2DVariableItem::findVariableDOMNode - getChildNodes return 0!");     
  }

  DOMNode * variableNode = 0;
  std::string variableName = name.toStdString();
std::cerr<< "in A2DVariableItem::findVariableDOMNode - variable name = " << variableName <<"\n";
std::cerr<< "found: "<<variableNodes->getLength()<<" varialbe nodes\n";

  for (XMLSize_t i = 0; i < variableNodes->getLength(); i++)
  {
     DOMNode * variableChild = variableNodes->item(i);
     if (((std::string)XMLStringConverter(variableChild->getNodeName())).find("variable") 
            == std::string::npos ) continue;

     XDOMElement xnode((DOMElement *)variableNodes->item(i));
     const std::string& sVariableName = xnode.getAttributeValue("name");
     if (sVariableName.c_str() == variableName) {
       variableNode = variableNodes->item(i);
       break;
     }
  }

  _variableDOMNode = variableNode;
  return(variableNode);
}

DOMNode* VariableItem::findSampleDOMNode()
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

// Change the variable's name element from one name to a new name  
// the old name needs to be used rather than the Nidas variable name as it may
// already have been changed prior to this call.
void VariableItem::setDOMName(QString fromName, std::string toName)
{
std::cerr << "In A2DVariableItem::setDOMName(" << fromName.toStdString() << ", "<< toName << ")\n";
  if (this->findVariableDOMNode(fromName)->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("A2DVariableItem::setDOMName - node is not an Element node.");     

  xercesc::DOMElement * varElement = ((xercesc::DOMElement*) this->findVariableDOMNode(fromName));
std::cerr << "about to remove Attribute\n";
  if (varElement->hasAttribute((const XMLCh*)XMLStringConverter("name")))
    try {
      varElement->removeAttribute((const XMLCh*)XMLStringConverter("name"));
    } catch (DOMException &e) {
      std::cerr << "exception caught trying to remove name attribute: " <<
                   (std::string)XMLStringConverter(e.getMessage()) << "\n";
    }
  else
    std::cerr << "varElement does not have name attribute ... how odd!\n";
std::cerr << "about to set Attribute\n";
  varElement->setAttribute((const XMLCh*)XMLStringConverter("name"),
                           (const XMLCh*)XMLStringConverter(toName));

}

// Re-initialize the nidas model from the DOM 
void VariableItem::fromDOM()
{
  if (!_variableDOMNode) this->getVariableDOMNode(this->name());
  if (_variableDOMNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("VariableItem::fromDOM - node is not an Element node.");

  xercesc::DOMElement * varElement = (xercesc::DOMElement*) _variableDOMNode;

  this->getVariable()->fromDOMElement(varElement);
}
