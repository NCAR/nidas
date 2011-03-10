
#include "Document.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"

#include <sys/param.h>
#include <libgen.h>

#include <xercesc/util/XMLUniDefs.hpp>

#if XERCES_VERSION_MAJOR < 3
#include <xercesc/dom/DOMWriter.hpp>
#else
#include <xercesc/dom/DOMLSSerializer.hpp>
#endif

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



bool Document::writeDocument()
{
    LocalFileFormatTarget *target;

    try {
cerr<<"filename = "<<filename->c_str()<<"\n";
        target = new LocalFileFormatTarget(filename->c_str());
        if (!target) {
            cerr << "target is null" << endl;
            return false;
            }
    } catch (...) {
        cerr << "LocalFileFormatTarget new exception" << endl;
        return false;
    }

    writeDOM(target,domdoc);
    delete target;
    return true;
}



bool Document::writeDOM( XMLFormatTarget * const target, const DOMNode * node )
{
    DOMImplementation *domimpl;
    DOMImplementationLS *lsimpl;
#if XERCES_VERSION_MAJOR < 3
    DOMWriter *myWriter;
#else
    DOMLSSerializer *myWriter;
#endif


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

        _project = new Project(); // start anew

            // build Project tree
        _project->fromDOMElement(domdoc->getDocumentElement());
}

string Document::getProjectName() const
{
    return (_project->getName());
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
  _project->setName(projectName);

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

    if (sensorIdName.empty()) return(NULL);
    if(!_project->getSensorCatalog()) {
        cerr<<"Configuration file doesn't contain a catalog!!"<<endl;
        return(0);
    }
    map<string,xercesc::DOMElement*>::const_iterator mi;
    for (mi = _project->getSensorCatalog()->begin();
         mi != _project->getSensorCatalog()->end(); mi++) {
        if (mi->first == sensorIdName) {
           return mi->second;
        }
    }
    return(NULL);
}

void Document::updateSensor(const std::string & sensorIdName, 
                            const std::string & device, 
                            const std::string & lcId,
                            const std::string & sfx, 
                            const std::string & a2dTempSfx,
                            const std::string & a2dSNFname,
                            QModelIndexList indexList)
{
cerr<<"entering Document::updateSensor\n"; 

  // Gather together all the elements we'll need to update the Sensor
  // in both the DOM model and the Nidas Model
  NidasModel *model = _configWindow->getModel();
  DSMItem* dsmItem = dynamic_cast<DSMItem*>(model->getCurrentRootItem());
  if (!dsmItem)
    throw InternalProcessingException("Current root index is not a DSM.");

  DSMConfig *dsmConfig = dsmItem->getDSMConfig();
  if (!dsmConfig)
    throw InternalProcessingException("null DSMConfig");

  NidasItem *item;
  if (indexList.size() > 0)  {
    for (int i=0; i<indexList.size(); i++) {
      QModelIndex index = indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue; 
      item = model->getItem(index);
    }
  }

  if (!item) throw InternalProcessingException("null DSMConfig");
  SensorItem* sItem = dynamic_cast<SensorItem*>(item);
  if (!sItem) throw InternalProcessingException("Sensor Item not selected");

  // Get the Sensor, save all the current values and then
  //  update to the new values
  DSMSensor* sensor = sItem->getDSMSensor();
  std::string currDevName = sensor->getDeviceName();
  unsigned int currSensorId = sensor->getSensorId();
  std::string currSuffix = sensor->getSuffix();
  QString currA2DTempSfx;
  std::string currA2DCalFname;
  A2DSensorItem* a2dSensorItem;
  if (sensorIdName == "Analog") {
    a2dSensorItem = dynamic_cast<A2DSensorItem*>(sItem);
    currA2DTempSfx = a2dSensorItem->getA2DTempSuffix();
    currA2DCalFname = sensor->getCalFile()->getFile();
cerr<< "Current calfile name: " << currA2DCalFname <<"\n";
  }

  // Start by updating the sensor DOM 
  updateSensorDOM(sItem, device, lcId, sfx);

  // If we've got an analog sensor then we need to update it's temp suffix
  // and its calibration file name
  if (sensorIdName ==  "Analog") {
    a2dSensorItem->updateDOMA2DTempSfx(currA2DTempSfx, a2dTempSfx);
cerr<< "calling updateDOMCalFile("<<a2dSNFname<<")\n";
    a2dSensorItem->updateDOMCalFile(a2dSNFname);
  }
  
  // Now we need to validate that all is right with the updated sensor
  // information - and if not change it all back to the original state
  try {
    sItem->fromDOM();

    // make sure new sensor works well with old (e.g. var names and suffix)
    //Site* site = const_cast <Site *> (dsmConfig->getSite());
    //site->validate();

  } catch (nidas::util::InvalidParameterException &e) {

    stringstream strS;
    strS<<currSensorId;
    updateSensorDOM(sItem, currDevName, strS.str(), currSuffix);
    if (sensorIdName ==  "Analog") {
      a2dSensorItem->updateDOMA2DTempSfx(QString::fromStdString(a2dTempSfx), 
                                         currA2DTempSfx.toStdString());
      a2dSensorItem->updateDOMCalFile(currA2DCalFname);
    }
    sItem->fromDOM();

    throw(e); // notify GUI
  } catch (InternalProcessingException) {
    stringstream strS;
    strS<<currSensorId;
    this->updateSensorDOM(sItem, currDevName, strS.str(), currSuffix);
    if (sensorIdName ==  "Analog") {
      a2dSensorItem->updateDOMA2DTempSfx(QString::fromStdString(a2dTempSfx), 
                                         currA2DTempSfx.toStdString());
      a2dSensorItem->updateDOMCalFile(currA2DCalFname);
    }
    sItem->fromDOM();
    throw; // notify GUI
  }

// Looks like the new values all pass the mustard...
std::cerr << "Finished updating sensor values - all seems ok\n";
   printSiteNames();
}

