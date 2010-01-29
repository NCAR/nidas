
#include "Document.h"
#include "configwindow.h"
#include "DSMDisplayWidget.h"
#include "exceptions/InternalProcessingException.h"

#include <sys/param.h>
#include <libgen.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>

#include <nidas/core/XMLParser.h>


using namespace std;
using namespace xercesc;
using namespace nidas::core;



static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
static const XMLCh gNull[] = { chNull };



const char *Document::getDirectory() const
{
static char buf[MAXPATHLEN];

strncpy(buf,filename->c_str(),MAXPATHLEN-1);
return dirname(buf);
}



void Document::writeDocument()
{
LocalFileFormatTarget *target;

    try {
        target = new LocalFileFormatTarget(filename->c_str());
        if (!target) {
            cerr << "target is null" << endl;
            return;
            }
    } catch (...) {
        cerr << "LocalFileFormatTarget new exception" << endl;
        return;
    }

writeDOM(target,domdoc);
delete target;
return;
}



bool Document::writeDOM( XMLFormatTarget * const target, const DOMNode * node )
{
DOMImplementation *domimpl;
DOMImplementationLS *lsimpl;
DOMWriter *myWriter;


    try {
        domimpl = XMLImplementation::getImplementation();
        //DOMImplementation *domimpl = DOMImplementationRegistry::getDOMImplementation(gLS);
    } catch (...) {
        cerr << "getImplementation exception" << endl;
        return(false);
    }
        if (!domimpl) {
            cerr << "xml implementation is null" << endl;
            return(false);
            }

    try {
        lsimpl =
        // (DOMImplementationLS*)domimpl;
         (domimpl->hasFeature(gLS,gNull)) ? (DOMImplementationLS*)domimpl : 0;
    } catch (...) {
        cerr << "hasFeature/cast exception" << endl;
        return(false);
    }

        if (!lsimpl) {
            cerr << "dom implementation is null" << endl;
            return(false);
            }

    try {
        myWriter = lsimpl->createDOMWriter();
        if (!myWriter) {
            cerr << "writer is null" << endl;
            return(false);
            }
    } catch (...) {
        cerr << "createDOMWriter exception" << endl;
        return(false);
    }

        if (myWriter->canSetFeature(XMLUni::fgDOMWRTValidation, true))
            myWriter->setFeature(XMLUni::fgDOMWRTValidation, true);
        if (myWriter->canSetFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true))
            myWriter->setFeature(XMLUni::fgDOMWRTFormatPrettyPrint, true);

        myWriter->setErrorHandler(&errorHandler);

    try {
        if (!myWriter->writeNode(target,*node)) {
            cerr << "writeNode returns false" << endl;
            }
    } catch (...) {
        cerr << "writeNode exception" << endl;
        return(false);
    }

        target->flush();
        myWriter->release();

        return(true);
}

// Return a pointer to the node which defines the DSM of the given id
//DOMNode * Document::getDSMNode(dsm_sample_id_t dsmId)
DOMNode * Document::getDSMNode(unsigned int dsmId)
{
  DOMNodeList * DSMNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("dsm"));
  DOMNode * DSMNode = 0;
  for (XMLSize_t i = 0; i < DSMNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)DSMNodes->item(i));
     const string& sDSMId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sDSMId.c_str()) == dsmId) { 
       cerr<<"getDSMNode - Found DSMNode with id:" << sDSMId << endl;
       DSMNode = DSMNodes->item(i);
     }
  }

  return(DSMNode);

}


void Document::parseFile()
{
        cerr << "Document::parseFile()" << endl;
        if (!filename) return;

        XMLParser * parser = new XMLParser();
    
        // turn on validation
        parser->setDOMValidation(true);
        parser->setDOMValidateIfSchema(true);
        parser->setDOMNamespaces(true);
        parser->setXercesSchema(true);
        parser->setXercesSchemaFullChecking(true);
        parser->setDOMDatatypeNormalization(false);
        parser->setXercesUserAdoptsDOMDocument(true);

        cerr << "parsing: " << *filename << endl;
        domdoc = parser->parse(*filename);
        cerr << "parsed" << endl;
        delete parser;

        Project::destroyInstance(); // clear it out
        Project *project = Project::getInstance(); // start anew

        cerr << "doing fromDOMElement" << endl;
        project->fromDOMElement(domdoc->getDocumentElement());
        cerr << "fromDOMElement done" << endl;
}

const xercesc::DOMElement* Document::findSensor(const std::string & sensorIdName)
{
Project *project = Project::getInstance();

    // Find a sensor based on its ID (i.e. the Name in xml)
    if (sensorIdName.empty()) return(NULL);
    if(!project->getSensorCatalog()) {
        cerr<<"Configuration file doesn't contain a catalog!!"<<endl;
        return(0);
    }
    map<string,xercesc::DOMElement*>::const_iterator mi;
    for (mi = project->getSensorCatalog()->begin();
         mi != project->getSensorCatalog()->end(); mi++) {
        if (mi->first == sensorIdName) {
           return mi->second;
        }
    }
    return(NULL);
}

