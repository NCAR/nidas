// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "ProjectConfigs.h"
#include "XMLWriter.h"
#include "XDOM.h"
#include "DSMEngine.h"

#include <nidas/util/Process.h>

#include <iostream>
#include <sys/stat.h>
#include <unistd.h> // close()

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ProjectConfig::ProjectConfig(): _name(),_xmlName(),
    _beginTime(LONG_LONG_MIN),_endTime(LONG_LONG_MIN),_envVars()
{
    // default end of project is a year after the start
    setEndTime(getBeginTime() + USECS_PER_DAY * 365 * 2);
}

void ProjectConfig::initProject(Project& project) const
{
    string xmlFileName2 = n_u::Process::expandEnvVars(getXMLName());

    struct stat statbuf;
    if (::stat(xmlFileName2.c_str(),&statbuf) < 0)
        throw n_u::IOException(xmlFileName2,"open",errno);

    xercesc::DOMDocument* doc = parseXMLConfigFile(xmlFileName2);

    // set the environment variables for this configuration.
    // Note, there is no config state maintained, where
    // the environment of a previous config is unset.
    putenv();

    try {
        project.fromDOMElement(doc->getDocumentElement());
    }
    catch(...) {
        doc->release();
        throw;
    }
    doc->release();
}

ProjectConfigs::ProjectConfigs(): _xmlName(),_constConfigs(),_configs()
{
}

ProjectConfigs::~ProjectConfigs()
{
    list<ProjectConfig*>::const_iterator ci = _configs.begin();
    for ( ; ci != _configs.end(); ++ci) {
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

void ProjectConfigs::addConfigByTime(ProjectConfig* val)
{
    list<ProjectConfig*>::iterator ci = _configs.begin();
    list<const ProjectConfig*>::iterator cci = _constConfigs.begin();
    ProjectConfig* prev_cfg = 0;
    for ( ; ci != _configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (val->getBeginTime() < cfg->getBeginTime()) {
            if (val->getEndTime() > cfg->getBeginTime())
                throw n_u::InvalidParameterException(configErrorMsg(val,cfg));
            if (prev_cfg && prev_cfg->getEndTime() > val->getBeginTime())
                throw n_u::InvalidParameterException(configErrorMsg(prev_cfg,val));
            _configs.insert(ci,val); // insert before ci
            _constConfigs.insert(cci,val); // insert before ci
            return;
        }
        if (val->getBeginTime() == cfg->getBeginTime()) {
            list<ProjectConfig*>::iterator ci2 = ci;
            ci2++;
            if (ci2 != _configs.end()) {
                ProjectConfig* cfg2 = *ci2;
                if (val->getEndTime() > cfg2->getBeginTime())
                    throw n_u::InvalidParameterException(configErrorMsg(val,prev_cfg));
            }
            delete cfg;
            *ci = val;
            *cci = val;
            return;
        }
        prev_cfg = cfg;
    }
    if (prev_cfg && prev_cfg->getEndTime() > val->getBeginTime())
        throw n_u::InvalidParameterException(configErrorMsg(prev_cfg,val));
    _configs.insert(ci,val); // append
    _constConfigs.insert(cci,val); // append
}

void ProjectConfigs::addConfigByName(ProjectConfig* val)
    throw()
{
    list<ProjectConfig*>::iterator ci = _configs.begin();
    list<const ProjectConfig*>::iterator cci = _constConfigs.begin();
    for ( ; ci != _configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (val->getName() == cfg->getName()) {
            delete cfg;
            *ci = val;
            *cci = val;
            return;
        }
    }
    _configs.insert(ci,val); // append
    _constConfigs.insert(cci,val); // append
}

void ProjectConfigs::removeConfig(const ProjectConfig* val)
{
    list<ProjectConfig*>::iterator ci = _configs.begin();
    list<const ProjectConfig*>::iterator cci = _constConfigs.begin();
    for ( ; ci != _configs.end(); ++ci,++cci) {
        ProjectConfig* cfg = *ci;
        if (cfg == val) {
            _configs.erase(ci);
            _constConfigs.erase(cci);
            delete cfg;
            return;
        }
    }
}

const ProjectConfig* ProjectConfigs::getConfig(const n_u::UTime& ut) const
{
    list<const ProjectConfig*>::const_iterator ci = _constConfigs.begin();
    for ( ; ci != _constConfigs.end(); ++ci) {
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

const ProjectConfig* ProjectConfigs::getConfig(const std::string& name) const
{
    list<const ProjectConfig*>::const_iterator ci = _constConfigs.begin();
    for ( ; ci != _constConfigs.end(); ++ci) {
        const ProjectConfig* cfg = *ci;
        if (cfg->getName() == name) return cfg;
    }
    throw n_u::InvalidParameterException(_xmlName,
              "no config for name",name);
}

const std::list<const ProjectConfig*>& ProjectConfigs::getConfigs() const
{
    return _constConfigs;
}

void ProjectConfigs::parseXML(const std::string& xmlFileName)
{
    _xmlName = xmlFileName;

    XMLParser parser;

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
	    addConfigByName(config);
	}
    }
}

void ProjectConfigs::writeXML(const std::string& xmlFileName)
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
            string evalue("_NOTSET_");
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
            if (ename.length() == 0 || evalue == "_NOTSET_")
                throw n_u::InvalidParameterException(
                    string("config ") + getName(),
                    "envvar element","must have a non-empty name and a value attribute");
            _envVars.insert(make_pair(ename,evalue));
	}
        else throw n_u::InvalidParameterException("config",
		    	elname,"unknown element");
    }
}

xercesc::DOMElement* ProjectConfig::toDOMParent(xercesc::DOMElement* parent) const
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

