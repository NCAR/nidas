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
cerr<<"A2DSensorItem::Child  _sensor is:also not here" << "\n";
    while (!childItems.empty()) childItems.pop_front();
    int j;
    SampleTagIterator it;
    DSMAnalogSensor * a2dsensor = dynamic_cast<DSMAnalogSensor*>(_sensor);
    for (j=0, it = a2dsensor->getSampleTagIterator(); it.hasNext();) {
cerr<<"A2DSensorItem::Child  _sensor is:trying" <<_sensor<<" also j="<<j<< "\n";
        SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
        for (VariableIterator vt = sample->getVariableIterator(); 
             vt.hasNext(); j++) {
          Variable* variable = (Variable*)vt.next(); // XXX cast from const
          NidasItem *childItem = new A2DVariableItem(variable, sample, j, 
                                                     model, this);
          childItems.append( childItem);
        }
    }

cerr<<"A2DSensorItem::Child after loop  _sensor is:trying" <<_sensor<< "\n";
    // we tried to build children but still can't find requested row i
    // probably (always?) when i==0 and this item has no children
    if ((i<0) || (i>=childItems.size())) return 0;

    // we built children, return child i from it
    return childItems[i];
}

void A2DSensorItem::refreshChildItems()
{
  while (!childItems.empty()) childItems.pop_front();
  int j;
  SampleTagIterator it;
cerr<<"refreshChildItems _sensor is:" << _sensor << "\n";
  DSMAnalogSensor * a2dsensor = dynamic_cast<DSMAnalogSensor*>(_sensor);
cerr<<"refreshChildItems now _sensor is:" << _sensor << "\n";
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
}

QString A2DSensorItem::getA2DTempSuffix()
{
cerr<<"geta2dTempSuffix _sensor is:" << _sensor << "\n";
  DSMAnalogSensor * a2dsensor = dynamic_cast<DSMAnalogSensor*>(_sensor);
cerr<<"get a2dTempSuffix now _sensor is:" << _sensor << "\n";
  SampleTagIterator it;
  for (it = a2dsensor->getSampleTagIterator(); it.hasNext();) {
    SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
    for (VariableIterator vt = sample->getVariableIterator();
             vt.hasNext();) {
      Variable* variable = (Variable*)vt.next(); // XXX cast from const
      std::string varName = variable->getName();
      if (!strncmp(varName.c_str(), "A2DTEMP_", 8)) {
        QString qStr = QString::fromStdString(varName);
        std::cerr << "in A2DSensorItem::getA2DTempSuffix() returning suffix: " << qStr.right(qStr.size()-7).toStdString() << "\n";
        return qStr.right(qStr.size()-7);  // Keep the _ in the suffix name
      }
    }
  }
  return QString();
}

void A2DSensorItem::setNidasA2DTempSuffix(std::string a2dTempSfx)
{
cerr<<"setNidasa2dTempSuffix _sensor is:" << _sensor << "\n";
  DSMAnalogSensor * a2dsensor = dynamic_cast<DSMAnalogSensor*>(_sensor);
cerr<<"setNidasa2dTempSuffix now _sensor is:" << _sensor << "\n";
  SampleTagIterator it;
  for (it = a2dsensor->getSampleTagIterator(); it.hasNext();) {
    SampleTag* sample = (SampleTag*)it.next(); // XXX cast from const
    for (VariableIterator vt = sample->getVariableIterator();
             vt.hasNext();) {
      Variable* variable = (Variable*)vt.next(); // XXX cast from const
      std::string varName = variable->getName();
      if (!strncmp(varName.c_str(), "A2DTEMP_", 8)) {
        variable->setName("A2DTEMP_" + a2dTempSfx);
      }
    }
  }
}

std::string A2DSensorItem::getCalFileName() 
{
cerr<<"Before doing anything with _sensor" << "\n";
//cerr<<"AddSensorItem a2dsensorItem:"  << a2dSensorItem<<"\n";
  const map<string,CalFile*>& cfs = _sensor->getCalFiles();
cerr<<"senspr->getCalFiles works" << "\n";

  if (!cfs.empty()) return cfs.begin()->second->getFile();

  return "";
}