void Document::updateSensorDOM(SensorItem * sItem, const std::string & device,
                               const std::string & lcId, const std::string & sfx)
{
  // get the DOM node for this Sensor
  xercesc::DOMNode *sensorNode = sItem->getDOMNode();
  if (!sensorNode) {
    throw InternalProcessingException("null sensor DOM node");
  }

  // get the DOM element for this Sensor
  if (sensorNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("Sensor DOM Node is not an Element Node!");
  xercesc::DOMElement* sensorElem = ((xercesc::DOMElement*) sensorNode);

  // setup the DOM element from user input
  sensorElem->removeAttribute((const XMLCh*)XMLStringConverter("devicename"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), 
                           (const XMLCh*)XMLStringConverter(device));
  sensorElem->removeAttribute((const XMLCh*)XMLStringConverter("id"));
  sensorElem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                     (const XMLCh*)XMLStringConverter(lcId));
  if (!sfx.empty()) {
    sensorElem->removeAttribute((const XMLCh*)XMLStringConverter("suffix"));
    sensorElem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), 
                             (const XMLCh*)XMLStringConverter(sfx));
  }
}


void Document::addSensor(const std::string & sensorIdName, 
                         const std::string & device,
                         const std::string & lcId, 
                         const std::string & sfx, 
                         const std::string & a2dTempSfx, 
                         const std::string & a2dSNFname)
{
cerr << "entering Document::addSensor about to make call to "
     << "_configWindow->getModel()\n  configwindow address = " 
     << _configWindow << "\n";
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
    elem->setAttribute((const XMLCh*)XMLStringConverter("class"), 
                       (const XMLCh*)XMLStringConverter("raf.DSMAnalogSensor"));
  } else {
    elem->setAttribute((const XMLCh*)XMLStringConverter("IDREF"), 
                       (const XMLCh*)XMLStringConverter(sensorIdName));
  }
  elem->setAttribute((const XMLCh*)XMLStringConverter("devicename"), 
                     (const XMLCh*)XMLStringConverter(device));
  elem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                     (const XMLCh*)XMLStringConverter(lcId));
  if (!sfx.empty()) 
    elem->setAttribute((const XMLCh*)XMLStringConverter("suffix"), 
                       (const XMLCh*)XMLStringConverter(sfx));

  // If we've got an analog sensor then we need to set up a calibration file,
  // a rate, a sample and variable for it
  if (sensorIdName ==  "Analog") {
    addCalFile(elem, dsmNode, a2dSNFname);
    addA2DRate(elem, dsmNode, a2dSNFname);
    addSampAndVar(elem, dsmNode, a2dTempSfx);
  }

