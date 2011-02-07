
#include "DSMItem.h"
#include "SensorItem.h"
#include "A2DSensorItem.h"
#include "NidasModel.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;

DSMItem::DSMItem(DSMConfig *dsm, int row, NidasModel *theModel, NidasItem *parent) 
{
    _dsm=dsm;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

DSMItem::~DSMItem()
{
std::cerr << "call to ~DSMItem() \n";
try {
NidasItem *parentItem = this->parent();
if (parentItem)
    parentItem->removeChild(this);
// Actual DSMConfig was deleted by RemoveDSM in nidas::core
} catch (...) {
    // ugh!?!
    cerr << "Caught Exception in ~DSMitem() \n";
}
}

NidasItem * DSMItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    int j;

    SensorIterator it;
    for (j=0, it = _dsm->getSensorIterator(); it.hasNext(); j++) {
        DSMSensor* sensor = it.next();
        if (j<i) continue; // skip old cached items (after it.next())
//std::cerr << "Creating new SensorItem named : " << sensor->getName() << "\n";
        NidasItem *childItem;
        if (sensor->getClassName() == "raf.DSMAnalogSensor") 
          childItem = new A2DSensorItem(dynamic_cast<DSMAnalogSensor*>(sensor), j, model, this);
        else 
          childItem = new SensorItem(sensor, j, model, this);
        childItems.append( childItem);
        }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

/// find the DOM node which defines this DSM
DOMNode *DSMItem::findDOMNode() 
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

  domNode = DSMNode;
  return(DSMNode);
}

QString DSMItem::dataField(int column)
{

  if (column == 0) return name();
  if (column == 1) {
    DSMConfig *dsmConfig = this->getDSMConfig();
    if (!dsmConfig)
      throw InternalProcessingException("null DSMConfig");
    return QString::number(dsmConfig->getId());
  }

  return QString();
}

const QVariant & DSMItem::childLabel(int column) const
{ 
  switch (column) {
    case 0:
      return NidasItem::_Sensor_Label;
    case 1:
      return NidasItem::_Device_Label;
    case 2:
      return NidasItem::_SN_Label;
    case 3:
      return NidasItem::_ID_Label;
    default: 
      return NidasItem::_Name_Label;
    }
}

/*!
 * \brief Re-initialize the nidas model from the DOM element
 *
 */
void DSMItem::fromDOM()
{
  // Make sure we've got the latest domNode
  xercesc::DOMNode* dNode = getDOMNode();
  if (dNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("DSMItem::fromDOM - node is not an Element node.");

  xercesc::DOMElement * dsmElement = (xercesc::DOMElement*) dNode;

cerr<<"DSMItem::fromDOM about to call DSMConfig->fromDOMElement on node...\n";

  DSMConfig* dsm = getDSMConfig();
  dsm->fromDOMElement(dsmElement);

cerr<<"back from DSMConfig->fromDOMElement\n";
  return;
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
cerr << "DSMItem::removeChild\n";
SensorItem *sensorItem = dynamic_cast<SensorItem*>(item);
string deleteDevice = sensorItem->devicename();
cerr << " deleting device " << deleteDevice << "\n";

  DSMConfig *dsmConfig = this->getDSMConfig();
  if (!dsmConfig)
    throw InternalProcessingException("null DSMConfig");

    // get the DOM node for this DSM
  xercesc::DOMNode *dsmNode = this->getDOMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }

    // delete all the matching sensor DOM nodes from this DSM's DOM node 
    //   (schema allows overrides/multiples)
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
      cerr << "found sensor with name " << sensor->getDeviceName() << "\n";
      if (sensor->getDeviceName() == deleteDevice) {
         cerr << "  calling removeSensor()\n";
         dsmConfig->removeSensor(sensor); // do not delete, leave that for ~SensorItem()
         break; // Nidas has only 1 object per sensor, regardless of how many XML has
         }
    }

  return true;
}

QString DSMItem::name()
{
    string loc = _dsm->getLocation();
    string name = _dsm->getName();
    return(QString::fromStdString(loc + " [" + name + "]"));
}
