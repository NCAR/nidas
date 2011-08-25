
#include "DSMItem.h"
#include "PMSSensorItem.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;

PMSSensorItem::PMSSensorItem(DSMSensor *sensor, int row, 
                  NidasModel *theModel, NidasItem *parent) :
      SensorItem(sensor, row, theModel, parent) {}

/*!
 * \brief update the PMS Serial Number to \a pmsSN
 *
 * Assumes that the DOM already has a PMS Serial Number for the PMS Sensor.
 *
 */
void PMSSensorItem::updateDOMPMSSN(const std::string & pmsSN)
{
std::cerr<< "in SensorItem::" << __func__ << "(" << pmsSN << ")\n";
  if (this->getDOMNode()->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException
           ("A2DSensorItem::updateDOMPMSSN - node is not an Element node.");

  // Look through child nodes for PMS Serial Number then replace the name.
  DOMNodeList * sensorChildNodes = this->getDOMNode()->getChildNodes();
  if (sensorChildNodes == 0) {
    std::cerr<< "  getChildNodes returns 0\n";
    throw InternalProcessingException
           ("SensorItem::updateDOMPMSSN - getChildNodes return is 0!");
  }

  DOMNode * sensorChildNode = 0;
  DOMNode * pmsSNNode = 0;
  for (XMLSize_t i = 0; i < sensorChildNodes->getLength(); i++)
  {
    DOMNode * sensorChildNode = sensorChildNodes->item(i);
    if (((std::string)XMLStringConverter(sensorChildNode->getNodeName())).find("parameter") == std::string::npos ) continue;

    // find the name ="SerialNumber" attribute
    if (sensorChildNode->getNodeType() != DOMNode::ELEMENT_NODE)
      throw InternalProcessingException("SensorItem::updateDOMPMSSN - node is not an Element node.");

    DOMElement * sensorChildElement = (DOMElement*) sensorChildNode;
    XDOMElement xnode(sensorChildElement);
    const string& attrName = xnode.getAttributeValue("name");
    if (attrName.length() > 0 && attrName == "SerialNumber") 
      pmsSNNode = sensorChildNode;
  }

  if (pmsSNNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("SensorItem::updateDOMPMSSN - node is not an Element node.");

  xercesc::DOMElement * pmsSNElmt = (xercesc::DOMElement*)pmsSNNode;
  if (pmsSNElmt->hasAttribute((const XMLCh*)XMLStringConverter("value")))
    try {
      pmsSNElmt->removeAttribute((const XMLCh*)XMLStringConverter("value"));
    } catch (DOMException &e) {
      std::cerr << "exception caught trying to remove SerialNumber attribute: "
                << (std::string)XMLStringConverter(e.getMessage()) << "\n";
    }
  else
    std::cerr << "param does not have SerialNumber attribute ... how odd!\n";
  pmsSNElmt->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter(pmsSN));

  return;
}

std::string PMSSensorItem::getSerialNumberString()
{
  const Parameter * parm = _sensor->getParameter("SerialNumber");
  if (parm) 
    return parm->getStringValue(0);

  return std::string();
}