// add sensor to nidas project

    // adapted from nidas::core::DSMConfig::fromDOMElement()
    // should be factored out of that method into a public method of DSMConfig

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
        dsmConfig->validate();
        sensor->validate();

        // make sure new sensor works well with old (e.g. var names and suffix)
        Site* site = const_cast <Site *> (dsmConfig->getSite());
        site->validate();

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

    for (SiteIterator si = _project->getSiteIterator(); si.hasNext(); ) {
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
    DSMConfig * dsm = const_cast<DSMConfig*>(it.next()); // XXX cast from const
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

void Document::addA2DRate(xercesc::DOMElement *sensorElem,
                          xercesc::DOMNode *dsmNode,
                          const std::string & a2dSNFname)
{
  const XMLCh * paramTagName = 0;
  XMLStringConverter xmlSamp("parameter");
  paramTagName = (const XMLCh *) xmlSamp;

  // Create a new DOM element for the param element.
  xercesc::DOMElement* paramElem = 0;
  try {
    paramElem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         paramTagName);
  } catch (DOMException &e) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm sample element: " + 
                              (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the rate parameter node attributes
  paramElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                            (const XMLCh*)XMLStringConverter("rate"));
  paramElem->setAttribute((const XMLCh*)XMLStringConverter("value"), 
                            (const XMLCh*)XMLStringConverter("500"));
  paramElem->setAttribute((const XMLCh*)XMLStringConverter("type"),
                            (const XMLCh*)XMLStringConverter("int"));

  sensorElem->appendChild(paramElem);

  return;
}

void Document::addCalFile(xercesc::DOMElement *sensorElem,
                          xercesc::DOMNode *dsmNode,
                          const std::string & a2dSNFname)
{
  const XMLCh * calfileTagName = 0;
  XMLStringConverter xmlSamp("calfile");
  calfileTagName = (const XMLCh *) xmlSamp;

  // Create a new DOM element for the calfile element.
  xercesc::DOMElement* calfileElem = 0;
  try {
    calfileElem = dsmNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         calfileTagName);
  } catch (DOMException &e) {
     cerr << "dsmNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new dsm sample element: " 
                             + (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the calfile node attributes
  calfileElem->setAttribute((const XMLCh*)XMLStringConverter("path"), 
                            (const XMLCh*)XMLStringConverter
                                    ("$PROJ_DIR/Configuration/raf/cal_files/A2D"));
  calfileElem->setAttribute((const XMLCh*)XMLStringConverter("file"), 
                            (const XMLCh*)XMLStringConverter(a2dSNFname));

  sensorElem->appendChild(calfileElem);

  return;
}

void Document::addSampAndVar(xercesc::DOMElement *sensorElem, 
                             xercesc::DOMNode *dsmNode, 
                             const std::string & a2dTempSfx)
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
     throw InternalProcessingException("dsm create new dsm sample element: " + 
                              (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the sample node attributes
  sampElem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                         (const XMLCh*)XMLStringConverter("1"));
  sampElem->setAttribute((const XMLCh*)XMLStringConverter("rate"), 
                         (const XMLCh*)XMLStringConverter("1"));

  // The sample Element needs an A2D Temperature parameter and variable element
  xercesc::DOMElement* a2dTempParmElem = createA2DTempParmElement(dsmNode);
  xercesc::DOMElement* a2dTempVarElem = createA2DTempVarElement(dsmNode, 
                                                                a2dTempSfx);

  // Now add the fully qualified sample to the sensor node
  sampElem->appendChild(a2dTempParmElem);
  sampElem->appendChild(a2dTempVarElem);
  sensorElem->appendChild(sampElem);

  return; 
}

void Document::updateSampAndVar(xercesc::DOMElement *sensorElem, 
                                xercesc::DOMNode *dsmNode, 
                                const std::string & a2dTempSfx)
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

  // The sample Element needs an A2D Temperature parameter and variable element
  xercesc::DOMElement* a2dTempParmElem = createA2DTempParmElement(dsmNode);
  xercesc::DOMElement* a2dTempVarElem = createA2DTempVarElement(dsmNode, a2dTempSfx);

  // Now add the fully qualified sample to the sensor node
  sampElem->appendChild(a2dTempParmElem);
  sampElem->appendChild(a2dTempVarElem);
  sensorElem->appendChild(sampElem);

  return; 
}

