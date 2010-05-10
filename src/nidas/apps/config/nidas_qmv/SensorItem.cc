
#include "SensorItem.h"
#include "DSMItem.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;

SensorItem::SensorItem(DSMSensor *sensor, int row, NidasModel *theModel, NidasItem *parent) 
{
    _sensor = sensor;
    domNode = 0;

    // Determine if we've got an analog card sensor type
    _isAnalog = false;
    if (sensor->getClassName() == "raf.DSMAnalogSensor") _isAnalog = true;

    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

SensorItem::~SensorItem()
{
  try {
    NidasItem *parentItem = this->parent();
    if (parentItem) {
      parentItem->removeChild(this);
    }

   // Actual DSMSensor object was deleted in removeSensor() in nidas::core

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
    cerr << "~SensorItem caught exception\n";
}
}

NidasItem * SensorItem::child(int i)
{
    if ((i>=0) && (i<childItems.size()))
        return childItems[i];

    int j;

    SampleTagIterator it;
    for (j=0, it = _sensor->getSampleTagIterator(); it.hasNext(); j++) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        if (j<i) continue; // skip old cached items (after it.next())
        NidasItem *childItem = new SampleItem(sample, j, model, this);
        childItems.append( childItem);
    }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

QString SensorItem::dataField(int column)
{
  //if (column == 0) return name();

    switch (column) {
      case 0: 
        return name();
      case 1:
        return QString::fromStdString(_sensor->getDeviceName());
      case 2:
        return QString::fromStdString(getSerialNumberString(_sensor));
      case 3:
        return QString("(%1,%2)").arg(_sensor->getDSMId()).arg(_sensor->getSensorId());
      /* default: fall thru */
    }

  return QString();
}

string SensorItem::getSerialNumberString(DSMSensor *sensor)
// maybe move this to a helper class
{
    const Parameter * parm = _sensor->getParameter("SerialNumber");
    if (parm) 
        return parm->getStringValue(0);

    CalFile *cf = _sensor->getCalFile();
    if (cf)
        return cf->getFile().substr(0,cf->getFile().find(".dat"));

return(string());
}


QString SensorItem::name()
{
    if (_sensor->getCatalogName().length() > 0)
        return(QString::fromStdString(_sensor->getCatalogName()+_sensor->getSuffix()));
    else return(QString::fromStdString(_sensor->getClassName()+_sensor->getSuffix()));
}

/// find the DOM node which defines this Sensor
DOMNode * SensorItem::findDOMNode()
{
cerr<<"SensorItem::findDOMNode\n";
  //DSMConfig *dsmConfig = getDSMConfig();
  //if (dsmConfig == NULL) return(0);
  DOMDocument *domdoc = model->getDOMDocument();
  if (!domdoc) return(0);

  //DOMNodeList * SiteNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("site"));
  // XXX also check "aircraft"

  // Get the DOM Node of the DSM to which I belong
  DSMItem * dsmItem = dynamic_cast<DSMItem*>(parent());
  if (!dsmItem) return(0);
  DOMNode * DSMNode = dsmItem->getDOMNode();

  //for (XMLSize_t i = 0; i < SiteNodes->getLength(); i++) 
  //{
     //XDOMElement xnode((DOMElement *)SiteNodes->item(i));
     //const string& sSiteName = xnode.getAttributeValue("name");
     //if (sSiteName == dsmConfig->getSite()->getName()) { 
       //cerr<<"getSiteNode - Found SiteNode with name:" << sSiteName << endl;
       //SiteNode = SiteNodes->item(i);
       //break;
     //}
  //}


cerr<< "getting a list of sensors for DSM\n";
  DOMNodeList * SensorNodes = DSMNode->getChildNodes();

  DOMNode * SensorNode = 0;
  int sensorId = _sensor->getSensorId();

  for (XMLSize_t i = 0; i < SensorNodes->getLength(); i++) 
  {
     DOMNode * dsmChild = SensorNodes->item(i);
     if ( ((string)XMLStringConverter(dsmChild->getNodeName())).find("ensor") == string::npos ) continue;

     XDOMElement xnode((DOMElement *)SensorNodes->item(i));
     const string& sSensorId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sSensorId.c_str()) == sensorId) { 
       cerr<<"getSensorNode - Found SensorNode with id:" << sSensorId << "\n";
       SensorNode = SensorNodes->item(i);
       break;
     }
  }

  domNode = SensorNode;
  return(SensorNode);
}

/*!
 * \brief remove the sample \a item from this Sensor's Nidas and DOM trees
 *
 * current implementation confused between returning bool and throwing exceptions
 * due to refactoring from Document
 *
 */
bool SensorItem::removeChild(NidasItem *item)
{
cerr << "SensorItem::removeChild\n";
SampleItem *sampleItem = dynamic_cast<SampleItem*>(item);
string deleteSampleIdStr = sampleItem->sSampleId();
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

  return true;
}
