/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLWriter.h>
#include <nidas/core/XDOM.h>
#include <nidas/core/DSMEngine.h>

#include <nidas/util/Process.h>

#include <iostream>
#include <memory> // auto_ptr<>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ProjectConfig::ProjectConfig()
{
    // default end of project is a year after the start
    setEndTime(getBeginTime() + USECS_PER_DAY * 365 * 2);
}

Project* ProjectConfig::getProject() const throw(nidas::core::XMLException,
		n_u::InvalidParameterException)
{
    string xmlFileName2 = n_u::Process::expandEnvVars(getXMLName());

    struct stat statbuf;
    if (::stat(xmlFileName2.c_str(),&statbuf) < 0)
        throw n_u::IOException(xmlFileName2,"open",errno);

    xercesc::DOMDocument* doc = DSMEngine::parseXMLConfigFile(xmlFileName2);

    Project* project = Project::getInstance();

    // set the environment variables for this configuration.
    // Note, there is no config state maintained, where
    // the environment of a previous config is unset.
    putenv();

    try {
        project->fromDOMElement(doc->getDocumentElement());
    }
    catch(...) {
        doc->release();
        throw;
    }
    doc->release();
    return project;
}

ProjectConfigs::ProjectConfigs()
{
}

ProjectConfigs::~ProjectConfigs()
{
    list<ProjectConfig*>::const_iterator ci = configs.begin();
    for ( ; ci != configs.end(); ++ci) {
        ProjectConfig* cfg = *ci;
	delete cfg;
    }
}

namespace {
string configErrorMsg(const ProjectConfig* c1,const ProjectConfig* c2)
{
    ostringstream ost;
    ost << "end time of config named " << c1->getName() <<
        " is later than begin time of next config named " << c2->getName();
    return ost.str();
}
}

void ProjectConfigs::addConfig(ProjectConfig* val)
    throw(n_u::InvalidParameterException)
{
    list<ProjectConfig*>::iterator ci = configs.begin();
    list<const ProjectConfig*>::iterator cci = constConfigs.begin();
    ProjectConfig* pcfg = 0;
    for ( ; ci != configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (val->getBeginTime() < cfg->getBeginTime()) {
            if (val->getEndTime() > cfg->getBeginTime())
                throw n_u::InvalidParameterException(configErrorMsg(val,cfg));
            if (pcfg && pcfg->getEndTime() > val->getBeginTime())
                throw n_u::InvalidParameterException(configErrorMsg(pcfg,val));
            configs.insert(ci,val); // insert before ci
            constConfigs.insert(cci,val); // insert before ci
            return;
        }
        if (val->getBeginTime() == cfg->getBeginTime()) {
            list<ProjectConfig*>::iterator ci2 = ci;
            ci2++;
            if (ci2 != configs.end()) {
                ProjectConfig* cfg2 = *ci2;
                if (val->getEndTime() > cfg2->getBeginTime())
                    throw n_u::InvalidParameterException(configErrorMsg(val,pcfg));
            }
            delete cfg;
            *ci = val;
            *cci = val;
            return;
        }
        pcfg = cfg;
    }
    if (pcfg && pcfg->getEndTime() > val->getBeginTime())
        throw n_u::InvalidParameterException(configErrorMsg(pcfg,val));
    configs.insert(ci,val); // append
    constConfigs.insert(cci,val); // append
}

void ProjectConfigs::removeConfig(const ProjectConfig* val)
{
    list<ProjectConfig*>::iterator ci = configs.begin();
    list<const ProjectConfig*>::iterator cci = constConfigs.begin();
    for ( ; ci != configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (cfg == val) {
            configs.erase(ci);
            constConfigs.erase(cci);
            delete cfg;
            return;
        }
    }
}

