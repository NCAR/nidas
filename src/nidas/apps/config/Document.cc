
#include "Document.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"

#include <sys/param.h>
#include <libgen.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>

#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMSensor.h>

#include <iostream>
#include <set>
#include <vector>

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
            // build DOM tree
        domdoc = parser->parse(*filename);
        cerr << "parsed" << endl;
        delete parser;

        Project::destroyInstance(); // clear it out
        Project * project = Project::getInstance(); // start anew

        cerr << "doing fromDOMElement" << endl;
            // build Project tree
        project->fromDOMElement(domdoc->getDocumentElement());
        cerr << "fromDOMElement done" << endl;
}

string Document::getProjectName() const
{
    Project *project;
    project = Project::getInstance();
    return (project->getName());
}

void Document::setProjectName(string projectName)
{
  // Set the project name in the DOM tree
  NidasModel *model = _configWindow->getModel();
  ProjectItem * projectItem = dynamic_cast<ProjectItem*>(model->getRootItem());
  if (!projectItem)
    throw InternalProcessingException("Model root index is not a Project.");

  // Set the project name in the DOM tree
  xercesc::DOMNode * projectNode = projectItem->getDOMNode();
  if (!projectNode)  {
    throw InternalProcessingException("null Project DOM node");
  }

  if (projectNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) 
    throw InternalProcessingException("Project DOM Node is not an Element Node!");
  xercesc::DOMElement * projectElement = ((xercesc::DOMElement*) projectNode);

  //xercesc::DOMElement * projectElement = dynamic_cast<DOMElement*>(projectNode);
  //if (!projectElement)
  //  throw InternalProcessingException("Project DOM Node is not an Element Node!");
  
  projectElement->removeAttribute((const XMLCh*)XMLStringConverter("name")); 
  projectElement->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                              (const XMLCh*)XMLStringConverter(projectName));

  // Now set the project name in the nidas tree
  //  NOTE: need to do this after changing the DOM attribute as ProjectItem uses old name
  //  in setting itself up.
  Project *project =  projectItem->getProject();
  project->setName(projectName);

  return;
}

/*!
 * \brief Find a sensor in Project's DOM sensor catalog
 *        based on its XML attribute "ID" (i.e. the Name)
 *
 * \return DOMElement pointer from the Sensor Catalog or NULL
 */
const xercesc::DOMElement* Document::findSensor(const std::string & sensorIdName)
{
Project *project = Project::getInstance();

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
cerr<<"entering Document::addSensor about to make call to _configWindow->getModel()" 
      " configwindow address = "<< _configWindow <<"\n";
  NidasModel *model = _configWindow->getModel();
  DSMItem* dsmItem = dynamic_cast<DSMItem*>(model->getCurrentRootItem());
  if (!dsmItem)
    throw InternalProcessingException("Current root index is not a DSM.");

  DSMConfig *dsmConfig = dsmItem->getDSMConfig();
  if (!dsmConfig)
    throw InternalProcessingException("null DSMConfig");

// gets XML tag name for the selected sensor
  const XMLCh * tagName = 0;
  XMLStringConverter xmlSensor("sensor");
  if (sensorIdName == "Analog") {
    tagName = (const XMLCh *) xmlSensor;
    cerr << "Analog Tag Name is " <<  (std::string)XMLStringConverter(tagName) << endl;   
  } else { // look for the sensor ID in the catalog
    const DOMElement * sensorCatElement;
    sensorCatElement = findSensor(sensorIdName);
    if (sensorCatElement == NULL) {
        cerr << "Null sensor DOMElement found for sensor " << sensorIdName << endl;
        throw InternalProcessingException("null sensor DOMElement");
        }
    tagName = sensorCatElement->getTagName();
  }

// get the DOM node for this DSM
  xercesc::DOMNode *dsmNode = dsmItem->getDOMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }
  cerr << "past getDSMNode()\n";

    // create a new DOM element
  xercesc::DOMElement* elem = 0;
  try {
     elem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         tagName);
  } catch (DOMException &e) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new sensor element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

    // setup the new DOM element from user input
  if (sensorIdName == "Analog") {
    elem->setAttribute((const XMLCh*)XMLStringConverter("class"), (const XMLCh*)XMLStringConverter("raf.DSMAnalogSensor"));
  } else {
    elem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), (const XMLCh*)XMLStringConverter(sensorIdName));
  }
  elem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), (const XMLCh*)XMLStringConverter(device));
  elem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter(lcId));
  if (!sfx.empty()) elem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), (const XMLCh*)XMLStringConverter(sfx));

  // If we've got an analog sensor then we need to set up a sample and variable for it
  if (sensorIdName ==  "Analog") {
    addSampAndVar(elem, dsmNode);
  }

