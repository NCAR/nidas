/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleTag.h>
#include <sstream>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;
using namespace xercesc;

SampleTag::~SampleTag()
{
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) delete *vi;
}

void SampleTag::addVariable(Variable* var)
	throw(atdUtil::InvalidParameterException)
{
    if (getSuffix().length() > 0) var->setSuffix(getSuffix());
    variables.push_back(var);
    constVariables.push_back(var);
    var->setSampleTag(this);
}

const std::vector<const Variable*>& SampleTag::getVariables() const
{
    return constVariables;
}

void SampleTag::setSuffix(const std::string& val)
{
    suffix = val;
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) (*vi)->setSuffix(suffix);
}

void SampleTag::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    istringstream ist(attr.getValue());
	    if (!attr.getName().compare("id")) {
		unsigned short val;
		// If you unset the dec flag, then a leading '0' means
		// octal, and 0x means hex.
		ist.unsetf(ios::dec);
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sample","id",
		    	attr.getValue());
		setId(val);
		// cerr << "attr=" << attr.getValue() << " id=" << val << endl;
	    }
	    else if (!attr.getName().compare("rate")) {
		float rate;
		ist >> rate;
		if (ist.fail() || rate < 0.0)
		    throw atdUtil::InvalidParameterException("sample","rate",
		    	attr.getValue());
		setRate(rate);
	    }
	    else if (!attr.getName().compare("scanfFormat"))
		setScanfFormat(attr.getValue());
	}
    }
    unsigned int nvars = 0;
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("variable")) {
	    Variable* var;
	    if (nvars == variables.size()) var = new Variable();
	    else var = variables[nvars];
	    var->fromDOMElement((DOMElement*)child);
	    if (nvars == variables.size()) addVariable(var);
	    nvars++;
	}
	else throw atdUtil::InvalidParameterException("sample",
		"unknown child element of sample",elname);
		    	
    }
}

DOMElement* SampleTag::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* SampleTag::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