void Document::deleteSensor()
{

  DSMDisplayWidget *dsmWidget = _configWindow->getCurrentDSMWidget();
  if (dsmWidget == 0) {
    throw InternalProcessingException("null dsm widget");
  }

  std::list <std::string> selectedDevices = dsmWidget->getSelectedSensorDevices();

  xercesc::DOMNode *dsmNode = dsmWidget->getDSMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }

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

        std::list <std::string>::iterator it;
        const std::string & device = xchild.getAttributeValue("devicename");
        cerr << "found node with name " << elname  << " and device: " << device << endl;
        for (it=selectedDevices.begin(); it!=selectedDevices.end(); it++)
        {
          cerr << "  list device: " << *it << endl;
          if (device == *it) 
          {
             xercesc::DOMNode* removableChld = dsmNode->removeChild(child);
             removableChld->release();
          }
        }
      }
  }

  DSMConfig *dsmConfig = dsmWidget->getDSMConfig();
  if (dsmConfig == NULL) throw InternalProcessingException("null DSMConfig");

  std::list <std::string>::iterator it;
  for (it=selectedDevices.begin(); it!=selectedDevices.end(); it++)
  {
    for (SensorIterator si = dsmConfig->getSensorIterator(); si.hasNext(); ) {
      DSMSensor* sensor = si.next();
      if (sensor->getDeviceName() == *it)  { dsmConfig->removeSensor(sensor); break; }
    }
  }

  dsmWidget->deleteSensors(selectedDevices);

}

void Document::addSensor(const std::string & sensorIdName, const std::string & device,
                         const std::string & lcId, const std::string & sfx)
{

  const XMLCh * tagName = 0;
  XMLStringConverter xmlSensor("sensor");
  if (sensorIdName == "Analog") {
    tagName = (const XMLCh *) xmlSensor;
    cerr << "Analog Tag Name is " <<  (std::string)XMLStringConverter(tagName) << endl;   
  } else {
  const DOMElement * sensorCatElement;
  sensorCatElement = findSensor(sensorIdName);
  if (sensorCatElement == NULL) {
    cerr << "Null sensor DOMElement found for sensor " << sensorIdName << endl;
    throw InternalProcessingException("null sensor DOMElement");
  }
  tagName = sensorCatElement->getTagName();
  }

  DSMDisplayWidget *dsmWidget = _configWindow->getCurrentDSMWidget();
  if (dsmWidget == 0) {
    throw InternalProcessingException("null dsm widget");
  }

  xercesc::DOMNode *dsmNode = dsmWidget->getDSMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }
  cerr << "past getDSMNode()\n";

  xercesc::DOMElement* elem = 0;
  try {
     elem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         tagName);
  } catch (DOMException &e) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new sensor element: " + (std::string)XMLStringConverter(e.getMessage()));
     
  }

  if (sensorIdName == "Analog") {
    elem->setAttribute((const XMLCh*)XMLStringConverter("class"), (const XMLCh*)XMLStringConverter("raf.DSMAnalogSensor"));
  } else {
    elem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), (const XMLCh*)XMLStringConverter(sensorIdName));
  }
  elem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), (const XMLCh*)XMLStringConverter(device));
  elem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter(lcId));
  if (!sfx.empty()) elem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), (const XMLCh*)XMLStringConverter(sfx));


    // add sensor to nidas project

    // adapted from nidas::core::DSMConfig::fromDOMElement()
    // should be factored out of that method into a public method of DSMConfig

    DSMConfig *dsmConfig = dsmWidget->getDSMConfig();
    if (dsmConfig == NULL) {
        throw InternalProcessingException("null DSMConfig");
    }

    dsmConfig->setDeviceUnique(true);
    DSMSensor* sensor = dsmConfig->sensorFromDOMElement(elem);
    if (sensor == NULL) {
      throw InternalProcessingException("null sensor(FromDOMElement)");
      
    }

    // check if this is a new DSMSensor for this DSMConfig.
    const std::list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si = std::find(sensors.begin(),sensors.end(),sensor);
    if (si == sensors.end()) dsmConfig->addSensor(sensor); else throw InternalProcessingException ("Found duplicate sensor unexpectedly");

    try {
        dsmConfig->validateSensorAndSampleIds();
        sensor->validate();

        // make sure new sensor works well with old (e.g. var names and suffix)
        // Site::validateVariables() coming soon
        dsmConfig->getSite()->validateVariables();

    } catch (nidas::util::InvalidParameterException &e) {
        dsmConfig->removeSensor(sensor); // validation failed so get it out of nidas Project tree
        throw(e); // notify GUI
    }

  try {
    // add sensor to DOM
    dsmNode->appendChild(elem);
  } catch (DOMException &e) {
     dsmConfig->removeSensor(sensor); // keep nidas Project tree in sync with DOM
     throw InternalProcessingException("add sensor to dsm element: " + (std::string)XMLStringConverter(e.getMessage()));
     
  }

    // add sensor to gui
  _configWindow->parseOtherSingleSensor(sensor,dsmWidget->getOtherTable());
  _configWindow->parseAnalogSingleSensor(sensor,dsmWidget->getAnalogTable());

   printSiteNames();
}

void Document::printSiteNames()
{
Project *project = Project::getInstance();

    for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
        Site * site = si.next();

        cerr << "Site: Name = " << site->getName() << "; Number = " << site->getNumber();
        cerr << "; Suffix = " << site->getSuffix() << "; Project = " << site->getProject()->getName();
        cerr << endl;
        }
}

unsigned int Document::getNextSensorId()
{
  unsigned int maxSensorId = 0;

  DSMDisplayWidget *dsmWidget = _configWindow->getCurrentDSMWidget();
  if (dsmWidget == 0) return 0;

  DSMConfig *dsmConfig = dsmWidget->getDSMConfig();
  if (dsmConfig == NULL) return 0;

  const std::list<DSMSensor*>& sensors = dsmConfig->getSensors();
  for (list<DSMSensor*>::const_iterator si = sensors.begin();si != sensors.end(); si++) 
  {
    if ((*si)->getSensorId() > maxSensorId) maxSensorId = (*si)->getSensorId();
  }

    maxSensorId += 200;
    return maxSensorId;
}