// add sensor to nidas project

    // adapted from nidas::core::DSMConfig::fromDOMElement()
    // should be factored out of that method into a public method of DSMConfig

    dsmConfig->setDeviceUnique(true);
    DSMSensor* sensor = dsmConfig->sensorFromDOMElement(elem);
    if (sensor == NULL)
      throw InternalProcessingException("null sensor(FromDOMElement)");

    // check if this is a new DSMSensor for this DSMConfig.
    const std::list<DSMSensor*>& sensors = dsmConfig->getSensors();
    list<DSMSensor*>::const_iterator si = std::find(sensors.begin(),sensors.end(),sensor);
    if (si == sensors.end())
         dsmConfig->addSensor(sensor);
    else throw InternalProcessingException ("Found duplicate sensor unexpectedly");

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


    // add sensor to DOM
  try {
    dsmNode->appendChild(elem);
  } catch (DOMException &e) {
     dsmConfig->removeSensor(sensor); // keep nidas Project tree in sync with DOM
     throw InternalProcessingException("add sensor to dsm element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

    // update Qt model
    // XXX returns bool
  model->appendChild(dsmItem);

   printSiteNames();
}


void Document::printSiteNames()
{

cerr << "printSiteNames " << endl;
Project *project = Project::getInstance();

    for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
        Site * site = si.next();

        cerr << "Site: Name = " << site->getName() << "; Number = " << site->getNumber();
        cerr << "; Suffix = " << site->getSuffix() << "; Project = " << site->getProject()->getName();
        cerr << endl;
        }
}

unsigned int Document::validateDsmInfo(Site *site, const std::string & dsmName, const std::string & dsmId)
{

  // Check that id is legit
  unsigned int iDsmId;
  if (dsmId.length() > 0) {
    istringstream ist(dsmId);
    ist >> iDsmId;
    if (ist.fail()) throw n_u::InvalidParameterException(
        string("dsm") + ": " + dsmName," id",dsmId);
  }

  // Make sure that the dsmName and dsmId are unique for this Site
  set<int> dsm_ids;
  set<string> dsm_names;
  int j;
  DSMConfigIterator it;
  for (j=0, it = site->getDSMConfigIterator(); it.hasNext(); j++) {
    DSMConfig * dsm = (DSMConfig*)(it.next()); // XXX cast from const
    // The following two if clauses would be really bizzarre, but lets check anyway
    // TODO: these should actually get the DMSItem and delete it.
    //   since this situation should never arise, lets not worry for now and just delete the dsm.
    if (!dsm_ids.insert(dsm->getId()).second) {
      ostringstream ost;
      ost << dsm->getId();
      delete dsm;
      throw InternalProcessingException("Existing dsm id"+ost.str()+
              "is not unique: This should NOT happen!");
    //  throw n_u::InvalidParameterException("dsm id",
    //          ost.str(),"is not unique: This should NOT happen!");
    }
    if (!dsm_names.insert(dsm->getName()).second) {
      const string& dsmname = dsm->getName();
      delete dsm;
      throw InternalProcessingException("dsm name"+
              dsmname+"is not unique: This should NOT happen!");
      //throw n_u::InvalidParameterException("dsm name",
      //        dsmname,"is not unique: This should NOT happen!");
    }
  }

  if (!dsm_ids.insert(iDsmId).second) {
    throw n_u::InvalidParameterException("dsm id", dsmId,
          "is not unique");
  }
  if (!dsm_names.insert(dsmName).second) {
    throw n_u::InvalidParameterException("dsm name",
          dsmName,"is not unique");
  }

  return iDsmId;
}

void Document::addSampAndVar(xercesc::DOMElement *sensorElem, xercesc::DOMNode *dsmNode)
{
  const XMLCh * sampTagName = 0;
  XMLStringConverter xmlSamp("sample");
  sampTagName = (const XMLCh *) xmlSamp;

  // Create a new DOM element for the sample node
  xercesc::DOMElement* sampElem = 0;
  try {
    sampElem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         sampTagName);
  } catch (DOMException &e) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm sample element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the sample node attributes
  sampElem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter("1"));
  sampElem->setAttribute((const XMLCh*)XMLStringConverter("rate"), (const XMLCh*)XMLStringConverter("1"));

  // The sample Element needs a Dummy a2d variable element
  xercesc::DOMElement* varElem = createA2DVarElement(dsmNode);

  // Now add the fully qualified sample to the sensor node
  sampElem->appendChild(varElem);
  sensorElem->appendChild(sampElem);

  return; 
}

xercesc::DOMElement* Document::createA2DVarElement(xercesc::DOMNode *seniorNode)
{
  // Make sure dummy variable name is unique
  dummyNum++;
  char dummyName[10];
  if (dummyNum > 99999) dummyNum = 0;  // highly unlikely but better to check
  sprintf(dummyName, "DUMMY%d",dummyNum);

  // The Sample node needs a variable node
  const XMLCh * varTagName = 0;
  XMLStringConverter xmlVar("variable");
  varTagName = (const XMLCh *) xmlVar;

  // Create a new DOM element for the variable node
  xercesc::DOMElement* varElem = 0;
  try {
    varElem = seniorNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         varTagName);
  } catch (DOMException &e) {
     cerr << "seniorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new variableelement:  " + 
                            (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the variable node attributes
  varElem->setAttribute((const XMLCh*)XMLStringConverter("longname"), 
                        (const XMLCh*)XMLStringConverter("Dummy variable"));
  varElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter(dummyName));
  varElem->setAttribute((const XMLCh*)XMLStringConverter("units"), 
                        (const XMLCh*)XMLStringConverter("V"));


  // The variable Element needs gain parameter node
  const XMLCh * parmTagName = 0;
  XMLStringConverter xmlParm("parameter");
  parmTagName = (const XMLCh *) xmlParm;

  // Create a new DOM element for the gain parameter node
  xercesc::DOMElement* gainParmElem = 0;
  try {
    gainParmElem = seniorNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "seniorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("Document::addSensor create new variable gain parameter element: " +
                            (std::string)XMLStringConverter(e.getMessage()));
  }

  // Set up the gain parameter attributes
  gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter("gain"));
  gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                        (const XMLCh*)XMLStringConverter("float"));
  gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                        (const XMLCh*)XMLStringConverter("4"));

  varElem->appendChild(gainParmElem);

  // Create a new DOM element for the bipolar parameter node
  xercesc::DOMElement* bipolarParmElem = 0;
  try {
    bipolarParmElem = seniorNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "seniorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("Document::addSensor create new variable bipolar parameter element: " +
                            (std::string)XMLStringConverter(e.getMessage()));
  }

  // Set up the bipolar parameter attributes
  bipolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("name"),
                        (const XMLCh*)XMLStringConverter("bipolar"));
  bipolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                        (const XMLCh*)XMLStringConverter("bool"));
  bipolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                        (const XMLCh*)XMLStringConverter("false"));

  varElem->appendChild(bipolarParmElem);

  return varElem;
}

