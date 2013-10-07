
#include "A2DVariableItem.h"
#include "A2DSensorItem.h"

#include <exceptions/InternalProcessingException.h>

#include <QMessageBox>

using namespace xercesc;


A2DVariableItem::A2DVariableItem(Variable *variable, SampleTag *sampleTag, int row, NidasModel *theModel, NidasItem *parent) 
{
    _variable = variable;
    _gotCalDate = _gotCalVals = false;
    _calDate = _calVals = "";
    _sampleTag = sampleTag;
    _sampleDOMNode = 0;
    _variableDOMNode = 0;
    domNode = 0;
    // Record the item's location within its parent.
    rowNumber = row;
    parentItem = parent;
    model = theModel;
}

A2DVariableItem::~A2DVariableItem()
{
std::cerr<<"A2DVariableItem::~A2DVariableItem\n";
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

  } catch (const std::exception& ex) {
   std::cerr << "~A2DVariableItem caught standard exception: " << ex.what() << "\n";

  } catch (const std::string& ex) {
    std::cerr << "~A2DVariableItem caught string exception: " << ex << "\n";
  } catch (...) {
    // ugh!?!
    std::cerr << "~A2DVariableItem caught exception\n";
  }
}

QString A2DVariableItem::dataField(int column)
{
  if (column == 0) return name();
  if (column == 1) return QString("%1").arg(_variable->getA2dChannel());
  if (column == 2) return QString("%1").arg(_sampleTag->getRate());
  if (column == 3) {
    A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
    DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
    int gain = a2dSensor->getGain(_variable->getA2dChannel());
    int bipolar = a2dSensor->getBipolar(_variable->getA2dChannel());
    if (gain == 4 && bipolar == 0) return QString(" 0-5 V");
    if (gain == 2 && bipolar == 0) return QString(" 0-10 V");
    if (gain == 2 && bipolar == 1) return QString("-5-5 V");
    if (gain == 1 && bipolar == 1) return QString("-10-10 V");
    return QString("N/A");
  }
  // NOTE: it's possible for changes at the sensor level to motivate a
  // re-initialization of the variable converter and calibration file
  // So refresh them every time.
  if (column == 4) {
    QString calString, noCalString="";
    VariableConverter *varConverter = _variable->getConverter(); 
    if (varConverter) {  
       CalFile * calFile = varConverter->getCalFile();  
       if (calFile) {
          std::string calFileName = calFile->getFile();
          if (!_gotCalVals) {
             nidas::util::UTime curTime, calTime;
             nidas::core::Polynomial * poly =  new nidas::core::Polynomial();
             try {
                poly->setCalFile(calFile);
                curTime = nidas::util::UTime();
                curTime.format(true, "%Y%m%d:%H:%M:%S");
                calTime = calFile->search(curTime);
                calTime.format(true, "%Y%m%d:%H:%M:%S");
                poly->readCalFile(calTime.toUsecs());
                calString.append(QString::fromStdString(poly->toString()));
                int lastQ = calString.lastIndexOf(QString::fromStdString("\""));
                calString.insert(lastQ, QString::fromStdString(
                                                 varConverter->getUnits()));
                calString.remove("poly ");
                _calVals = calString.toStdString();
                _gotCalVals = true;
                _calDate = calTime.format(true, "%m/%d/%Y");
                _gotCalDate = true;
             } catch (nidas::util::IOException &e) {
                std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
                return QString("ERROR: File is Missing");
             } catch (nidas::util::ParseException &e) {
                std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
                return QString("ERROR: Parse Failed");
             } catch (...) {
                std::cerr<<__func__<<":ERROR: Unexpected Cal access error\n";
                return QString("ERROR: Cal Access");
             }
          } else {
             calString = QString::fromStdString(_calVals);
          }
       } else
          calString.append(QString::fromStdString(varConverter->toString()));
       return calString;
    } else return noCalString;
  }
  // NOTE: it's possible for changes at the sensor level to motivate a
  // re-initialization of the variable converter and calibration file
  // So refresh them every time.
  if (column == 5) {
    VariableConverter * varConverter = _variable->getConverter(); 
    if (varConverter) {
      CalFile * calFile = varConverter->getCalFile();
      if (calFile) {
        std::string calFileName = calFile->getFile();
        return QString::fromStdString(calFileName);
      } else
        return QString("XML");
    }
    else
        return QString("N/A");
  }
  if (column == 6) {
    VariableConverter * varConverter = _variable->getConverter(); 
    if (varConverter) {
      CalFile * calFile = varConverter->getCalFile();
      if (calFile) {
        if (!_gotCalDate) {
           nidas::util::UTime curTime, calTime;
           nidas::core::Polynomial * poly =  new nidas::core::Polynomial();
           try {
             poly->setCalFile(calFile);
             curTime = nidas::util::UTime();
             curTime.format(true, "%Y%m%d:%H:%M:%S");
             calTime = calFile->search(curTime);
             _calDate = calTime.format(true, "%m/%d/%Y");
             _gotCalDate = true;
             return QString::fromStdString(calTime.format(true, "%m/%d/%Y"));
           } catch (nidas::util::IOException &e) {
             std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
             return QString("ERROR: File is Missing");
           } catch (nidas::util::ParseException &e) {
             std::cerr<<__func__<<":ERROR: "<< e.toString()<<"\n";
             return QString("ERROR: Parse Failed");
           }
         } else
            return QString::fromStdString(_calDate);
      } else
        return QString();
    }
  }
  if (column == 7) return QString("%1").arg(_sampleTag->getSampleId());

  return QString();
}


