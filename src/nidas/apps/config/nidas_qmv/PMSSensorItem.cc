/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

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
void PMSSensorItem::updateDOMPMSParams(const std::string & pmsSN, 
                                       const std::string & pmsResltn)
{
std::cerr<< "in SensorItem::" << __func__ << "(" << pmsSN << ")\n";
  if (this->getDOMNode()->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException
           ("A2DSensorItem::updateDOMPMSSN - node is not an Element node.");

  // Look through child nodes for PMS Serial Number and for RESOLUTION
  // and replace them both
  DOMNodeList * sensorChildNodes = this->getDOMNode()->getChildNodes();
  if (sensorChildNodes == 0) {
    std::cerr<< "  getChildNodes returns 0\n";
    throw InternalProcessingException
           ("SensorItem::updateDOMPMSSN - getChildNodes return is 0!");
  }

  DOMNode * pmsSNNode = 0;
  DOMNode * pmsResltnNode = 0;
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
    if (attrName.length() > 0 && attrName == "RESOLUTION")
      pmsResltnNode = sensorChildNode;
  }

  if (pmsSNNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) {
      std::string exStr;
      exStr.append("SensorItem::updateDOMPMSSN - node is not an Element node.");
      throw InternalProcessingException(exStr);
    }

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

  // If we have a RESOLUTION node lets get rid of it and then recreate 
  // it if we have a RESOLUTION defined.
  if (pmsResltnNode) 
    try {
      this->getDOMNode()->removeChild(pmsResltnNode);
    } catch (DOMException &e) {
      std::cerr << "exception caught trying to remove RESOLUTION attribute: "
                << (std::string)XMLStringConverter(e.getMessage()) << "\n";
    }

  // Only add RESOLUTION param if we've actually got a resolution defined
  if (pmsResltn.size() > 0) {
    const XMLCh * paramTagName = 0;
    XMLStringConverter xmlSamp("parameter");
    paramTagName = (const XMLCh *) xmlSamp;
  
    // Create a new DOM element for the param element.
    xercesc::DOMElement* paramElem = 0;
    try {
      paramElem = this->getDOMNode()->getOwnerDocument()->createElementNS(
           DOMable::getNamespaceURI(),
           paramTagName);
    } catch (DOMException &e) {
       cerr << "Node->getOwnerDocument()->createElementNS() threw exception\n";
       throw InternalProcessingException("dsm create new dsm sample element: "+
                              (std::string)XMLStringConverter(e.getMessage()));
    }

    // set up the rate parameter node attributes
    paramElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                              (const XMLCh*)XMLStringConverter("RESOLUTION"));
    paramElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                              (const XMLCh*)XMLStringConverter("int"));
    paramElem->setAttribute((const XMLCh*)XMLStringConverter("value"), 
                              (const XMLCh*)XMLStringConverter(pmsResltn));
  
    this->getDOMNode()->appendChild(paramElem);
  }


  return;
}

std::string PMSSensorItem::getSerialNumberString()
{
  const Parameter * parm = _sensor->getParameter("SerialNumber");
  if (parm) 
    return parm->getStringValue(0);

  return std::string();
}