xercesc::DOMElement* Document::createDsmOutputElem(xercesc::DOMNode *siteNode)
{
  const XMLCh * outTagName = 0;
  XMLStringConverter xmlOut("output");
  outTagName = (const XMLCh *) xmlOut;

  // Create a new DOM element for the output node
  xercesc::DOMElement* outElem = 0;
  try {
    outElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         outTagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm output element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the output node attributes
  outElem->setAttribute((const XMLCh*)XMLStringConverter("class"), (const XMLCh*)XMLStringConverter("RawSampleOutputStream"));

  // The Output node needs a socket node
  const XMLCh * sockTagName = 0;
  XMLStringConverter xmlSock("socket");
  sockTagName = (const XMLCh *) xmlSock;

  // Create a new DOM element for the socket node
  xercesc::DOMElement* sockElem = 0;
  try {
    sockElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         sockTagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new socket element:  " + (std::string)XMLStringConverter(e.getMessage()));
  }
  sockElem->setAttribute((const XMLCh*)XMLStringConverter("type"), (const XMLCh*)XMLStringConverter("mcrequest"));

  // Create the dsm->output->socket hierarchy in preparation for inserting it into the DOM tree
  outElem->appendChild(sockElem);

  return outElem;
}

xercesc::DOMElement* Document::createDsmServOutElem(xercesc::DOMNode *siteNode)
{
  const XMLCh * outTagName = 0;
  XMLStringConverter xmlOut("output");
  outTagName = (const XMLCh *) xmlOut;

  // Create a new DOM element for the output node
  xercesc::DOMElement* outElem = 0;
  try {
    outElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         outTagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm output element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the output node attributes
  outElem->setAttribute((const XMLCh*)XMLStringConverter("class"), (const XMLCh*)XMLStringConverter("RawSampleOutputStream"));

  // The Output node needs a socket node
  const XMLCh * sockTagName = 0;
  XMLStringConverter xmlSock("socket");
  sockTagName = (const XMLCh *) xmlSock;

  // Create a new DOM element for the socket node
  xercesc::DOMElement* sockElem = 0;
  try {
    sockElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         sockTagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new socket element:  " + (std::string)XMLStringConverter(e.getMessage()));
  }
  sockElem->setAttribute((const XMLCh*)XMLStringConverter("port"), (const XMLCh*)XMLStringConverter("3000"));
  sockElem->setAttribute((const XMLCh*)XMLStringConverter("type"), (const XMLCh*)XMLStringConverter("server"));

  // Create the dsm->output->socket hierarchy in preparation for inserting it into the DOM tree
  outElem->appendChild(sockElem);

  return outElem;
}

unsigned int Document::validateSampleInfo(DSMSensor *sensor, const std::string & sampleId)
{

  // Check that id is legit
  unsigned int iSampleId;
  if (sampleId.length() > 0) {
    istringstream ist(sampleId);
    ist >> iSampleId;
    if (ist.fail()) throw n_u::InvalidParameterException(
        string("sample Id") + ": " + sampleId);
  }

  // Make sure that the sampleId is unique for this Site
  set<int> sample_ids;
  int j;
  SampleTagIterator it;
  for (j=0, it = sensor->getSampleTagIterator(); it.hasNext(); j++) {
    SampleTag * sample = (SampleTag*)(it.next()); // XXX cast from const
    // The following clause would be really bizzarre, but lets check anyway
    // TODO: these should actually get the SampleItem and delete it.
    //   since this situation should never arise, lets not worry for now and just delete the sample.
    if (!sample_ids.insert(sample->getId()).second) {
      ostringstream ost;
      ost << sample->getId();
      delete sample;
      throw InternalProcessingException("Existing sample id"+ost.str()+
              "is not unique: This should NOT happen!");
    }
  }

  if (!sample_ids.insert(iSampleId).second) {
    throw n_u::InvalidParameterException("sample id", sampleId,
          "is not unique");
  }

  return iSampleId;
}

xercesc::DOMElement* Document::createSampleElement(xercesc::DOMNode *sensorNode, 
                         const std::string & sampleId, 
                         const std::string & sampleRate,
                         const std::string & sampleFilter)
{
  // XML tagname for Samples is "sample"
  const XMLCh * tagName = 0;
  XMLStringConverter xmlSample("sample");
  tagName = (const XMLCh *) xmlSample;

  // create a new DOM element for the Sample
  xercesc::DOMElement* sampleElem = 0;
  try {
     sampleElem = sensorNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         tagName);
  } catch (DOMException &e) {
     cerr << "sensorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("sample create new sample element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // setup the new Sample DOM element from user input
cerr << "setting samp element attribs: id = " << sampleId << "  rate = " << sampleRate << "\n";
  sampleElem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter(sampleId));
  sampleElem->setAttribute((const XMLCh*)XMLStringConverter("rate"), (const XMLCh*)XMLStringConverter(sampleRate));
  if (!sampleFilter.empty()) {
    // we will need to add two parameters as children to the sample
    ///the first is filter w/type (boxcar, ...? )
    //  and the second is numpoints (of type integer
    cerr << "We have a filter type, but are not actually adding a filter parameter yet\n";
  }
  return sampleElem;
}

void Document::addSample(const std::string & sampleId, const std::string & sampleRate,
                         const std::string & sampleFilter) 
{
cerr<<"entering Document::addSample about to make call to _configWindow->getModel()"  <<"\n";
  NidasModel *model = _configWindow->getModel();
cerr<<"got model \n";
  SensorItem * sensorItem = dynamic_cast<SensorItem*>(model->getCurrentRootItem());
  if (!sensorItem)
    throw InternalProcessingException("Current root index is not a Sensor.");

cerr << "got sensor item \n";

  DSMSensor *sensor = sensorItem->getDSMSensor();
  if (!sensor)
    throw InternalProcessingException("null DSMSensor");

cerr << "got sensor \n";

  // Make sure we have a new unique sample id and valid rate
  unsigned int iSampleId;
  iSampleId = validateSampleInfo(sensor, sampleId);
  float fRate;
  if (sampleRate.length() > 0) {
    istringstream ist(sampleId);
    ist >> fRate;
    if (ist.fail()) throw n_u::InvalidParameterException(
        string("sample") + ": " ," id",sampleId);
  }

cerr << "past check for valid sample info\n";


//***
// get the DOM node for this DSMSensor
  xercesc::DOMNode *sensorNode = sensorItem->getDOMNode();
  if (!sensorNode) {
    throw InternalProcessingException("null sensor DOM node");
  }
cerr << "past getDSMSensorNode()\n";


  // The Sample needs a Dummy variable as a placeholder
  const XMLCh * variableName = 0;
  XMLStringConverter xmlVariable("variable");
  variableName =  (const XMLCh *) xmlVariable;

  xercesc::DOMElement* sampleElem = createSampleElement(sensorNode,sampleId,sampleRate,sampleFilter);

  // Create and add a new DOM element for the variable node
  xercesc::DOMElement* variableElem = createA2DVarElement(sensorNode);
  sampleElem->appendChild(variableElem);
cerr<< "appended variable element to sampleElem \n";

// add sample to nidas project

    // Taken from nidas::core::SampleTag::fromDOMElement()

    SampleTag* sample = new SampleTag();
    sample->setSampleId(iSampleId);
    sample->setRate(fRate);
    sample->setSensorId(sensor->getSensorId());
    sample->setDSMId(sensor->getDSMId());
//    sample_sample_id_t nidasId;
//    nidasId = convert from string to sample_sample_id_t;k
//    sample->setId(nidasId);
cerr << "Calling fromDOM \n";
    try {
                sample->fromDOMElement((xercesc::DOMElement*)sampleElem);
    }
    catch(const n_u::InvalidParameterException& e) {
        delete sample;
        throw;
    }

    sensor->addSampleTag(sample);
 
cerr<<"added SampleTag to the Sensor\n";

    // add sample to DOM
  try {
    sensorNode->appendChild(sampleElem);
  } catch (DOMException &e) {
     sensor->removeSampleTag(sample);  // keep nidas Project tree in sync with DOM
     throw InternalProcessingException("add sample to dsm element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

cerr<<"added sample node to the DOM\n";

    // update Qt model
    // XXX returns bool
  model->appendChild(sensorItem);

//   printSiteNames();
}

void Document::addDSM(const std::string & dsmName, const std::string & dsmId,
                         const std::string & dsmLocation) 
//               throw (nidas::util::InvalidParameterException, InternalProcessingException)
{
cerr<<"entering Document::addDSM about to make call to _configWindow->getModel()"  <<"\n"
      "dsmName = "<<dsmName<<" id= "<<dsmId<<" location= " <<dsmLocation<<"\n"
      "configwindow address = "<< _configWindow <<"\n";
  NidasModel *model = _configWindow->getModel();
  SiteItem * siteItem = dynamic_cast<SiteItem*>(model->getCurrentRootItem());
  if (!siteItem)
    throw InternalProcessingException("Current root index is not a Site.");

  Site *site = siteItem->getSite();
  if (!site)
    throw InternalProcessingException("null Site");

  unsigned int iDsmId;
  iDsmId = validateDsmInfo(site, dsmName,dsmId);

// get the DOM node for this Site
  xercesc::DOMNode *siteNode = siteItem->getDOMNode();
  if (!siteNode) {
    throw InternalProcessingException("null site DOM node");
  }
  cerr << "past getSiteNode()\n";

// XML tagname for DSMs is "dsm"
  const XMLCh * tagName = 0;
  XMLStringConverter xmlDSM("dsm");
  tagName = (const XMLCh *) xmlDSM;

    // create a new DOM element for the DSM
  xercesc::DOMElement* dsmElem = 0;
  try {
     dsmElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         tagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // setup the new DSM DOM element from user input
  //  TODO: are the three "fixed" attributes ok?  e.g. derivedData only needed for certain sensors.
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter(dsmName));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                        (const XMLCh*)XMLStringConverter(dsmId));
  if (!dsmLocation.empty()) dsmElem->setAttribute((const XMLCh*)XMLStringConverter("location"), 
                                                  (const XMLCh*)XMLStringConverter(dsmLocation));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("rserialPort"), 
                        (const XMLCh*)XMLStringConverter("30002"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("statusAddr"), 
                        (const XMLCh*)XMLStringConverter("sock::30001"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("derivedData"), 
                        (const XMLCh*)XMLStringConverter("sock::31000"));

  // The DSM needs an IRIG card sensor type
  const XMLCh * sensorTagName = 0;
  XMLStringConverter xmlSensor("sensor");
  sensorTagName =  (const XMLCh *) xmlSensor;

  // Create a new DOM element for the sensor node
  xercesc::DOMElement* sensorElem = 0;
  try {
    sensorElem = siteNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         sensorTagName);
  } catch (DOMException &e) {
     cerr << "siteNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm sensor element: " + 
                                      (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the sensor node attributes
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), 
                           (const XMLCh*)XMLStringConverter("IRIG"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), 
                           (const XMLCh*)XMLStringConverter("/dev/irig0"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                           (const XMLCh*)XMLStringConverter("100"));

  string suffix = dsmName;
  size_t found = suffix.find("dsm");
  if (found != string::npos)
    suffix.replace(found,3,"");
  suffix.insert(0,"_");
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), 
                           (const XMLCh*)XMLStringConverter(suffix));

  dsmElem->appendChild(sensorElem);
cerr<< "appended sensor element to dsmElem \n";

  // The DSM node needs an output node w/mcrequest
  xercesc::DOMElement* outElem = createDsmOutputElem(siteNode);
  dsmElem->appendChild(outElem);

  // The DSM node needs an output node w/server port
  xercesc::DOMElement* servOutElem = createDsmServOutElem(siteNode);
  dsmElem->appendChild(servOutElem);

// add dsm to nidas project

    // adapted from nidas::core::Site::fromDOMElement()
    // should be factored out of that method into a public method of Site

    DSMConfig* dsm = new DSMConfig();
    dsm->setSite(site);
    dsm->setName(dsmName);
//    dsm_sample_id_t nidasId;
//    nidasId = convert from string to dsm_sample_id_t;k
//    dsm->setId(nidasId);
    dsm->setId(iDsmId);
    dsm->setLocation(dsmLocation);
    try {
                dsm->fromDOMElement((xercesc::DOMElement*)dsmElem);
    }
    catch(const n_u::InvalidParameterException& e) {
        delete dsm;
        throw;
    }

    site->addDSMConfig(dsm);
 
cerr<<"added DSMConfig to the site\n";

//    DSMSensor* sensor = dsm->sensorFromDOMElement(dsmElem);
//    if (sensor == NULL)
//      throw InternalProcessingException("null sensor(FromDOMElement)");


    // add dsm to DOM
  try {
    siteNode->appendChild(dsmElem);
  } catch (DOMException &e) {
     site->removeDSMConfig(dsm);  // keep nidas Project tree in sync with DOM
     throw InternalProcessingException("add dsm to site element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

cerr<<"added dsm node to the DOM\n";

    // update Qt model
    // XXX returns bool
  model->appendChild(siteItem);

//   printSiteNames();
}

unsigned int Document::getNextSensorId()
{
cerr<< "in getNextSensorId" << endl;
  unsigned int maxSensorId = 0;

  NidasModel *model = _configWindow->getModel();

  DSMItem * dsmItem = dynamic_cast<DSMItem*>(model->getCurrentRootItem());
  if (!dsmItem) {
    throw InternalProcessingException("Current root index is not a DSM.");
    cerr<<" dsmItem = NULL" << endl;
    return 0;
    }
cerr<< "after call to model->getCurrentRootItem" << endl;

  //DSMConfig *dsmConfig = (DSMConfig *) dsmItem;
  DSMConfig *dsmConfig = dsmItem->getDSMConfig();
  if (dsmConfig == NULL) {
    cerr << "dsmConfig is null!" <<endl;
    return 0;
    }

cerr << "dsmConfig name : " << dsmConfig->getName() << endl;
  const std::list<DSMSensor*>& sensors = dsmConfig->getSensors();
cerr<< "after call to dsmConfig->getSensors" << endl;

  for (list<DSMSensor*>::const_iterator si = sensors.begin();si != sensors.end(); si++) 
  {
if (*si == 0) cerr << "si is zero" << endl;
cerr<<" si is: " << (*si)->getName() << endl;
    if ((*si)->getSensorId() > maxSensorId) maxSensorId = (*si)->getSensorId();
cerr<< "after call to getSensorId" << endl;
  }

    maxSensorId += 200;
cerr<< "returning maxSensorId " << maxSensorId << endl;
    return maxSensorId;
}

std::list <int> Document::getAvailableA2DChannels()
{
cerr<< "in getAvailableA2DChannels" << endl;
  std::list<int> availableChannels;
  for (int i = 0; i < 8; i++) availableChannels.push_back(i);
  std::list<int>::iterator aci;

  NidasModel *model = _configWindow->getModel();

  /*
  SampleItem * sampleItem = dynamic_cast<SampleItem*>(model->getCurrentRootItem());
  if (!sampleItem) {
    throw InternalProcessingException("Current root index is not a Sample.");
    cerr<<" sampleItem = NULL" << endl;
    return availableChannels;
  }
  */

  SensorItem * sensorItem = dynamic_cast<SensorItem*>(model->getCurrentRootItem());
  if (!sensorItem) {
    throw InternalProcessingException("Parent of SampleItem is not a SensorItem!");
    cerr<<" sensorItem = NULL\n";
    return availableChannels;
  }

  DSMSensor *sensor = sensorItem->getDSMSensor();
  if (sensor == NULL) {
    cerr << "dsmSensor is null!\n";
    return availableChannels;
  }

  for (SampleTagIterator sti = sensor->getSampleTagIterator(); sti.hasNext(); ) {
    const SampleTag* tag = sti.next();
    for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); )
    {
      const Variable* var = vi.next();
      int curChan = var->getA2dChannel();
      for (aci = availableChannels.begin(); aci!= availableChannels.end(); ++aci)
        if (*aci == curChan) {aci=availableChannels.erase(aci);break;}
    }
  }

  cerr << "Available channels are: ";
  for (aci = availableChannels.begin(); aci!= availableChannels.end(); ++aci)
    cerr<< " " << *aci;
  cerr << "\n";

    return availableChannels;
}

unsigned int Document::getNextDSMId()
{
cerr<< "in getNextDSMId" << endl;
  unsigned int maxDSMId = 0;

  NidasModel *model = _configWindow->getModel();

  SiteItem * siteItem = dynamic_cast<SiteItem*>(model->getCurrentRootItem());
  if (!siteItem) {
    throw InternalProcessingException("Current root index is not a Site.");
    cerr<<" siteItem = NULL" << endl;
    return 0;
    }
cerr<< "after call to model->getCurrentRootItem" << endl;

  //DSMConfig *dsmConfig = (DSMConfig *) dsmItem;
  Site *site = siteItem->getSite();
  if (site == NULL) {
    cerr << "Site is null!" <<endl;
    return 0;
    }

cerr << "Site name : " << site->getName() << endl;
  const std::list<const DSMConfig*>& dsmConfigs = site->getDSMConfigs();
cerr<< "after call to site->getDSMConfigs" << endl;

  for (list<const DSMConfig*>::const_iterator di = dsmConfigs.begin();di != dsmConfigs.end(); di++) 
  {
if (*di == 0) cerr << "di is zero" << endl;
cerr<<" di is: " << (*di)->getName() << endl;
    if ((*di)->getId() > maxDSMId) maxDSMId = (*di)->getId();
cerr<< "after call to getDSMId" << endl;
  }

    maxDSMId += 1;
cerr<< "returning maxDSMId" << maxDSMId << endl;
    return maxDSMId;
}

void Document::addA2DVariable(const std::string & a2dVarName, const std::string & a2dVarLongName,
                         const std::string & a2dVarVolts, const std::string & a2dVarChannel,
                         const std::string & a2dVarSR, const std::string & a2dVarUnits, 
                         vector <std::string> cals)
{
cerr<<"entering Document::addA2DVariable about to make call to _configWindow->getModel()"  <<"\n";
  NidasModel *model = _configWindow->getModel();
cerr<<"got model \n";
  SensorItem * sensorItem = dynamic_cast<SensorItem*>(model->getCurrentRootItem());
  //SampleItem * sampleItem = dynamic_cast<SampleItem*>(model->getCurrentRootItem());
  if (!sensorItem)
    throw InternalProcessingException("Current root index is not a Sensor.");

cerr << "got sensor item \n";

// Find or create the SampleTag that will house this variable
  SampleTag *sampleTag2Add2=0; // = sampleItem->getSampleTag();
  unsigned int iSampRate;
  if (a2dVarSR.length() > 0) {
    istringstream ist(a2dVarSR);
    ist >> iSampRate;
    if (ist.fail()) throw n_u::InvalidParameterException(
        string("sample rate:") + a2dVarSR);
  }
  set<unsigned int> sampleIds;

// We want a sampleTag with the same sample rate as requested, but if the 
// SampleTag found is A2D temperature, we don't want it.
  for (int i=0; i< sensorItem->childCount(); i++) {
    VariableItem* variableItem = dynamic_cast<VariableItem*>(sensorItem->child(i));
    if (!variableItem)
      throw InternalProcessingException("Found child of SensorItem that's not a VariableItem!");
    SampleTag* sampleTag = variableItem->getSampleTag();
    sampleIds.insert(sampleTag->getSampleId());
    if (sampleTag->getRate() == iSampRate) 
      if  ( !sampleTag->getParameter("temperature")) sampleTag2Add2 = sampleTag;
  }

set<unsigned int>::iterator it;
cerr << "Sample IDs found: ";
for (it=sampleIds.begin(); it!=sampleIds.end(); it++)
    cerr << " " << *it;
cerr << "\n";

  bool createdNewSamp = false;
  xercesc::DOMElement* newSampleElem = 0;
  xercesc::DOMNode *sampleNode = 0;
  if (!sampleTag2Add2) {
    // We need a unique sample Id
    unsigned int sampleId;
    for (unsigned int i = 1; i<99; i++) {
      pair<set<unsigned int>::iterator,bool> ret;
      ret = sampleIds.insert(i);
      if (ret.second == true) {
        sampleId=i;
        break;
      }
    }
    char sSampleId[10];
    sprintf(sSampleId,"%d",sampleId);
    xercesc::DOMElement* newSampleElem = 0;
    xercesc::DOMNode * sensorDOMNode = sensorItem->getDOMNode();

    newSampleElem = createSampleElement(sensorDOMNode,
                                                 string(sSampleId),a2dVarSR,string(""));

cerr << "prior to fromdom newSampleElem = " << newSampleElem << "\n";
    createdNewSamp = true;

    try {
      sampleTag2Add2 = new SampleTag();
      //sampleTag2Add2->setSampleId(sampleId);
      //sampleTag2Add2->setRate(iSampRate);
      DSMSensor* sensor = sensorItem->getDSMSensor();
      sampleTag2Add2->setSensorId(sensor->getSensorId());
      sampleTag2Add2->setDSMId(sensor->getDSMId());
      sampleTag2Add2->fromDOMElement((xercesc::DOMElement*)newSampleElem);
      sensor->addSampleTag(sampleTag2Add2);
 
cerr << "after fromdom newSampleElem = " << newSampleElem << "\n";
cerr<<"added SampleTag to the Sensor\n";

      // add sample to DOM
      DOMNode * sensorNode = sensorItem->getDOMNode();
      try {
        sampleNode = sensorNode->appendChild(newSampleElem);
      } catch (DOMException &e) {
        sensor->removeSampleTag(sampleTag2Add2);  // keep nidas Project tree in sync with DOM
        throw InternalProcessingException("add sample to dsm element: " + 
                                      (std::string)XMLStringConverter(e.getMessage()));
      }
    }
    catch(const n_u::InvalidParameterException& e) {
        delete sampleTag2Add2;
        throw;
    }

  }
cerr << "got sampleTag\n";

//  Getting the sampleNode - if we created a newSampleElem above then we just need to cast it 
//  as a DOMNode, but if not, we need to step through sample nodes of this sensor until we find the 
//  one with the right ID
  if (!createdNewSamp)  {
//cerr << "Assigning new Sample Element = " << newSampleElem << "  to SampleNode\n";
 //   sampleNode = ((xercesc::DOMNode *) newSampleElem);
//cerr << " After assignment SampleNode = " << sampleNode << "\n";
  //}
  //else  
    sampleNode = sensorItem->findSampleDOMNode(sampleTag2Add2);
  }

  if (!sampleNode) {
    throw InternalProcessingException("null sample DOM node");
  }
cerr << "past getSampleNode()\n";

// Now add the new variable to the sample get the DOM node for this SampleTag
// XML tagname for A2DVariables is "variable"
  const XMLCh * tagName = 0;
  XMLStringConverter xmlA2DVariable("variable");
  tagName = (const XMLCh *) xmlA2DVariable;

    // create a new DOM element for the A2DVariable
  xercesc::DOMElement* a2dVarElem = 0;
  try {
     a2dVarElem = sampleNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         tagName);
  } catch (DOMException &e) {
     cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("a2dVar create new a2dVar element: " + 
                              (std::string)XMLStringConverter(e.getMessage()));
  }

  // setup the new A2DVariable DOM element from user input
cerr << "setting variable element attribs: name = " << a2dVarName << "\n";
  a2dVarElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                           (const XMLCh*)XMLStringConverter(a2dVarName));
  a2dVarElem->setAttribute((const XMLCh*)XMLStringConverter("longname"), 
                           (const XMLCh*)XMLStringConverter(a2dVarLongName));
  a2dVarElem->setAttribute((const XMLCh*)XMLStringConverter("units"), 
                           (const XMLCh*)XMLStringConverter("V"));

  // Now we need parameters for channel, gain and bipolar
  const XMLCh * parmTagName = 0;
  XMLStringConverter xmlParm("parameter");
  parmTagName = (const XMLCh *) xmlParm;

  // create a new DOM element for the Channel parameter
  xercesc::DOMElement* chanParmElem = 0;
  try {
    chanParmElem  = sampleNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("a2dVar create new channel element: " +
                             (std::string)XMLStringConverter(e.getMessage()));
  }
  chanParmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                           (const XMLCh*)XMLStringConverter("channel"));
  chanParmElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                           (const XMLCh*)XMLStringConverter("int"));
  chanParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"), 
                           (const XMLCh*)XMLStringConverter(a2dVarChannel));

  // create new DOM elements for the gain and bipolar parameters
  xercesc::DOMElement* gainParmElem = 0;
  try {
    gainParmElem = sampleNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("a2dVar create new gain element: " +
                             (std::string)XMLStringConverter(e.getMessage()));
  }
  gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("name"),
                           (const XMLCh*)XMLStringConverter("gain"));
  gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                           (const XMLCh*)XMLStringConverter("float"));

  xercesc::DOMElement* biPolarParmElem = 0;
  try {
    biPolarParmElem = sampleNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("a2dVar create new biPolar element: " +
                             (std::string)XMLStringConverter(e.getMessage()));
  }
  biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("name"),
                           (const XMLCh*)XMLStringConverter("bipolar"));
  biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                           (const XMLCh*)XMLStringConverter("bool"));

  // Now set gain and BiPolar according to the user's selection
  if (a2dVarVolts =="  0 to  5 Volts") {
    gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("4"));
    biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("false"));
  } else
  if (a2dVarVolts == " -5 to  5 Volts") {
      gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("2"));
      biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("true"));
  } else
  if (a2dVarVolts == "  0 to 10 Volts") {
    gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("2"));
    biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("false"));
  } else
  if (a2dVarVolts == "-10 to 10 Volts") {
    gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("1"));
    biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("true"));
  } else 
    throw InternalProcessingException("Voltage choice not found in Document if/else block!");

  a2dVarElem->appendChild(chanParmElem);
  a2dVarElem->appendChild(gainParmElem);
  a2dVarElem->appendChild(biPolarParmElem);

  // Insert Calibration info if the user provided it.
  if (cals[0].size()) {  // we have some cal info

    if (cals[2].size()) {  // poly cal
      // We need a poly node
      const XMLCh * polyTagName = 0;
      XMLStringConverter xmlPoly("poly");
      polyTagName = (const XMLCh *) xmlPoly;
    
      // create a new DOM element for the poly node
      xercesc::DOMElement* polyElem = 0;
      try {
        polyElem  = sampleNode->getOwnerDocument()->createElementNS(
             DOMable::getNamespaceURI(),
             polyTagName);
      } catch (DOMException &e) {
         cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
         throw InternalProcessingException("a2dVar create new poly calibration element: " +
                                 (std::string)XMLStringConverter(e.getMessage()));
      }

      // set up the poly node attributes
      std::string polyStr = cals[0];
      for (int i = 1; i < cals.size(); i++)
        if (cals[i].size()) polyStr += (" " + cals[i]);

      polyElem->setAttribute((const XMLCh*)XMLStringConverter("units"), 
                               (const XMLCh*)XMLStringConverter(a2dVarUnits));
      polyElem->setAttribute((const XMLCh*)XMLStringConverter("coefs"),
                               (const XMLCh*)XMLStringConverter(polyStr));

      a2dVarElem->appendChild(polyElem);

    } else {   // slope & offset cal
      // We need a linear node
      const XMLCh * linearTagName = 0;
      XMLStringConverter xmlLinear("linear");
      linearTagName = (const XMLCh *) xmlLinear;

      // create a new DOM element for the linear node
      xercesc::DOMElement* linearElem = 0;
      try {
        linearElem = sampleNode->getOwnerDocument()->createElementNS(
             DOMable::getNamespaceURI(),
             linearTagName);
      } catch (DOMException &e) {
         cerr << "sampleNode->getOwnerDocument()->createElementNS() threw exception\n";
         throw InternalProcessingException("a2dVar create new linear calibration element: " +
                                 (std::string)XMLStringConverter(e.getMessage()));
      }

      // set up the linear node attributes
      linearElem->setAttribute((const XMLCh*)XMLStringConverter("units"),
                               (const XMLCh*)XMLStringConverter(a2dVarUnits));
      linearElem->setAttribute((const XMLCh*)XMLStringConverter("intercept"),
                               (const XMLCh*)XMLStringConverter(cals[0]));
      linearElem->setAttribute((const XMLCh*)XMLStringConverter("slope"),
                               (const XMLCh*)XMLStringConverter(cals[1]));
 
      a2dVarElem->appendChild(linearElem); 
    }
  }
  