std::string A2DSensorItem::getSerialNumberString() 
{
cerr<<"Swerial Num String Before doing anything with _sensor" << _sensor<<"\n";
  const map<string,CalFile*>& cfs = _sensor->getCalFiles();
cerr<<"Serial Num Strirng  _sensor" << _sensor<<"\n";

  if (!cfs.empty())  {
      string cfName = cfs.begin()->second->getFile();
      return cfName.substr(0,cfName.find(".dat"));
  }

  return "";
}

/*!
 * \brief update the A2D Calibration Filename to \a calFileName.
 *
 * Assumes that the DOM already has a calibration file for the A2D Sensor.
 *
 */
void A2DSensorItem::updateDOMCalFile(const std::string & calFileName)
{
std::cerr<< "in A2DSensorItem::updateDOMCalFile(" << calFileName << ")\n";
  if (this->getDOMNode()->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("A2DSensorItem::updateDOMCalFile - node is not an Element node.");

  // Look through child nodes for calfile then replace the name.
  DOMNodeList * sensorChildNodes = this->getDOMNode()->getChildNodes();
  if (sensorChildNodes == 0) {
    std::cerr<< "  getChildNodes returns 0\n";
    throw InternalProcessingException("A2DSensorItem::updateDOMCalFile - getChildNodes return is 0!");
  }

  DOMNode * calFileNode = 0;
  for (XMLSize_t i = 0; i < sensorChildNodes->getLength(); i++)
  {
    DOMNode * sensorChildNode = sensorChildNodes->item(i);
    if (((std::string)XMLStringConverter(sensorChildNode->getNodeName())).find("calfile") == std::string::npos ) continue;

    calFileNode = sensorChildNode;
  }

  if (calFileNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("A2DSensorItem::updateDOMCalFile - node is not an Element node.");

  xercesc::DOMElement * calFileElmt = (xercesc::DOMElement*)calFileNode;
  if (calFileElmt->hasAttribute((const XMLCh*)XMLStringConverter("file")))
    try {
      calFileElmt->removeAttribute((const XMLCh*)XMLStringConverter("file"));
    } catch (DOMException &e) {
      std::cerr << "exception caught trying to remove file attribute: " <<
                   (std::string)XMLStringConverter(e.getMessage()) << "\n";
    }
  else
    std::cerr << "varElement does not have file attribute ... how odd!\n";
  calFileElmt->setAttribute((const XMLCh*)XMLStringConverter("file"),
                           (const XMLCh*)XMLStringConverter(calFileName));

  return;
}

void A2DSensorItem::updateDOMA2DTempSfx(QString oldSfx, std::string newSfx)
{
  // Find the A2DTemperature variable in childItems list
  NidasItem *var;
  A2DVariableItem *a2dVar;
  QList<NidasItem*>::iterator i;
  for (i = childItems.begin(); i!=childItems.end(); ++i) {
    var = *i;
    a2dVar = dynamic_cast<A2DVariableItem*>(var);
    if (!a2dVar) 
      throw InternalProcessingException("SensorItem::child not an A2DVariableItem");
    if (a2dVar->name().contains("A2DTEMP")) {
      QString qStr("A2DTEMP");
      qStr.append(oldSfx);
      std::string str("A2DTEMP");
      str.append(newSfx);
      a2dVar->setDOMName(qStr,str);
    }
  }
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

cerr << "A2DSensorItem::removeChild\n";

  A2DVariableItem *a2dVariableItem = dynamic_cast<A2DVariableItem*>(item);
  string deleteVariableName = a2dVariableItem->name().toStdString();

cerr << "  Remove Variable:" << deleteVariableName << "from all 3 models\n";

  SampleTag *sampleTag = a2dVariableItem->getSampleTag();
  if (!sampleTag)
    throw InternalProcessingException("SensorItem::removeChild - null SampleTag");

  // Grab the sample ID in case the Variable being deleted is 
  // the last in this sample so we can delete the sample as well.
  //string deleteSampleIdStr = a2dVariableItem->sSampleId();
  unsigned int delSampleId = sampleTag->getSampleId();

  // get the DOM node for this Variable's SampleTag
  DOMNode *sampleNode = this->findSampleDOMNode(sampleTag->getSampleId());
  if (!sampleNode) {
    throw InternalProcessingException("Could not find sample DOM node");
  }

  // delete all the matching variable DOM nodes from this Sample's DOM node 
  //   (schema allows overrides/multiples)
  DOMNode* child;
  std::vector<DOMNode*> deletables;
  DOMNodeList* sampleChildren = sampleNode->getChildNodes();
  XMLSize_t numChildren, index;
  numChildren = sampleChildren->getLength();
  int numVarsInSample = 0;
  for (index = 0; index < numChildren; index++ )
  {
      if (!(child = sampleChildren->item(index))) continue;
      if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
      nidas::core::XDOMElement xchild((DOMElement*) child);

      const string& elname = xchild.getNodeName();
      if (elname == "variable")
      {

        numVarsInSample += 1;
        const string & name = xchild.getAttributeValue("name");
        if (name == deleteVariableName)
        {
           deletables.push_back(child);
           numVarsInSample -= 1;
        }
      }
  }

  // Remove a2d variable node(s) from the sample node.
  for (vector<DOMNode*>::iterator it = deletables.begin(); 
                                  it != deletables.end(); ++it) {
           DOMNode* removableChld = sampleNode->removeChild(*it);
           removableChld->release();
  }

  // delete variable from nidas model 
  for (VariableIterator vi = sampleTag->getVariableIterator(); vi.hasNext(); ) {
    const Variable* variable = vi.next();
    if (variable->getName() == deleteVariableName)  {
         sampleTag->removeVariable(variable); break; }
  }

  if (numVarsInSample == 0) {
    // Since this was the last variable for the Sample, delete the sample
    cerr << "  deleting SampleId " << delSampleId << "\n";

    DSMSensor *sensor = this->getDSMSensor();
    if (!sensor)
      throw InternalProcessingException("null Sensor");

    // get the DOM node for this Sensor
    DOMNode *sensorNode = this->getDOMNode();
    if (!sensorNode) {
      throw InternalProcessingException("null sensor DOM node");
    }

    // delete all the matching sample DOM nodes from this Sensor's DOM node 
    //   (schema allows overrides/multiples)
    DOMNode* child;
    DOMNodeList* sensorChildren = sensorNode->getChildNodes();
    XMLSize_t numChildren, index;
    numChildren = sensorChildren->getLength();
    for (index = 0; index < numChildren; index++ )
    {
        if (!(child = sensorChildren->item(index))) continue;
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        nidas::core::XDOMElement xchild((DOMElement*) child);
  
        const string& elname = xchild.getNodeName();
        if (elname == "sample")
        {
  
          const string & id = xchild.getAttributeValue("id");
          cerr << "  found node with name " << elname  << " and id: " << id << endl;
            if (((unsigned int)atoi(id.c_str())) == delSampleId) 
            {
               DOMNode* removableChld = sensorNode->removeChild(child);
               removableChld->release();
            }
        }
    }

    // delete sample from nidas model : move into NidasModel?
    // Make a copy of the list of SampleTags since one might be removed.
    list<SampleTag*> tags = sensor->getSampleTags();
    list<SampleTag*>::iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
      SampleTag* sampleTag = *ti;
      if (sampleTag->getSampleId() == delSampleId)  
      {
           cerr<<"  Removing sample tag with sampleid:"<<delSampleId<<"\n";
           sensor->removeSampleTag(sampleTag); break; 
      }
    }
  }
  
  return true;
}
