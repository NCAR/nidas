// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include "Datasets.h"
#include "XMLWriter.h"
#include "XDOM.h"
#include "DOMable.h"
#include <nidas/util/Process.h>

#include <iostream>
#include <sstream>

#ifdef NEEDED
#include <memory> // auto_ptr<>
#include <sys/stat.h>
#endif

#include <unistd.h> // close()
#include <algorithm>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

Dataset::Dataset(): _name(),_desc(),_resSecs(0.0), _envVars()
{
}

Datasets::Datasets(): _xmlName(),_datasetsByName()
{
}

void Datasets::addDataset(const Dataset& val)
    throw(n_u::InvalidParameterException)
{
    _datasetsByName[val.getName()] = val;
}

void Datasets::removeDataset(const Dataset& val)
{
    std::map<std::string, Dataset>::iterator di;
    if ((di = _datasetsByName.find(val.getName())) != _datasetsByName.end())
        _datasetsByName.erase(di);
}

const Dataset& Datasets::getDataset(const string& name) const
    throw(n_u::InvalidParameterException)
{
    std::map<std::string, Dataset>::const_iterator di;
    if ((di = _datasetsByName.find(name)) == _datasetsByName.end())
        throw n_u::InvalidParameterException(_xmlName,
                  "no dataset for name",name);
    return di->second;
}

std::list<Dataset> Datasets::getDatasets() const
{
    std::list<Dataset> dsets;
    std::map<std::string, Dataset>::const_iterator di = _datasetsByName.begin();
    for ( ; di != _datasetsByName.end(); ++di)
        dsets.push_back(di->second);
    return dsets;
}

void Datasets::parseXML(const std::string& xmlFileName,bool verbose)
    throw(nidas::core::XMLException,
	 nidas::util::InvalidParameterException)
{
    _xmlName = xmlFileName;

    XMLParser parser;

    xercesc::DOMDocument* doc = parser.parse(xmlFileName,verbose);

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
void Datasets::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
        if (elname == "dataset") {
            Dataset dataset;
	    dataset.fromDOMElement((xercesc::DOMElement*)child);
	    addDataset(dataset);
	}
    }
}

void Datasets::writeXML(const std::string& xmlFileName)
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
    XMLStringConverter qualifiedName("datasets");

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

xercesc::DOMElement* Datasets::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException)
{

    // cerr << "datasets, start toDOMParent" << endl;
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("datasets"));
    // cerr << "datasets, appendChild" << endl;
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* Datasets::toDOMElement(xercesc::DOMElement* elem) const
    throw(xercesc::DOMException)
{
    // cerr << "datasets, start toDOMElement" << endl;
    std::list<Dataset> datasets = getDatasets();
    std::list<Dataset>::const_iterator di =  datasets.begin();
    for ( ; di != datasets.end(); ++di) {
        const Dataset& dataset = *di;
        dataset.toDOMParent(elem);
    }
    return elem;
}

void Dataset::addEnvironmentVariable(const string& name, const string& value)
{
    _envVars.insert(make_pair(name,value));
}

void Dataset::putenv() const
{
    n_u::Process::setEnvVar("DATASET",getName());
    map<string,string>::const_iterator vi = _envVars.begin();
    for ( ; vi != _envVars.end(); ++vi) {
        string name = vi->first;
        string value = vi->second;
        n_u::Process::setEnvVar(name,value);
    }
}

void Dataset::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    if (xnode.getNodeName() != "dataset")
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
	    else if (atname == "description") setDescription(atval);
	    else if (atname == "resolution") {
                std::istringstream ist(atval);
                ist >> _resSecs;
                if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),atname, atval);
	    }
            else throw n_u::InvalidParameterException("dataset",
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
                    string("dataset ") + getName(),
                    "envvar element","must have a name and value attribute");
            _envVars.insert(make_pair(ename,evalue));
	}
        else throw n_u::InvalidParameterException("dataset",
		    	elname,"unknown element");
    }
}

xercesc::DOMElement* Dataset::toDOMParent(xercesc::DOMElement* parent) const
    throw(xercesc::DOMException)
{
    // cerr << "dataset, start toDOMParent" << endl;
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                DOMable::getNamespaceURI(),
                (const XMLCh*)XMLStringConverter("dataset"));
    parent->appendChild(elem);
    return toDOMElement(elem);
}
xercesc::DOMElement* Dataset::toDOMElement(xercesc::DOMElement* elem) const
throw(xercesc::DOMException)
{
    // cerr << "dataset, start toDOMElement" << endl;
    XDOMElement xelem(elem);
    xelem.setAttributeValue("name",getName());
    xelem.setAttributeValue("description",getDescription());
    ostringstream ost;
    ost << getResolutionSecs();
    xelem.setAttributeValue("resolution",ost.str());

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

