
#include "SensorItem.h"
#include "DSMItem.h"

#include <iostream>
#include <fstream>

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


  DOMNodeList * SensorNodes = DSMNode->getChildNodes();

  DOMNode * SensorNode = 0;
  int sensorId = _sensor->getId();

  for (XMLSize_t i = 0; i < SensorNodes->getLength(); i++) 
  {
     DOMNode * dsmChild = SensorNodes->item(i);
     if ( !((string)XMLStringConverter(dsmChild->getNodeName())).find("ensor") ) continue;

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