xercesc::DOMElement* Document::createA2DTempParmElement(xercesc::DOMNode *seniorNode)
{
  // tag for parameter is "parameter"
  const XMLCh * parmTagName = 0;
  XMLStringConverter xmlParm("parameter");
  parmTagName = (const XMLCh *) xmlParm;

  // Create a new DOM element for the variable node
  xercesc::DOMElement* parmElem = 0;
  try {
    parmElem = seniorNode->getOwnerDocument()->createElementNS(
         DOMable::getNamespaceURI(),
         parmTagName);
  } catch (DOMException &e) {
     cerr << "Document::createA2DTempParmElement: seniorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new variableelement:  " + 
                            (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the variable node attributes
  parmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter("temperature"));
  parmElem->setAttribute((const XMLCh*)XMLStringConverter("value"), 
                        (const XMLCh*)XMLStringConverter("true"));
  parmElem->setAttribute((const XMLCh*)XMLStringConverter("type"), 
                        (const XMLCh*)XMLStringConverter("bool"));

  return parmElem;
}

xercesc::DOMElement* Document::createA2DTempVarElement(xercesc::DOMNode *seniorNode, const std::string & a2dTempSfx)
{
  // tag for variable is "variable"
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
     cerr << "Document::createA2DTempVarElement: seniorNode->getOwnerDocument()->createElementNS() threw exception\n";
     throw InternalProcessingException("dsm create new variableelement:  " + 
                            (std::string)XMLStringConverter(e.getMessage()));
  }

  // set up the variable node attributes
  varElem->setAttribute((const XMLCh*)XMLStringConverter("longname"), 
                        (const XMLCh*)XMLStringConverter("A2DTemperature"));
  varElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter("A2DTEMP_" + a2dTempSfx));
  varElem->setAttribute((const XMLCh*)XMLStringConverter("units"), 
                        (const XMLCh*)XMLStringConverter("deg_C"));

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
    SampleTag * sample = const_cast<SampleTag*>(it.next()); // XXX cast from const
    // The following clause would be really bizzarre, but lets check anyway
    // TODO: these should actually get the VariableItems with duplicate sample id and delete them.
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
cerr << "  setting samp element attribs: id = " << sampleId 
     << "  rate = " << sampleRate << "\n";
  sampleElem->setAttribute((const XMLCh*)XMLStringConverter("id"), 
                           (const XMLCh*)XMLStringConverter(sampleId));
  sampleElem->setAttribute((const XMLCh*)XMLStringConverter("rate"), 
                           (const XMLCh*)XMLStringConverter(sampleRate));

  // create parameter elements for boxcar filtering
  XMLStringConverter xmlParameter("parameter");
  tagName = (const XMLCh *) xmlParameter;
  xercesc::DOMElement* paramElems[] = {0,0};
  for (int i=0; i<2; i++) {
    try {
      paramElems[i] = sensorNode->getOwnerDocument()->createElementNS(
           DOMable::getNamespaceURI(), tagName);
    } catch (DOMException &e) {
      cerr << "createElementNS() threw exception creating boxcar param\n";
      throw InternalProcessingException("sample create new parameter elemtn: " +
                (std::string)XMLStringConverter(e.getMessage()));
    }
  }

  int sRate = atoi(sampleRate.c_str());
  int nPts = 500/sRate;
  stringstream strS;
  strS<<nPts;

  if (nPts != 1) {
    paramElems[0]->setAttribute((const XMLCh*)XMLStringConverter("name"),
                                (const XMLCh*)XMLStringConverter("filter"));
    paramElems[0]->setAttribute((const XMLCh*)XMLStringConverter("value"),
                                (const XMLCh*)XMLStringConverter("boxcar"));
    paramElems[0]->setAttribute((const XMLCh*)XMLStringConverter("type"),
                                (const XMLCh*)XMLStringConverter("string"));
    paramElems[1]->setAttribute((const XMLCh*)XMLStringConverter("name"),
                                (const XMLCh*)XMLStringConverter("numpoints"));
    paramElems[1]->setAttribute((const XMLCh*)XMLStringConverter("value"),
                                (const XMLCh*)XMLStringConverter(strS.str()));
    paramElems[1]->setAttribute((const XMLCh*)XMLStringConverter("type"),
                                (const XMLCh*)XMLStringConverter("int"));
    
    sampleElem->appendChild(paramElems[0]);
    sampleElem->appendChild(paramElems[1]);
  }

  return sampleElem;
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
  // HMM - not sure where I came up with that idea - seems this is 
  // type of output is only needed on rare occasion 
  //  better to stick with just mcrequest type outputs
  //xercesc::DOMElement* servOutElem = createDsmServOutElem(siteNode);
  //dsmElem->appendChild(servOutElem);

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

