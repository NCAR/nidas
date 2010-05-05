
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
        Project *project = Project::getInstance(); // start anew

        cerr << "doing fromDOMElement" << endl;
            // build Project tree
        project->fromDOMElement(domdoc->getDocumentElement());
        cerr << "fromDOMElement done" << endl;
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



void Document::deleteSensor(QModelIndexList selectedIndexList)
{
  NidasModel *model = _configWindow->getModel();
  DSMItem * dsmItem = dynamic_cast<DSMItem*>(model->getCurrentRootItem());
  if (!dsmItem)
    throw InternalProcessingException("Current root index is not a DSM.");
  DSMConfig *dsmConfig = dsmItem->getDSMConfig();
  if (!dsmConfig)
    throw InternalProcessingException("null DSMConfig");

// get the DOM node for this DSM
  xercesc::DOMNode *dsmNode = dsmItem->getDOMNode();
  if (!dsmNode) {
    throw InternalProcessingException("null dsm DOM node");
  }
  cerr << "past getDSMNode()\n";

  std::list <std::string> selectedDevices;
  std::list<int> selectedRows;
  getSelectedSensorDevices(selectedIndexList,selectedDevices,selectedRows);
  if (selectedDevices.size() == 0) return;
  cerr << "selectedDevices.size() == " << selectedDevices.size() << "\n";

    // delete sensor from DOM tree : move into NidasModel?
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


    // delete sensor from nidas model : move into NidasModel?
  std::list <std::string>::iterator it;
  for (it=selectedDevices.begin(); it!=selectedDevices.end(); it++)
  {
    for (SensorIterator si = dsmConfig->getSensorIterator(); si.hasNext(); ) {
      DSMSensor* sensor = si.next();
      if (sensor->getDeviceName() == *it)  { dsmConfig->removeSensor(sensor); break; }
    }
  }

    // update Qt model
    // XXX returns bool
model->removeChildren(selectedRows,dsmItem);
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

  // The Sample needs a Dummy variable as a placeholder
  const XMLCh * variableName = 0;
  XMLStringConverter xmlVariable("variable");
  variableName =  (const XMLCh *) xmlVariable;

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
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), (const XMLCh*)XMLStringConverter(dsmName));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter(dsmId));
  if (!dsmLocation.empty()) dsmElem->setAttribute((const XMLCh*)XMLStringConverter("location"), (const XMLCh*)XMLStringConverter(dsmLocation));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("rserialPort"), (const XMLCh*)XMLStringConverter("30002"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("statusAddr"), (const XMLCh*)XMLStringConverter("sock::30001"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("derivedData"), (const XMLCh*)XMLStringConverter("sock::31000"));

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
     throw InternalProcessingException("dsm create new dsm sensor element: " + (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the sensor node attributes
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), (const XMLCh*)XMLStringConverter("IRIG"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), (const XMLCh*)XMLStringConverter("/dev/irig0"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("id"), (const XMLCh*)XMLStringConverter("100"));

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

void Document::getSelectedSensorDevices(QModelIndexList & il, std::list <std::string> & devList, std::list<int> & rows)
{
 QModelIndexList::const_iterator mi;
 for (mi = il.begin(); mi!= il.end(); mi++)
 {
  if ((*mi).column() != 0) continue; // actually column 1 (devicename) but the whole row is selected
  NidasItem *item = static_cast<NidasItem*>((*mi).internalPointer());
  devList.push_back(item->dataField(1).toStdString());
  rows.push_back((*mi).row());
  cerr << "getSelectedSensorDevices found " << item->dataField(1).toStdString() << " at " << (*mi).row() << "," << (*mi).column() << "\n";
 }
}