const ProjectConfig* ProjectConfigs::getConfig(const n_u::UTime& ut) const
    throw(n_u::InvalidParameterException)
{
    list<const ProjectConfig*>::const_iterator ci = constConfigs.begin();
    for ( ; ci != constConfigs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
#ifdef DEBUG
        cerr << "ut=" << ut.format(true,"%c") << endl;
        cerr << "begin=" << cfg->getBeginTime().format(true,"%c") << endl;
        cerr << "end=" << cfg->getEndTime().format(true,"%c") << endl;
#endif
	if (cfg->getBeginTime() <= ut &&
		cfg->getEndTime() > ut) return cfg;
    }
    throw n_u::InvalidParameterException(_xmlName,
              "no config for time",ut.format(true,"%c"));
}

const ProjectConfig* ProjectConfigs::getConfig(const string& name) const
    throw(n_u::InvalidParameterException)
{
    list<const ProjectConfig*>::const_iterator ci = constConfigs.begin();
    for ( ; ci != constConfigs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        if (cfg->getName() == name) return cfg;
    }
    throw n_u::InvalidParameterException(_xmlName,
              "no config for name",name);
}

const std::list<const ProjectConfig*>& ProjectConfigs::getConfigs() const
{
    return constConfigs;
}

void ProjectConfigs::parseXML(const std::string& xmlFileName)
    throw(nidas::core::XMLException,
	 nidas::util::InvalidParameterException)
{
    _xmlName = xmlFileName;

    XMLParser parser;
    parser.setXercesUserAdoptsDOMDocument(true);

    xercesc::DOMDocument* doc = parser.parse(xmlFileName);

    xercesc::DOMElement* node = doc->getDocumentElement();

    try {
        fromDOMElement(node);
    }
    catch(...) {
        doc->release();
        throw;
    }
    doc->release();
}
void ProjectConfigs::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
        if (elname == "config") {
            ProjectConfig* config = new ProjectConfig();
	    config->fromDOMElement((xercesc::DOMElement*)child);
	    addConfig(config);
	}
    }
}

void ProjectConfigs::writeXML(const std::string& xmlFileName)
    throw(nidas::core::XMLException,n_u::IOException)
{
    /*
     * From www.w3.org Dom level 3 docs:
     qualified name
         A qualified name is the name of an element or attribute
         defined as the concatenation of a local name
         (as defined in this specification), optionally preceded
         by a namespace prefix and colon character.
         See Qualified Names in Namespaces in XML [XML Namespaces].
    */
    XMLStringConverter qualifiedName("configs");

    xercesc::DOMDocument* doc =
        XMLImplementation::getImplementation()->createDocument(
            DOMable::getNamespaceURI(), (const XMLCh *)qualifiedName,0);
    try {
        toDOMElement(doc->getDocumentElement());
    }
    catch(const xercesc::DOMException& e) {
        // XMLStringConverter excmsg(e.getMessage());
        // cerr << "DOMException: " << excmsg << endl;
        doc->release();
        throw nidas::core::XMLException(e);
    }

    char* tmpName = new char[xmlFileName.length() + 8];
    strcpy(tmpName,xmlFileName.c_str());
    strcat(tmpName,".XXXXXX");
    int fd = mkstemp(tmpName);
    string newName = tmpName;
    delete [] tmpName;
    try {
        if (fd < 0) throw n_u::IOException(newName,"create",errno);
        ::close(fd);

        // cerr << "newName=" << newName << endl;

        XMLWriter writer;
        writer.setPrettyPrint(true);
        writer.write(doc,newName);

        if (::rename(newName.c_str(),xmlFileName.c_str()) < 0)
            throw n_u::IOException(newName,"rename",errno);
    }
    catch(...) {
        doc->release();
        throw;
    }
    doc->release();
}

xercesc::DOMElement* ProjectConfigs::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException)
{

    // cerr << "configs, start toDOMParent" << endl;
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("configs"));
    // cerr << "configs, appendChild" << endl;
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* ProjectConfigs::toDOMElement(xercesc::DOMElement* elem) const
    throw(xercesc::DOMException)
{
    // cerr << "configs, start toDOMElement" << endl;
    const std::list<const ProjectConfig*>& cfgs = getConfigs();
    std::list<const ProjectConfig*>::const_iterator ci =  cfgs.begin();
    for ( ; ci != cfgs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        cfg->toDOMParent(elem);
    }
    return elem;
}