void Document::updateDSM(const std::string & dsmName,
                         const std::string & dsmId,
                         const std::string & dsmLocation,
                         QModelIndexList indexList)
{
cerr<<"entering Document::updateDSM\n";

  // Gather together all the elements we'll need to update the Sensor
  // in both the DOM model and the Nidas Model
  NidasModel *model = _configWindow->getModel();

  NidasItem *item;
  if (indexList.size() > 0)  {
    for (int i=0; i<indexList.size(); i++) {
      QModelIndex index = indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue;
      item = model->getItem(index);
    }
  }

  if (!item) throw InternalProcessingException("null DSMConfig");
  DSMItem* dsmItem = dynamic_cast<DSMItem*>(item);
  if (!dsmItem) throw InternalProcessingException("DSM Item not selected.");

  // Get the DSM and save all the current values, then update
  // to the new values
  DSMConfig* dsm = dsmItem->getDSMConfig();
  std::string currDSMName = dsm->getName();
  dsm_sample_id_t currDSMId = dsm->getId();
  std::string currLocation = dsm->getLocation();

  // Now update the DSM DOM
  updateDSMDOM(dsmItem, dsmName, dsmId, dsmLocation);

  // Now we need to validate that all is right with the updated dsm
  // in the nidas world and if not, change it all back.
  try {
cerr<<" Getting and validating site.\n";
    dsmItem->fromDOM();
    Site* site = const_cast<Site *>(dsmItem->getDSMConfig()->getSite());
    site->validate();
  } catch (nidas::util::InvalidParameterException &e) {
    stringstream strS;
    strS<<currDSMId;
    updateDSMDOM(dsmItem, currDSMName, strS.str(), currLocation);
    dsmItem->fromDOM();
    throw(e); // notify GUI
  } catch (InternalProcessingException) {
    stringstream strS;
    strS<<currDSMId;
    this->updateDSMDOM(dsmItem, currDSMName, strS.str(), currLocation);
    dsmItem->fromDOM();
    throw; // notify GUI
  }
}

