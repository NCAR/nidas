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
#include <nidas/core/XDOM.h>

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

void ProjectConfigs::parseXML(const std::string& xmlFileName)
    throw(nidas::core::XMLException,
	 nidas::util::InvalidParameterException)
{
    XMLParser parser;
    xercesc::DOMDocument* doc = parser.parse(xmlFileName);

    xercesc::DOMElement* node = doc->getDocumentElement();

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
	    else if (atname == "begin") {
		n_u::UTime ut;
		try {
		    ut.parse(true,atval);
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
		    ut.parse(true,atval);
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

const ProjectConfig* ProjectConfigs::getConfig(const n_u::UTime& ut) const
{
    list<const ProjectConfig*>::const_iterator ci = constConfigs.begin();
    for ( ; ci != constConfigs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
	if (cfg->getBeginTime() <= ut &&
		cfg->getEndTime() >= ut) return cfg;
    }
    return 0;
}
