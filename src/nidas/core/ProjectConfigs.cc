/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-06-21 23:33:19 -0600 (Wed, 21 Jun 2006) $

    $LastChangedRevision: 3406 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/Project.cc $
 ********************************************************************

*/

#include <nidas/core/ProjectConfigs.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLWriter.h>
#include <nidas/core/XDOM.h>
#include <nidas/core/DSMEngine.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

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

void ProjectConfigs::addConfig(ProjectConfig* val)
    throw(n_u::InvalidParameterException)
{
    list<ProjectConfig*>::iterator ci = configs.begin();
    list<const ProjectConfig*>::iterator cci = constConfigs.begin();
    for ( ; ci != configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (val->getBeginTime() < cfg->getBeginTime()) {
            if (val->getEndTime() > cfg->getBeginTime())
                throw n_u::InvalidParameterException(val->getName(),"end time",
                    string("is greater than begin time of ") + cfg->getName());
            configs.insert(ci,val); // insert before ci
            constConfigs.insert(cci,val); // insert before ci
            return;
        }
    }
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
		cfg->getEndTime() >= ut) return cfg;
    }
    return 0;
}

const std::list<const ProjectConfig*>& ProjectConfigs::getConfigs() const
{
    return constConfigs;
}


/* static */
Project* ProjectConfigs::getProject(const string& xmlFileName,
        const n_u::UTime& beginTime)
        throw(XMLException,
		n_u::InvalidParameterException)
{
    string configXML =
        Project::expandEnvVars(xmlFileName);

    ProjectConfigs configs;
    configs.parseXML(configXML);

    const ProjectConfig* cfg = configs.getConfig(beginTime);
    if (!cfg) throw n_u::InvalidParameterException(configXML,
        "no config for time",beginTime.format(true,"%c"));
    string xmlFileName2 = Project::expandEnvVars(cfg->getXMLName());

    struct stat statbuf;
    if (::stat(xmlFileName2.c_str(),&statbuf) < 0)
        throw n_u::IOException(xmlFileName2,"open",errno);

    auto_ptr<xercesc::DOMDocument> doc(
        DSMEngine::parseXMLConfigFile(xmlFileName2));

    auto_ptr<Project> project(Project::getInstance());
    project->fromDOMElement(doc->getDocumentElement());
    return project.release();
}

void ProjectConfigs::parseXML(const std::string& xmlFileName)
    throw(XMLException,
	 nidas::util::InvalidParameterException)
{
    XMLParser parser;
    parser.setXercesUserAdoptsDOMDocument(true);

    xercesc::DOMDocument* doc = parser.parse(xmlFileName);

    xercesc::DOMElement* node = doc->getDocumentElement();

    fromDOMElement(node);
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
    throw(XMLException)
{
    XMLStringConverter qualifiedName("configs");
    xercesc::DOMDocument* doc =
        XMLImplementation::getImplementation()->createDocument(
            DOMable::getNamespaceURI(), (const XMLCh *)qualifiedName,0);
    toDOMParent((xercesc::DOMElement*)doc);
    XMLWriter writer;
    writer.write(doc,xmlFileName);
}

xercesc::DOMElement* ProjectConfigs::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException) {
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("configs"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* ProjectConfigs::toDOMElement(xercesc::DOMElement* elem) const
    throw(xercesc::DOMException)
{
    const std::list<const ProjectConfig*>& cfgs = getConfigs();
    std::list<const ProjectConfig*>::const_iterator ci =  cfgs.begin();
    for ( ; ci != cfgs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        cfg->toDOMParent(elem);
    }
    return elem;
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
		n_u::UTime ut;
		try {
		    ut.set(atval,true);
		}
		catch(const n_u::ParseException& e) {
		    throw n_u::InvalidParameterException(atname,
		    	atval,e.what());
		}
		setBeginTime(ut);
	    }
	    else if (atname == "end") {
		n_u::UTime ut;
		try {
		    ut.set(atval,true);
		}
		catch(const n_u::ParseException& e) {
		    throw n_u::InvalidParameterException(atname,
		    	atval,e.what());
		}
		setEndTime(ut);
	    }
	}
    }
}

xercesc::DOMElement* ProjectConfig::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("config"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* ProjectConfig::toDOMElement(xercesc::DOMElement* elem) const
throw(xercesc::DOMException)
{
    XDOMElement xelem(elem);
    xelem.setAttributeValue("name",getName());
    xelem.setAttributeValue("xml",getXMLName());
    xelem.setAttributeValue("begin",getXMLName());
    xelem.setAttributeValue("end",getXMLName());
    return elem;
}