void Document::updateDSMDOM(DSMItem* dsmItem, 
                            const std::string & dsmName,
                            const std::string & dsmId, 
                            const std::string & dsmLocation)
{
  // get DOM node for this DSM
  xercesc::DOMNode *dsmNode = dsmItem->getDOMNode();
  if (!dsmNode) 
    throw InternalProcessingException("null DSM DOM node!");

  // get the DOM Element for this DSM
  if (dsmNode->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
    throw InternalProcessingException("DSM DOM Node is not an Element Node!");
  xercesc::DOMElement* dsmElem = ((xercesc::DOMElement*) dsmNode);

  // insert new values into the DOM element

  dsmElem->removeAttribute((const XMLCh*)XMLStringConverter("name"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter(dsmName));
  dsmElem->removeAttribute((const XMLCh*)XMLStringConverter("id"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("id"),
                        (const XMLCh*)XMLStringConverter(dsmId));
  dsmElem->removeAttribute((const XMLCh*)XMLStringConverter("location"));
  dsmElem->setAttribute((const XMLCh*)XMLStringConverter("location"),
                        (const XMLCh*)XMLStringConverter(dsmLocation));
cerr<<"updateDSMDOM - name:" << dsmName << " ID:"<<dsmId<<" loc:"<<dsmLocation<< "\n";

  return;
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

  SensorItem * sensorItem = dynamic_cast<SensorItem*>(model->getCurrentRootItem());
  if (!sensorItem) {
    throw InternalProcessingException("Parent of VariableItem is not a SensorItem!");
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

void Document::updateVariable(VariableItem * varItem,
                              const std::string & varName, 
                              const std::string & varLongName, 
                              const std::string & varSR, 
                              const std::string & varUnits, 
                              vector <std::string> cals)
{
cerr<<"Document::updateVariable\n";
  NidasModel *model = _configWindow->getModel();
  SensorItem * sensorItem = dynamic_cast<SensorItem*>(model->getCurrentRootItem());
  if (!sensorItem)
    throw InternalProcessingException("Current root index is not a SensorItem.");

  DSMSensor* sensor;
  sensor = dynamic_cast<DSMSensor*>(sensorItem->getDSMSensor());
  if (!sensor)
    throw InternalProcessingException("Current root nidas element is not a DSMSensor.");
cerr << "  got sensor item \n";


  // We need to make a copy of the whole of the sample 
  // element either from the catalog or from the previous instantiation
  // of the sample element in the sensor element.  So see if it's from 
  //  a previous instantiation within the sensor element and if so, copy that 
  // instantiation and update the variable accordingly.  If not then it must be 
  // an IDREF from the sensorcatalog, get a copy of that sample element
  // modify its variable and put it in place
  // otherwise then something's really wrong!

  DOMNode *sampleNode = 0;
  DOMElement *origSampleElem = 0;
  DOMNode *newSampleNode = 0;
  DOMElement *newSampleElem = 0;
  DOMNode * variableNode = 0;
  
cerr<<" Testing varItem - name = "<<varItem->name().toStdString()<<"\n";
cerr<<"  about to findSampleDOMNode for sampleID: " << varItem->getSampleId() << "\n";
  sampleNode = sensorItem->findSampleDOMNode(varItem->getSampleId());
  if (sampleNode != 0) {  // sample is defined in <sensor>
    cerr<<"  found sample node defined in <site> xml - looking for var\n";
  
    newSampleNode = sampleNode->cloneNode(true); // complete copy 

  } else {  // Need to get newSampleNode from the catalog

    cerr<<"  sample node not defined in <site> xml, looking in catalog.\n";

    QString siBName = sensorItem->getBaseName();

    if(!_project->getSensorCatalog()) 
       throw InternalProcessingException("Document::updateVariable - can't find sensor catalog!");

    map<string,xercesc::DOMElement*>::const_iterator mi;

    // Find the sensor in the sensor catalog
    for (mi = _project->getSensorCatalog()->begin();
         mi != _project->getSensorCatalog()->end(); mi++) {
      if (mi->first == siBName.toStdString()) {
        cerr << "  found sensor node in catalog\n";

        // find Sample node in the sensor node from the catalog
        DOMNodeList * sampleNodes = mi->second->getChildNodes(); // from DOMElement?
        for (XMLSize_t i = 0; i < sampleNodes->getLength(); i++) {
          DOMNode * sensorChild = sampleNodes->item(i);
          if ( ((string)XMLStringConverter(sensorChild->getNodeName())).find("sample")==
               string::npos ) continue;  // not a sample item

          XDOMElement xnode((DOMElement *)sampleNodes->item(i));
          const string& sSampleId = xnode.getAttributeValue("id");
          if ((unsigned int)atoi(sSampleId.c_str()) == varItem->getSampleId()) {
            cerr<<"  about to clone the sample node in the catalog\n";
            sampleNode = sampleNodes->item(i);
            newSampleNode = sampleNodes->item(i)->cloneNode(true);
            cerr<<"   cloned the node\n";
            break;
          }
        }
      }
    }
  }

  origSampleElem = ((xercesc::DOMElement*) sampleNode);

  if (!newSampleNode) {
    std::cerr << "  clone of sample node appears to have failed!\n";
    throw InternalProcessingException("Document::updateVariable - clone of sample node seems to have failed");
  }

  // set the sample rate based on user input
  newSampleElem = (DOMElement*) newSampleNode;
  newSampleElem->removeAttribute((const XMLCh*)XMLStringConverter("rate"));
  newSampleElem->setAttribute((const XMLCh*)XMLStringConverter("rate"),
                              (const XMLCh*)XMLStringConverter(varSR));

  // Find the variable in the copy of the samplenode
  DOMNodeList * variableNodes = newSampleNode->getChildNodes();
  if (variableNodes == 0) {
    std::cerr << "  found no children nodes in copied sample node \n";
    throw InternalProcessingException("Document::updateVariable - copy of sample node has no children - something is very wrong!");
  }
  std::string variableName = varItem->getBaseName();
cerr<< "after call to varItem->getBaseName - name = " ;
cerr<< variableName <<"\n";
  for (XMLSize_t i = 0; i < variableNodes->getLength(); i++)
  {
     DOMNode * sampleChild = variableNodes->item(i);
     if (((string)XMLStringConverter(sampleChild->getNodeName())).find("variable")
            == string::npos ) continue;

     XDOMElement xnode((DOMElement *)variableNodes->item(i));
     const std::string& sVariableName = xnode.getAttributeValue("name");
     if (sVariableName.c_str() == variableName) {
       variableNode = variableNodes->item(i);
       cerr << "  Found variable node in sample copy!\n";
       break;
     }
  }

  // Look through variable node children, find and eliminate calibrations
  DOMNodeList * varChildNodes = variableNode->getChildNodes();
  DOMNode * varCalChild = 0;
  for (XMLSize_t i = 0; i < varChildNodes->getLength(); i++)
  {
    DOMNode * varChild = varChildNodes->item(i);
    if (((string)XMLStringConverter(varChild->getNodeName())).find("poly") 
        == string::npos && 
        ((string)XMLStringConverter(varChild->getNodeName())).find("linear") 
        == string::npos)   
      continue;

    cerr << "  Found a calibration node - setting up to remove it\n";
    varCalChild = varChild;
  }

  // found a poly or linear node - remove it
  if (varCalChild)
    DOMNode * rmVarChild = variableNode->removeChild(varCalChild);

cerr<< "  getting ready to update variable copy\n";
  // Update values of variablenode in samplenode copy based on user input
  DOMElement * varElem = ((xercesc::DOMElement*) variableNode);
  varElem->removeAttribute((const XMLCh*)XMLStringConverter("name")); 
  varElem->setAttribute((const XMLCh*)XMLStringConverter("name"), 
                        (const XMLCh*)XMLStringConverter(varName));
  varElem->removeAttribute((const XMLCh*)XMLStringConverter("longname"));   
  varElem->setAttribute((const XMLCh*)XMLStringConverter("longname"),   
                        (const XMLCh*)XMLStringConverter(varLongName));
  varElem->removeAttribute((const XMLCh*)XMLStringConverter("units"));
  varElem->setAttribute((const XMLCh*)XMLStringConverter("units"),
                        (const XMLCh*)XMLStringConverter(varUnits));
cerr<< " updated variable copy\n";

  // Add Calibration info if the user provided it.
  if (cals[0].size()) {  
    addCalibElem(cals, varUnits, sampleNode, varElem);
  } 

  // update the nidas SampleTag using the new DOMElement
  SampleTag* origSampTag = varItem->getSampleTag();
cerr << "Calling fromDOM \n";
  try {
    origSampTag->fromDOMElement((xercesc::DOMElement*)newSampleElem);
  }
    catch(const n_u::InvalidParameterException& e) {
    origSampTag->fromDOMElement((xercesc::DOMElement*)origSampleElem);
    throw;
  }

  // Make sure all is right with the sample and variable
  // Note - this will require getting the site and doing a validate variables.
  try {
    Site* site = const_cast <Site *> (sensor->getSite());
cerr << "  doing site validation\n";
    site->validate();

  } catch (nidas::util::InvalidParameterException &e) {
    cerr << "Caught invalidparameter exception\n";
    // Return DOM to prior state
    origSampTag->fromDOMElement((xercesc::DOMElement*)origSampleElem);
    throw(e); // notify GUI
  } catch ( ... ) {
    cerr << "Caught unexpected error\n";
    // Return DOM to prior state
    origSampTag->fromDOMElement((xercesc::DOMElement*)origSampleElem);
    throw InternalProcessingException("Caught unexpected error trying to add A2D Variable to model.");
  }

cerr << "past check for valid var info\n";

    // add sample to Sensor DOM - get the varItem parent which should be the
    // SensorItem then get it's Dom
  DOMNode *sensorNode = 0;
  sensorNode = sensorItem->getDOMNode();
  try {
    sensorNode->appendChild(newSampleElem);
    DOMElement *sensorElem = (DOMElement*) sensorNode;
    sensor->fromDOMElement((DOMElement*)sensorElem);
  } catch (DOMException &e) {
     origSampTag->fromDOMElement((xercesc::DOMElement*)origSampleElem);
     throw InternalProcessingException("add var to sensor element: " +
                     (std::string)XMLStringConverter(e.getMessage()));
  }

cerr<<"added sample node to the DOM\n";

    // update Qt model
    // XXX returns bool
  
  //model->appendChild(sensorItem);

  return;
}

void Document::addA2DVariable(const std::string & a2dVarName, 
                              const std::string & a2dVarLongName, 
                              const std::string & a2dVarVolts, 
                              const std::string & a2dVarChannel,
                              const std::string & a2dVarSR, 
                              const std::string & a2dVarUnits, 
                              vector <std::string> cals)
{
cerr<<"entering Document::addA2DVariable about to make call to _configWindow->getModel()"  <<"\n";
  NidasModel *model = _configWindow->getModel();
cerr<<"got model \n";
  A2DSensorItem * sensorItem = dynamic_cast<A2DSensorItem*>(model->getCurrentRootItem());
  if (!sensorItem)
    throw InternalProcessingException("Current root index is not an A2D SensorItem.");

  DSMAnalogSensor* analogSensor;
  analogSensor = dynamic_cast<DSMAnalogSensor*>(sensorItem->getDSMSensor());
  if (!analogSensor)
    throw InternalProcessingException("Current root nidas element is not a DSMAnalogSensor.");
cerr << "got sensor item \n";

// Find or create the SampleTag that will house this variable
  SampleTag *sampleTag2Add2=0; 
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
    A2DVariableItem* variableItem = dynamic_cast<A2DVariableItem*>(sensorItem->child(i));
    if (!variableItem)
      throw InternalProcessingException("Found child of A2DSensorItem that's not an A2DVariableItem!");
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
      //DSMSensor* sensor = sensorItem->getDSMSensor();
      sampleTag2Add2->setSensorId(analogSensor->getSensorId());
      sampleTag2Add2->setDSMId(analogSensor->getDSMId());
      sampleTag2Add2->fromDOMElement((xercesc::DOMElement*)newSampleElem);
      analogSensor->addSampleTag(sampleTag2Add2);
 
cerr << "after fromdom newSampleElem = " << newSampleElem << "\n";
cerr<<"added SampleTag to the Sensor\n";

      // add sample to DOM
      DOMNode * sensorNode = sensorItem->getDOMNode();
      try {
        sampleNode = sensorNode->appendChild(newSampleElem);
      } catch (DOMException &e) {
        analogSensor->removeSampleTag(sampleTag2Add2);  // keep nidas Project tree in sync with DOM
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

//  Getting the sampleNode - if we created a newSampleElem above then we just 
//  need to cast it as a DOMNode, but if not, we need to step through sample 
//  nodes of this sensor until we find the one with the right ID
  if (!createdNewSamp)  {
//cerr << "Assigning new Sample Element = " << newSampleElem << "  to SampleNode\n";
 //   sampleNode = ((xercesc::DOMNode *) newSampleElem);
//cerr << " After assignment SampleNode = " << sampleNode << "\n";
  //}
  //else  
    sampleNode = sensorItem->findSampleDOMNode(sampleTag2Add2->getSampleId());
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
    analogSensor->setA2DParameters(atoi(a2dVarChannel.c_str()), 4, 0);
  } else
  if (a2dVarVolts == " -5 to  5 Volts") {
      gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("2"));
      biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("true"));
    analogSensor->setA2DParameters(atoi(a2dVarChannel.c_str()), 2, 1);
  } else
  if (a2dVarVolts == "  0 to 10 Volts") {
    gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("2"));
    biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("false"));
    analogSensor->setA2DParameters(atoi(a2dVarChannel.c_str()), 2, 0);
  } else
  if (a2dVarVolts == "-10 to 10 Volts") {
    gainParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("1"));
    biPolarParmElem->setAttribute((const XMLCh*)XMLStringConverter("value"),
                           (const XMLCh*)XMLStringConverter("true"));
    analogSensor->setA2DParameters(atoi(a2dVarChannel.c_str()), 1, 1);
  } else 
    throw InternalProcessingException("Voltage choice not found in Document if/else block!");

  a2dVarElem->appendChild(chanParmElem);
  a2dVarElem->appendChild(gainParmElem);
  a2dVarElem->appendChild(biPolarParmElem);

  // Add Calibration info if the user provided it.
  if (cals[0].size()) {  
    addCalibElem(cals, a2dVarUnits, sampleNode, a2dVarElem);
  } 
  
    // add a2dVar to nidas project by doing a fromDOM

    Variable* a2dVar = new Variable();
cerr << "Calling fromDOM \n";
    try {
                a2dVar->fromDOMElement((xercesc::DOMElement*)a2dVarElem);
    }
    catch(const n_u::InvalidParameterException& e) {
        delete a2dVar;
        throw;
    }
cerr << "setting a2d Channel for new variable to value" << a2dVarChannel.c_str() << "\n";
    a2dVar->setA2dChannel(atoi(a2dVarChannel.c_str()));

  // Make sure we have a new unique a2dVar name 
  // Note - this will require getting the site and doing a validate variables.
  try {
cerr << "adding variable to sample tag\n";
    sampleTag2Add2->addVariable(a2dVar);
    Site* site = const_cast <Site *> (sampleTag2Add2->getSite());
cerr << "doing site validation\n";
    site->validate();

  } catch (nidas::util::InvalidParameterException &e) {
    cerr << "Caught invalidparameter exception\n";
    sampleTag2Add2->removeVariable(a2dVar); // validation failed so get it out of nidas Project tree
    throw(e); // notify GUI
  } catch ( ... ) {
    cerr << "Caught unexpected error\n";
    delete a2dVar;
    throw InternalProcessingException("Caught unexpected error trying to add A2D Variable to model.");
  }

cerr << "past check for valid a2dVar info\n";

    // add a2dVar to DOM
  try {
    sampleNode->appendChild(a2dVarElem);
  } catch (DOMException &e) {
     sampleTag2Add2->removeVariable(a2dVar);  // keep nidas Project tree in sync with DOM
     throw InternalProcessingException("add a2dVar to dsm element: " + 
                     (std::string)XMLStringConverter(e.getMessage()));
  }

cerr<<"added a2dVar node to the DOM\n";

    // update Qt model
    // XXX returns bool
  model->appendChild(sensorItem);

//   printSiteNames();
}

void Document::addCalibElem(std::vector <std::string> cals, 
                            const std::string & VarUnits, 
                            xercesc::DOMNode *sampleNode,
                            xercesc::DOMElement *varElem)
{
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
                             (const XMLCh*)XMLStringConverter(VarUnits));
    polyElem->setAttribute((const XMLCh*)XMLStringConverter("coefs"),
                             (const XMLCh*)XMLStringConverter(polyStr));

    varElem->appendChild(polyElem);

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
                             (const XMLCh*)XMLStringConverter(VarUnits));
    linearElem->setAttribute((const XMLCh*)XMLStringConverter("intercept"),
                             (const XMLCh*)XMLStringConverter(cals[0]));
    linearElem->setAttribute((const XMLCh*)XMLStringConverter("slope"),
                             (const XMLCh*)XMLStringConverter(cals[1]));

    varElem->appendChild(linearElem); 
  }
} // add CalibElem
