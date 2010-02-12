
#include "DSMItem.h"
#include "SensorItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;



DSMItem::~DSMItem()
{
try {
NidasItem *parentItem = this->parent();
if (parentItem)
    parentItem->removeChild(this);
delete this->getDSMConfig();
} catch (...) {
    // ugh!?!
}
}



/// find the DOM node which defines this DSM
DOMNode *DSMItem::findDOMNode() const
{
DSMConfig *dsmConfig = getDSMConfig();
if (dsmConfig == NULL) return(0);
DOMDocument *domdoc = model->getDOMDocument();
if (!domdoc) return(0);

  DOMNodeList * SiteNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("site"));
  // XXX also check "aircraft"

  DOMNode * SiteNode = 0;
  for (XMLSize_t i = 0; i < SiteNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)SiteNodes->item(i));
     const string& sSiteName = xnode.getAttributeValue("name");
     if (sSiteName == dsmConfig->getSite()->getName()) { 
       cerr<<"getSiteNode - Found SiteNode with name:" << sSiteName << endl;
       SiteNode = SiteNodes->item(i);
       break;
     }
  }


  DOMNodeList * DSMNodes = SiteNode->getChildNodes();

  DOMNode * DSMNode = 0;
  int dsmId = dsmConfig->getId();

  for (XMLSize_t i = 0; i < DSMNodes->getLength(); i++) 
  {
     DOMNode * siteChild = DSMNodes->item(i);
     if ((string)XMLStringConverter(siteChild->getNodeName()) != string("dsm")) continue;

     XDOMElement xnode((DOMElement *)DSMNodes->item(i));
     const string& sDSMId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sDSMId.c_str()) == dsmId) { 
       cerr<<"getDSMNode - Found DSMNode with id:" << sDSMId << endl;
       DSMNode = DSMNodes->item(i);
       break;
     }
  }

return(DSMNode);
}



/*!
 * \brief remove the sensor \a item from this DSM's Nidas and DOM trees
 *
 * current implementation confused between returning bool and throwing exceptions
 * due to refactoring from Document
 *
 */
bool DSMItem::removeChild(NidasItem *item)
{
//SensorItem *sensorItem = qobject_cast<SensorItem*>(item);
//if (!sensorItem) return false;
//string deleteDevice = sensorItem->devicename();
string deleteDevice = item->dataField(1).toStdString(); // XXX replace with above when SensorItem is done

  DSMConfig *dsmConfig = this->getDSMConfig();
  if (!dsmConfig)
    throw InternalProcessingException("null DSMConfig");

    // get the DOM node for this DSM
  xercesc::DOMNode *dsmNode = this->getDOMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }

    // delete all the sensor DOM nodes from this DSM's DOM node (schema allows overrides/multiples)
  xercesc::DOMNode* child;
  xercesc::DOMNodeList* dsmChildren = dsmNode->getChildNodes();
  XMLSize_t numChildren, index;
  numChildren = dsmChildren->getLength();
  for (index = 0; index < numChildren; index++ )
  {
      if (!(child = dsmChildren->item(index))) continue;
      if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
      nidas::core::XDOMElement xchild((xercesc::DOMElement*) child);

      const string& elname = xchild.getNodeName();
      if (elname == "sensor" ||
          elname == "serialSensor" ||
          elname == "arincSensor" ||  
          elname == "irigSensor" ||   // not needed, identical to <sensor> in schema
          elname == "lamsSensor" ||   // not needed, identical to <sensor> in schema
          elname == "socketSensor")
      {

        const string & device = xchild.getAttributeValue("devicename");
        cerr << "found node with name " << elname  << " and device: " << device << endl;

          if (device == deleteDevice) 
          {
             xercesc::DOMNode* removableChld = dsmNode->removeChild(child);
             removableChld->release();
          }
      }
  }

    // delete sensor from nidas model (Project tree)
    for (SensorIterator si = dsmConfig->getSensorIterator(); si.hasNext(); ) {
      DSMSensor* sensor = si.next();
      if (sensor->getDeviceName() == deleteDevice) {
         dsmConfig->removeSensor(sensor); // do not delete, leave that for ~SensorItem()
         break; // Nidas has only 1 object per sensor, regardless of how many XML has
         }
    }

return true;
}
