/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include "SensorItem.h"
#include "DSMItem.h"
#include "VariableItem.h"
#include "../DeviceValidator.h"

#include <iostream>
#include <fstream>

#include <exceptions/InternalProcessingException.h>

using namespace xercesc;
using namespace std;

SensorItem::SensorItem(DSMSensor *sensor, int row, NidasModel *theModel, 
                       NidasItem *parent) 
{
    _sensor = sensor;
    domNode = 0;

    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

SensorItem::SensorItem(DSMAnalogSensor *sensor, int row, NidasModel *theModel, 
                       NidasItem *parent) 
{
    _sensor = sensor;
    domNode = 0;
    
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
 * prevents parentItem->removeChild() from being called for an already gone 
 * parentItem
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
    for (j=0, it = _sensor->getSampleTagIterator(); it.hasNext();) {
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        for (VariableIterator vt = sample->getVariableIterator(); vt.hasNext(); j++) {
          Variable* variable = (Variable*)vt.next(); // XXX cast from const
          if (j<i) continue; // skip old cached items (after it.next())
          NidasItem *childItem = new VariableItem(variable, sample, j, model, this);
          childItems.append( childItem);
        }
    }

    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

void SensorItem::refreshChildItems()
{
  while (!childItems.empty()) childItems.pop_front();
  int j;
  SampleTagIterator it;
  for (j=0, it = _sensor->getSampleTagIterator(); it.hasNext();) {
    SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
    for (VariableIterator vt = sample->getVariableIterator(); 
                          vt.hasNext(); j++) {
      Variable* variable = (Variable*)vt.next(); // XXX cast from const
      NidasItem *childItem = new VariableItem(variable, sample, j, 
                                              model, this);
      childItems.append( childItem);
    }
  }
}

QString SensorItem::dataField(int column)
{
    switch (column) {
      case 0: 
        return viewName();
      case 1:
        return QString::fromStdString(_sensor->getSuffix());
      case 2:
        return QString::fromStdString(_sensor->getDeviceName());
      case 3: {
        std::string dName=_sensor->getDeviceName();
        DeviceValidator * devVal = DeviceValidator::getInstance();
        if (devVal == 0) {
          cerr << "bad error: device validator is null\n";
          return QString();
        }
        std::string sensorName = viewName().toStdString();
        std::string defDName = devVal->getDevicePrefix(sensorName);
        std::string chan = dName.substr(defDName.size(),dName.size()-defDName.size());
        return QString::fromStdString(chan);
      }
      case 4:
        return QString::fromStdString(getSerialNumberString());
      case 5:
        return QString("(%1,%2)").arg(_sensor->getDSMId()).arg(_sensor->getSensorId());
      /* default: fall thru */
    }

  return QString();
}

QString SensorItem::getBaseName()
{
  if (_sensor->getCatalogName().length() > 0)
    return(QString::fromStdString(_sensor->getCatalogName()));
  else {
    QString className = QString::fromStdString(_sensor->getClassName());
    if (className == "raf.DSMAnalogSensor") return (QString("Analog"));
    return className;
  }
}

QString SensorItem::viewName()
{
  if (_sensor->getCatalogName().length() > 0)
    return(QString::fromStdString(_sensor->getCatalogName()));
  else {
    if (_sensor->getClassName() == "raf.DSMAnalogSensor")
      return QString("Analog");
    if (_sensor->getClassName() == "raf.DSMMesaSensor")
      return QString("MESA");
    return(QString::fromStdString(_sensor->getClassName()));
  }
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
     if (atoi(sSensorId.c_str()) == sensorId) {
       cerr<<"getSensorNode - Found SensorNode with id:" << sSensorId << "\n";
       SensorNode = SensorNodes->item(i);
       break;
     }
  }

  domNode = SensorNode;
  return(SensorNode);
}

/*!
 * \brief find the DOM node for the given sample tag
 *
 *  Search through the Sensor DOM Node children looking for nodes with
 *  the name sample and for every one found, see if the id attribute of 
 *  that node is equal to the sampleId passed in.  Note that we search
 *  through all children of the sensor node because we want the last such
 *  node.  Nidas allows multiple definitions, using the last one as the 
 *  'final' say so.
 */
DOMNode * SensorItem::findSampleDOMNode(unsigned int sampleId)
{
cerr << "SensorItem::findSampleDOMNode\n";

  DOMDocument *domdoc = model->getDOMDocument();
  if (!domdoc) return(0);

  // Sample is part of this Sensor's DOM node
  DOMNode * sensorNode = getDOMNode();
  DOMNodeList * sampleNodes = sensorNode->getChildNodes();
  DOMNode * sampleNode = 0;

  // Search through sample nodes to find our node
  for (XMLSize_t i = 0; i < sampleNodes->getLength(); i++)
  {
     DOMNode * sensorChild = sampleNodes->item(i);
     if ( ((string)XMLStringConverter(sensorChild->getNodeName())).find("sample") == string::npos ) continue;

     XDOMElement xnode((DOMElement *)sampleNodes->item(i));
     const string& sSampleId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sSampleId.c_str()) == sampleId) {
       sampleNode = sampleNodes->item(i);
       //break;  // We actually want the last node so no break!
     }
  }

  return(sampleNode);
}

/*!
 * \brief Re-initialize the nidas model from the DOM element
 *
 */
void SensorItem::fromDOM()
{
  xercesc::DOMNode* dNode = getDOMNode();
  if (dNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("SensorItem::fromDOM - node is not an Element node.");

  xercesc::DOMElement * sensorElement = (xercesc::DOMElement*) dNode;

  this->getDSMSensor()->fromDOMElement(sensorElement);

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

  VariableItem *variableItem = dynamic_cast<VariableItem*>(item);
  string deleteVariableName = variableItem->name().toStdString();

cerr << " deleting Variable" << deleteVariableName << "\n";

  SampleTag *sampleTag = variableItem->getSampleTag();
  if (!sampleTag)
    throw InternalProcessingException("SensorItem::removeChild - null SampleTag");

  // get the DOM node for this Variable's SampleTag
  xercesc::DOMNode *sampleNode = this->findSampleDOMNode(sampleTag->getSampleId());
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
    string deleteSampleIdStr = variableItem->sSampleId();
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
  if (ist.fail())
     throw InternalProcessingException("selected sample id:" + deleteSampleIdStr + " is not an integer");

    // Make a copy of the list of SampleTags since one might be removed.
    list<SampleTag*> tags = sensor->getSampleTags();
    list<SampleTag*>::iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
      SampleTag* sampleTag = *ti;
      if (sampleTag->getSampleId() == iSelSampId)  {
           cerr<<"Removing sample tag with sampleid:"<<iSelSampId<<"\n";
           sensor->removeSampleTag(sampleTag); break; }
    }
  }
  return true;
}