void ProjectConfig::addEnvironmentVariable(const string& name, const string& value)
{
    _envVars.insert(make_pair(name,value));
}

void ProjectConfig::putenv() const
{
    string aircraft;
    if (n_u::Process::getEnvVar("AIRCRAFT",aircraft)) n_u::Process::setEnvVar("FLIGHT",getName());
    map<string,string>::const_iterator vi = _envVars.begin();
    for ( ; vi != _envVars.end(); ++vi) {
        string name = vi->first;
        string value = vi->second;
        n_u::Process::setEnvVar(name,value);
    }
}

void ProjectConfig::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    if (xnode.getNodeName() != "config")
	    throw n_u::InvalidParameterException(
		    "Project::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
	// get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    const string& atname = attr.getName();
	    const string& atval = attr.getValue();
	    if (atname == "name") setName(atval);
	    else if (atname == "xml") setXMLName(atval);
	    else if (atname == "begin") {
		try {
                    n_u::UTime ut = n_u::UTime::parse(true,atval);
                    setBeginTime(ut);
		}
		catch(const n_u::ParseException& e) {
		    throw n_u::InvalidParameterException(atname,
		    	atval,e.what());
		}
	    }
	    else if (atname == "end") {
		try {
                    n_u::UTime ut = n_u::UTime::parse(true,atval);
                    setEndTime(ut);
		}
		catch(const n_u::ParseException& e) {
		    throw n_u::InvalidParameterException(atname,
		    	atval,e.what());
		}
	    }
            else throw n_u::InvalidParameterException("config",
		    	atname,"unknown attribute");
	}
    }

    // check for envvar child elements
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
        if (elname == "envvar") {
            string ename;
            string evalue;
            if(child->hasAttributes()) {
                // get all the attributes of the node
                xercesc::DOMNamedNodeMap *pAttributes = child->getAttributes();
                int nSize = pAttributes->getLength();
                for(int i=0;i<nSize;++i) {
                    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
                    const string& atname = attr.getName();
                    const string& atval = attr.getValue();
                    if (atname == "name") ename = atval;
                    else if (atname == "value") evalue = atval;
                    else throw n_u::InvalidParameterException("envvar",
                        atname,"unknown attribute");
                }
            }
            if (ename.length() == 0 || evalue.length() == 0)
                throw n_u::InvalidParameterException(
                    string("config ") + getName(),
                    "envvar element","must have a name and value attribute");
            _envVars.insert(make_pair(ename,evalue));
	}
    }
}

xercesc::DOMElement* ProjectConfig::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException)
{
    // cerr << "config, start toDOMParent" << endl;
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                DOMable::getNamespaceURI(),
                (const XMLCh*)XMLStringConverter("config"));
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* ProjectConfig::toDOMElement(xercesc::DOMElement* elem) const
throw(xercesc::DOMException)
{
    // cerr << "config, start toDOMElement" << endl;
    XDOMElement xelem(elem);
    xelem.setAttributeValue("name",getName());
    xelem.setAttributeValue("xml",getXMLName());
    xelem.setAttributeValue("begin",getBeginTime().format(true,"%Y %b %d %H:%M:%S"));
    xelem.setAttributeValue("end",getEndTime().format(true,"%Y %b %d %H:%M:%S"));
    map<string,string>::const_iterator vi = _envVars.begin();

    for ( ; vi != _envVars.end(); ++vi) {
        xercesc::DOMElement* envvarElement =
            elem->getOwnerDocument()->createElementNS(
                    DOMable::getNamespaceURI(),
                    (const XMLCh*)XMLStringConverter("envvar"));
        XDOMElement xenvvar(envvarElement);
        elem->appendChild(envvarElement);
        xenvvar.setAttributeValue("name",vi->first);
        xenvvar.setAttributeValue("value",vi->second);
    }
    return elem;
}

