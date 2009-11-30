
#include "Document.h"
#include "configwindow.h"
#include "DSMTableWidget.h"


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

void Document::addSensor(const std::string & sensorIdName, const std::string & device,
                         const std::string & lcId, const std::string & sfx)
{

  const DOMElement * sensorCatElement;
  sensorCatElement = findSensor(sensorIdName);
  if (sensorCatElement == NULL) {
    cerr << "Null sensor DOMElement found for sensor " << sensorIdName << endl;
    return;
  } else {
    cerr << "Found sensor DOMElement for sensor " << sensorIdName << endl;
  }

  DSMDisplayWidget *dsmWidget = _configWindow->getCurrentDSMWidget();
  if (dsmWidget == 0) return;

  DSMTableWidget *dsmTable = dsmWidget->getTable();
  if (dsmTable == 0) return;
  cerr << "past getCurrentDSMTable()\n";
 
  xercesc::DOMNode *dsmNode = dsmTable->getDSMNode();
  if (!dsmNode) dsmNode = dsmWidget->getDSMNode();
  if (!dsmNode) return;
  cerr << "past getDSMNode()\n";

  xercesc::DOMElement* elem = 0;
  try {
     elem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         sensorCatElement->getTagName());
  } catch (...) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     return;
  }

  elem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), (const XMLCh*)XMLStringConverter(sensorIdName));
  elem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), (const XMLCh*)XMLStringConverter(device));
  elem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter(lcId));
  if (!sfx.empty()) elem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), (const XMLCh*)XMLStringConverter(sfx));


  try {
    // adapted from nidas::core::DSMConfig::fromDOMElement()
    // should be factored out of that method into a public method of DSMConfig

    DSMConfig *dsmConfig = dsmTable->getDSMConfig();

    DSMSensor* sensor = dsmConfig->sensorFromDOMElement(elem);

    // check if this is a new DSMSensor for this DSMConfig.
    const std::list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si = std::find(sensors.begin(),sensors.end(),sensor);
    if (si == sensors.end()) dsmConfig->addSensor(sensor);

  _configWindow->parseOtherSingleSensor(sensor,dsmTable);
  _configWindow->parseAnalogSingleSensor(sensor,dsmTable);

  } catch (...) {
    cerr << "hacked dsm sensor adding code crashed\n";
  }

  dsmNode->appendChild(elem);


#if 0
  try {
    Project::destroyInstance(); // clear it out
    Project *project = Project::getInstance(); // start anew

    cerr << "doing fromDOMElement" << endl;
    project->fromDOMElement(domdoc->getDocumentElement());
    cerr << "fromDOMElement done" << endl;
    } catch (nidas::util::Exception & e) {
        cerr << "project->fromDOMElement throws exception: " << e.what() << endl;
        cout << "project->fromDOMElement throws exception: " << e.what() << endl;
        return;
    };
#endif

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