// add a2dVar to nidas project

    // Taken from nidas::core::Variable::fromDOMElement()

    Variable* a2dVar = new Variable();
    //a2dVar->setSampleTag(sample);
    //a2dVar->setA2DVariableId(iA2DVariableId);
    //a2dVar->setRate(fRate);
    //a2dVar->setSensorId(sensor->getSensorId());
    //a2dVar->setDSMId(sensor->getDSMId());
//    a2dVar_a2dVar_id_t nidasId;
//    nidasId = convert from string to a2dVar_a2dVar_id_t;k
//    a2dVar->setId(nidasId);
    a2dVar->setA2dChannel(atoi(a2dVarChannel.c_str()));
cerr << "Calling fromDOM \n";
    try {
                a2dVar->fromDOMElement((xercesc::DOMElement*)a2dVarElem);
    }
    catch(const n_u::InvalidParameterException& e) {
        delete a2dVar;
        throw;
    }

  // Make sure we have a new unique a2dVar name 
  // Note - this will require getting the site and doing a validate variables.
  try {
    sampleTag2Add2->addVariable(a2dVar);
    sampleTag2Add2->getSite()->validateVariables();

  } catch (nidas::util::InvalidParameterException &e) {
    sampleTag2Add2->removeVariable(a2dVar); // validation failed so get it out of nidas Project tree
    delete a2dVar;
    throw(e); // notify GUI
  }

cerr << "past check for valid a2dVar info\n";

    // add a2dVar to DOM
  try {
    sampleNode->appendChild(a2dVarElem);
  } catch (DOMException &e) {
     sampleTag2Add2->removeVariable(a2dVar);  // keep nidas Project tree in sync with DOM
     delete a2dVar;
     throw InternalProcessingException("add a2dVar to dsm element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

cerr<<"added a2dVar node to the DOM\n";

    // update Qt model
    // XXX returns bool
  model->appendChild(sensorItem);

//   printSiteNames();
}