QString A2DVariableItem::name()
{
    return QString::fromStdString(_variable->getName());
}

DOMNode* A2DVariableItem::findSampleDOMNode()
{
std::cerr<<"A2DVariableItem::findSampleDOMNode()\n";
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
     if ( ((std::string)XMLStringConverter(sensorChild->getNodeName()))
                 .find("sample") == std::string::npos ) continue;

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


int A2DVariableItem::getGain()
{
  A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
  DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
  return (a2dSensor->getGain(_variable->getA2dChannel()));
}

int A2DVariableItem::getBipolar()
{
  A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(getParentItem());
  DSMAnalogSensor * a2dSensor = sensorItem->getDSMAnalogSensor();
  return (a2dSensor->getBipolar(_variable->getA2dChannel()));
}

// Return a vector of strings which are the calibration coefficients starting 
// w/offset, then least significant polinomial coef, next least, etc.  The 
// last item is the units string.    Borrows liberally from 
// VariableConverter::fromString methods.
std::vector<std::string> A2DVariableItem::getCalibrationInfo()
{
  // Get the variable's conversion String
  std::vector<std::string> calInfo, noCalInfo;
  noCalInfo.push_back(std::string("No Calibrations Found"));
  std::string calStr, str;
  VariableConverter * varConverter = _variable->getConverter(); 
  if (!varConverter) return noCalInfo;  // there is no conversion string
  calStr = varConverter->toString();
  CalFile * calFile = varConverter->getCalFile();
  if (calFile) calInfo.push_back(std::string("CalFile:"));
  else calInfo.push_back(std::string("XML:"));
  std::istringstream ist(calStr);
  std::string which;
  ist >> which;
  if (ist.eof() || ist.fail() || (which != "linear" && which != "poly")) {
    std::cerr << "Somthing not right with conversion string from " 
              << "variable converter\n";
    return noCalInfo; 
  }

  char cstr[256];

  ist.getline(cstr,sizeof(cstr),'=');
  const char* cp;
  for (cp = cstr; *cp == ' '; cp++);

  if (which=="linear") {

      ist >> str;
      if (ist.eof() || ist.fail())  {
        std::cerr << "Error in linear conversion string from " 
                  << "variable converter\n";
        return noCalInfo;  
      }
      std::string slope, intercept, units;
      if (!strcmp(cp,"slope")) slope = str;
      else if (!strcmp(cp,"intercept")) intercept = str;
      else {
        std::cerr << "Could not find linear slope/intercept in "
                  << "conversion string";
        return noCalInfo; 
      }

      ist.getline(cstr,sizeof(cstr),'=');
      for (cp = cstr; *cp == ' '; cp++);
      ist >> str;
      if (ist.eof() || ist.fail())  {
        std::cerr << "Error in linear conversion string from "
                  << "variable converter\n";
        return noCalInfo;
      }
      if (!strcmp(cp,"slope")) slope = str;
      else if (!strcmp(cp,"intercept")) intercept = str;
      else {
        std::cerr << "Could not find linear slope/intercept in "
                  << "conversion string\n";
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
        std::cerr << "Error in poly conversion string from "
                  << "variable converter\n";
        return noCalInfo;  
      }
      if (!strcmp(cp,"coefs")) {
          for(;;) {
              ist >> str;
              strcpy(cstr, str.c_str());
              if (ist.fail() || !strncmp(cstr,"units=",6)) break;
              calInfo.push_back(str);
          }
      }
      else {
        std::cerr << "Error: Could not find poly coefs in conversion string\n";
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

    } else return noCalInfo;  // Should never happen given earlier testing.
  
}

// getName() then break it up myself
std::string A2DVariableItem::getVarNamePfx() 
{ 
  std::string varName;
  varName = _variable->getName(); 
  unsigned underLoc = varName.find("_");
  if (underLoc != (unsigned)std::string::npos)
    varName.erase(varName.begin()+underLoc, varName.end());

  return varName;
}

std::string A2DVariableItem::getVarNameSfx() 
{ 
  std::string varName;
  varName = _variable->getName(); 
  unsigned underLoc = varName.find("_");
  if (underLoc != (unsigned)std::string::npos)
    varName.erase(varName.begin(), varName.begin()+underLoc+1);
  else
    varName = "";
  return varName;
}

DOMNode* A2DVariableItem::findVariableDOMNode(QString name)
{
  DOMNode * sampleNode = getSampleDOMNode();
std::cerr<<"A2DVariableItem::findVariableDOMNode - sampleNode = " 
         << sampleNode << "\n";

if (!sampleNode) std::cerr<<"Did not find sample node in a2d variable item\n";

  DOMNodeList * variableNodes = sampleNode->getChildNodes();
  if (variableNodes == 0) {
    std::cerr << "getChildNodes returns 0 \n";
    throw InternalProcessingException("A2DVariableItem::findVariableDOMNode - getChildNodes return 0!");     
  }

  DOMNode * variableNode = 0;
  std::string variableName = name.toStdString();
std::cerr<< "in A2DVariableItem::findVariableDOMNode - variable name = " << variableName <<"\n";
std::cerr<< "found: "<<variableNodes->getLength()<<" variable nodes\n";

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

// Change the variable's name element from one name to a new name  
// the old name needs to be used rather than the Nidas variable name as it may
// already have been changed prior to this call.
void A2DVariableItem::setDOMName(QString fromName, std::string toName)
{
std::cerr << "In A2DVariableItem::setDOMName(" << fromName.toStdString() 
          << ", "<< toName << ")\n";
  if (this->findVariableDOMNode(fromName)->getNodeType() 
      != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException(
               "A2DVariableItem::setDOMName - node is not an Element node.");

  xercesc::DOMElement * varElement;
  varElement  = ((xercesc::DOMElement*) this->findVariableDOMNode(fromName));
  if (varElement->hasAttribute((const XMLCh*)XMLStringConverter("name")))
    try {
      varElement->removeAttribute((const XMLCh*)XMLStringConverter("name"));
    } catch (DOMException &e) {
      std::cerr << "exception caught trying to remove name attribute: " <<
                   (std::string)XMLStringConverter(e.getMessage()) << "\n";
    }
  else
    std::cerr << "varElement does not have name attribute ... how odd!\n";

  varElement->setAttribute((const XMLCh*)XMLStringConverter("name"),
                           (const XMLCh*)XMLStringConverter(toName));

}

