/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleTag.h>
#include <sstream>
#include <iostream>

using namespace dsm;
using namespace std;

SampleTag::~SampleTag()
{
    for (list<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) delete *vi;
}

void SampleTag::addVariable(Variable* var)
	throw(atdUtil::InvalidParameterException)
{
    variables.push_back(var);
    constVariables.push_back(var);
}

const std::vector<const Variable*>& SampleTag::getVariables() const
{
    return constVariables;
}

void SampleTag::fromDOMElement(const xercesc::DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    istringstream ist(attr.getValue());
	    if (!attr.getName().compare("id")) {
		unsigned short val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("sample","id",
		    	attr.getValue());
		setId(val);
		cerr << "attr=" << attr.getValue() << " id=" << val << endl;
	    }
	    else if (!attr.getName().compare("rate")) {
		float rate;
		ist >> rate;
		if (ist.fail() || rate < 0.0)
		    throw atdUtil::InvalidParameterException("sample","rate",
		    	attr.getValue());
		setRate(rate);
	    }
	}
    }
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("variable")) {
	    Variable* var = new Variable();
	    var->fromDOMElement((xercesc::DOMElement*)child);
	    addVariable(var);
	}
	else throw atdUtil::InvalidParameterException("sample",
		"unknown child element of sample",elname);
		    	
    }
}

xercesc::DOMElement* SampleTag::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* SampleTag::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}


