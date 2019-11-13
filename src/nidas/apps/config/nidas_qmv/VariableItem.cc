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

#include "VariableItem.h"

#include <exceptions/InternalProcessingException.h>

#include <iostream>

using namespace xercesc;
using namespace std;


VariableItem::VariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent)
{
    _variable = variable;
    _varConverter = variable->getConverter();
    if (_varConverter) {
      _calFile = _varConverter->getCalFile();
      if (_calFile) _calFileName = _calFile->getFile();
      else _calFileName = std::string();
    }
    else {
       _calFile = NULL;
       _calFileName = std::string();
    }
    _gotCalDate = _gotCalVals = false;
    _calDate = _calVals = "";
    _sampleTag = sampleTag;
    _sampleID = sampleTag->getSampleId();
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

VariableItem::~VariableItem()
{
std::cerr<<"VariableItem::~VariableItem\n";
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
  if (column == 1) return QString("%1").arg(_sampleTag->getRate());
  if (column == 2) return getCalValues();
  if (column == 3) return getCalSrc();
  if (column == 4) return getCalDate();
  if (column == 5) return QString("%1").arg(_sampleTag->getSampleId());

  return QString();
}

QString VariableItem::name()
{
    return QString::fromStdString(_variable->getName());
}

// Nidas name will include sensor suffix - if we just want the name as
// defined in XML we must get rid of that suffix.
std::string VariableItem::getBaseName()
{
std::cerr<<"VariableItem::getBaseName()\n";
  // Need to get Sensor Suffix
  SensorItem * sensorItem = dynamic_cast<SensorItem*>(parent());
  if (!sensorItem) return(0);
  DSMSensor * sensor = sensorItem->getDSMSensor();

  std::string suffix = sensor->getSuffix();
  std::string fullName = _variable->getName();

  if (suffix.size() == 0) return fullName;

  std::string baseName;
  size_t pos = fullName.find(suffix);
  baseName = fullName.substr(0,pos);

  return baseName;
}

// Return a vector of strings which are the calibration coefficients starting
// w/offset, then least significant polinomial coef, next least, etc.  The
// last item is the units string.    Borrows liberally from
// VariableConverter::fromString methods.
std::vector<std::string> VariableItem::getCalibrationInfo()
{
std::cerr<<__func__<<"\n";
  // Get the variable's conversion String
  std::vector<std::string> calInfo, noCalInfo;
  noCalInfo.push_back(_variable->getUnits());
  std::string calStr, str;
  VariableConverter* varConv = _variable->getConverter();
  if (!varConv) return noCalInfo;  // there is no conversion string
  calStr = getCalValues().toStdString();

  std::istringstream ist(calStr);
  std::string which;
  ist >> which;
std::cerr<<"in getCalinfo: which ="<<which<<"\n";
  if (ist.eof() || ist.fail() ||
     (which != "linear" && which != "poly" &&
     (!strncmp(which.c_str(),"coefs",5)==0) && which != "ERROR:")) {
    std::cerr << "Somthing not right with conversion string from variable" <<
                 " converter\n";
    return noCalInfo;
  }

  if (which == "ERROR:") {
     calInfo.push_back("ERROR");
     //calInfo.push_back(_variable->getUnits());
     calInfo.push_back(varConv->getUnits());
     return calInfo;
  }

  char cstr[256];

  ist.getline(cstr,sizeof(cstr),'=');
  const char* cp;
  for (cp = cstr; *cp == ' '; cp++);

  if (which=="linear") {

      ist >> str;
      if (ist.eof() || ist.fail())  {
        std::cerr << "Error in linear conversion string from ";
        std::cerr << "variable converter\n";
        return noCalInfo;
      }
      std::string slope, intercept, units;
      if (!strcmp(cp,"slope")) slope = str;
      else if (!strcmp(cp,"intercept")) intercept = str;
      else {
        std::cerr << "Could not find linear slope/intercept in conversion string";
        return noCalInfo;
      }

      ist.getline(cstr,sizeof(cstr),'=');
      for (cp = cstr; *cp == ' '; cp++);
      ist >> str;
      if (ist.eof() || ist.fail())  {
        std::cerr << "Error in linear conversion string from variable converter\n";
        return noCalInfo;
      }
      if (!strcmp(cp,"slope")) slope = str;
      else if (!strcmp(cp,"intercept")) intercept = str;
      else {
        std::cerr << "Could not find linear slope/intercept in conversion string";
        return noCalInfo;
      }

      ist.getline(cstr,sizeof(cstr),'=');
      for (cp = cstr; *cp == ' '; cp++);
      if (!strcmp(cp,"units")) {
          ist.getline(cstr,sizeof(cstr),'"');
          ist.getline(cstr,sizeof(cstr),'"');
          units = std::string(cstr);
      }

      calInfo.push_back(intercept);
      calInfo.push_back(slope);
      calInfo.push_back(units);
      return calInfo;

    } else if (which== "poly")  {

      if (ist.eof() || ist.fail())  {
        std::cerr << "Error in poly conversion string from variable converter\n";
        return noCalInfo;
      }
      if (!strncmp(cp,"coefs",5)) {
          for(;;) {
              ist >> str;
              strcpy(cstr, str.c_str());
              if (ist.fail() || !strncmp(cstr,"units=",6)) break;
              calInfo.push_back(str);
          }
      }
      else {
        std::cerr << "Could not find poly coefs in conversion string";
        return noCalInfo;
      }

      ist.str(str);
      ist.getline(cstr,sizeof(cstr),'=');
      for (cp = cstr; *cp == ' '; cp++);
      if (!strcmp(cp,"units")) {
          ist.getline(cstr,sizeof(cstr),'"');
          ist.getline(cstr,sizeof(cstr),'"');
          calInfo.push_back(std::string(cstr));
      }

      return calInfo;

    } else if (strncmp(which.c_str(),"coefs",5)==0) {
      ist.str(calStr);
std::cerr<<"parsing coefs line\n  ist elmts: ";
      ist.getline(cstr,sizeof(cstr),'=');
      ist.getline(cstr,sizeof(cstr),' ');
std::cerr<<std::string(cstr)<<" ";
      calInfo.push_back(std::string(cstr));
      for(;;) {
        ist >> str;
std::cerr<<str<<" ";
        strcpy(cstr, str.c_str());
        if (ist.fail() || !strncmp(cstr,"units=",6)) break;
        calInfo.push_back(str);
      }
std::cerr<<"\n";

      ist.str(str);
      ist.getline(cstr,sizeof(cstr),'=');
      for (cp = cstr; *cp == ' '; cp++);
      if (!strcmp(cp,"units")) {
          ist.getline(cstr,sizeof(cstr),'"');
          ist.getline(cstr,sizeof(cstr),'"');
          calInfo.push_back(std::string(cstr));
      }

      return calInfo;
    } 

    return noCalInfo;  // Should never happen given earlier testing.
  
}

DOMNode* VariableItem::findVariableDOMNode(QString name)
{
  DOMNode * sampleNode = getSampleDOMNode();
std::cerr<<"VariableItem::findVariableDOMNode - sampleNode = " << sampleNode << "\n";

if (!sampleNode) std::cerr<<"Did not find sample node in variable item";

  DOMNodeList * variableNodes = sampleNode->getChildNodes();
  if (variableNodes == 0) {
    std::cerr << "getChildNodes returns 0 \n";
    throw InternalProcessingException("A2DVariableItem::findVariableDOMNode - getChildNodes return 0!");     
  }

  DOMNode * variableNode = 0;
  std::string variableName = name.toStdString();
  std::cerr<< "in A2DVariableItem::findVariableDOMNode - variable name = " 
         << variableName <<"\n";
  std::cerr<< "found: "<<variableNodes->getLength()<<" variable nodes\n";

  for (XMLSize_t i = 0; i < variableNodes->getLength(); i++)
  {
     DOMNode * variableChild = variableNodes->item(i);
     if (((std::string)XMLStringConverter(variableChild->getNodeName())).
            find("variable") == std::string::npos ) continue;

     XDOMElement xnode((DOMElement *)variableNodes->item(i));
     const std::string& sVariableName = xnode.getAttributeValue("name");
     if (sVariableName.c_str() == variableName) {
       variableNode = variableNodes->item(i);
       //break;
     }
  }

  _variableDOMNode = variableNode;
  return(variableNode);
}

DOMNode* VariableItem::findSampleDOMNode()
{
  std::cerr<<__func__<<"\n";
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
     if ( ((std::string)XMLStringConverter(sensorChild->getNodeName())).
                        find("sample") == std::string::npos ) continue;

     XDOMElement xnode((DOMElement *)sampleNodes->item(i));
     const std::string& sSampleId = xnode.getAttributeValue("id");
     // Here we must treat the sample id as does the SampleTag class
     // i.e. treating 0 prefixed values as octal and 0x prefixed as hex
     std::istringstream ist(sSampleId);
     unsigned int val;
     ist.unsetf(ios::dec);
     ist >> val;
     if (val == sampleId) {
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
  std::cerr << "In A2DVariableItem::setDOMName(" << fromName.toStdString() 
          << ", "<< toName << ")\n";
  if (this->findVariableDOMNode(fromName)->getNodeType() 
      != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException(
               "A2DVariableItem::setDOMName - node is not an Element node.");

  xercesc::DOMElement * varElement = ((xercesc::DOMElement*) 
                                       this->findVariableDOMNode(fromName));
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

void VariableItem::clearVarItem() 
{
  _variableDOMNode = NULL; 
  _varConverter=NULL;
  _sampleDOMNode = NULL;
  _calFile = NULL;
  _calFileName = std::string();
  _gotCalDate = _gotCalVals = false;
  _calDate = _calVals = "";
  return;
}

// Re-initialize the nidas model from the DOM 
void VariableItem::fromDOM()
{
  //this->getVariableDOMNode(this->name());
  if (!_variableDOMNode) {
    std::cerr<<"VariableItem::fromDOM getting new node\n";
    this->getVariableDOMNode(this->name());
  }
  if (_variableDOMNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException(
                      "VariableItem::fromDOM - node is not an Element node.");

  xercesc::DOMElement * varElement = (xercesc::DOMElement*) _variableDOMNode;

  _variable->fromDOMElement(varElement);
  _varConverter = _variable->getConverter();
  if (_varConverter) {
    _calFile = _varConverter->getCalFile();
    if (_calFile) _calFileName = _calFile->getFile();
    else _calFileName = std::string();
  }
  else {
     _calFile = NULL;
     _calFileName = std::string();
  }     
  _gotCalDate = _gotCalVals = false;
  _calDate = _calVals = "";

  return;
}

QString VariableItem::getCalValues()
{
  QString calString, noCalString;
  noCalString.append("");
  if (_varConverter) {
     if (_calFile) {
        if (!_gotCalVals) {
           nidas::util::UTime curTime, calTime;
           nidas::core::Polynomial * poly =  new nidas::core::Polynomial();
           try {
std::cerr<<"VarItem: getting cals: from file: "<<_calFileName<<"\n";
              poly->setCalFile(_calFile);
              curTime = nidas::util::UTime();
              curTime.format(true, "%Y%m%d:%H:%M:%S");
              calTime = _calFile->search(curTime);
              calTime.format(true, "%Y%m%d:%H:%M:%S");
std::cerr<<"Varitem:";
std::cerr<<name().toStdString();
std::cerr<<" getting cals: curTime:"<<curTime.format(true, "%m/%d/%Y")<<"  calTime:"<<calTime.format(true, "%m/%d/%Y")<<"\n";
              poly->readCalFile(calTime.toUsecs());
              calString.append(QString::fromStdString(poly->toString()));
              int lastQ = calString.lastIndexOf(QString::fromStdString("\""));
              calString.insert(lastQ, QString::fromStdString(
                                               _varConverter->getUnits()));
              calString.remove("poly ");
              _calVals = calString.toStdString();
              _gotCalVals = true;
              _calDate = calTime.format(true, "%m/%d/%Y");
              _gotCalDate = true;
           } catch (nidas::util::IOException &e) {
              std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
              _calVals = "ERROR: File is Missing";
              _gotCalVals = true;
              return QString::fromStdString(_calVals);
           } catch (nidas::util::ParseException &e) {
              std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
              return QString("ERROR: Parse Failed");
           }
        } else {
           calString = QString::fromStdString(_calVals);
        }
     } else {
        if (!_gotCalVals) {
std::cerr<<"About to call calString.append on Varitem: "<<name().toStdString()<<"\n";
std::cerr<<"  **** varConverter->toString():"<<_varConverter->toString()<<"\n";
           _calVals = _varConverter->toString();
           _gotCalVals = true;
        }
        calString = QString::fromStdString(_calVals);
     }
     return calString;
  } else return noCalString;
}

QString VariableItem::getCalSrc()
{
  if (_varConverter) {
    if (_calFile)
      return QString::fromStdString(_calFileName);
    else
      return QString("XML");
  }
  else
    return QString("N/A");
}

QString VariableItem::getCalDate()
{
  if (_varConverter) {
    if (_calFile) {
      if (!_gotCalDate) {
         nidas::util::UTime curTime, calTime;
         nidas::core::Polynomial * poly =  new nidas::core::Polynomial();
         try {
           poly->setCalFile(_calFile);
           curTime = nidas::util::UTime();
           curTime.format(true, "%Y%m%d:%H:%M:%S");
           calTime = _calFile->search(curTime);
           _calDate = calTime.format(true, "%m/%d/%Y");
           _gotCalDate = true;
           return QString::fromStdString(calTime.format(true, "%m/%d/%Y"));
         } catch (nidas::util::IOException &e) {
           std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
           _calDate = "ERROR: File is Missing";
           _gotCalDate = true;
           return QString::fromStdString(_calDate);
         } catch (nidas::util::ParseException &e) {
           std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
           return QString("ERROR: Parse Failed");
         }
       } else
          return QString::fromStdString(_calDate);
    } else
      return QString();
  }
  return QString();
}

