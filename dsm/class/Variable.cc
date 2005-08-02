/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Variable.h>
#include <SampleTag.h>
#include <sstream>

using namespace dsm;
using namespace std;
using namespace xercesc;

Variable::Variable(): sampleTag(0),type(CONTINUOUS),length(1),converter(0)
{
}

Variable::Variable(const Variable& x):
	sampleTag(0),name(x.name),longname(x.longname),units(x.units),
	type(x.type),length(x.length),converter(0)
{
    if (x.converter) converter = x.converter->clone();
    const list<const Parameter*>& params = x.getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* parm = *pi;
	Parameter* newp = parm->clone();
	addParameter(newp);
    }
}

Variable::~Variable()
{
    delete converter;
    list<Parameter*>::const_iterator pi;
    for (pi = parameters.begin(); pi != parameters.end(); ++pi)
    	delete *pi;
}

void Variable::setName(const string& val)
{
    setNameWithSuffix(val,getSuffix());
}

void Variable::setSuffix(const string& val)
{   suffix = val;
    setNameWithSuffix(getName(),val);
}

void Variable::setNameWithSuffix(const string& nval,const string& sval) 
{
    int vl = nval.length();
    int sl = sval.length();
    // if val does not already have suffix, append it.
    if (vl < sl || nval.substr(vl-sl).compare(sval))
	name = nval + sval;
    else name = nval;
}

float Variable::getSampleRate() const {
    if (!sampleTag) return 0.0;
    else return sampleTag->getRate();
}


void Variable::fromDOMElement(const DOMElement* node)
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
	    if (!attr.getName().compare("name"))
		setName(attr.getValue());
	    else if (!attr.getName().compare("longname"))
		setLongName(attr.getValue());
	    else if (!attr.getName().compare("units"))
		setUnits(attr.getValue());
	    else if (!attr.getName().compare("length")) {
	        istringstream ist(attr.getValue());
		size_t val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
		    	"variable","length",attr.getValue());
		setLength(val);
	    }
	}
    }

    int nconverters = 0;
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;

        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();
	if (!elname.compare("parameter"))  {
	    Parameter* parameter =
	    	Parameter::createParameter((DOMElement*)child);
	    addParameter(parameter);
	}
	else {
	    if (nconverters > 0)
	    	throw atdUtil::InvalidParameterException(getName(),
		    "only one child converter allowed, <linear>, <poly> etc",
		    	elname);
	    VariableConverter* cvtr =
	    	VariableConverter::createVariableConverter(elname);
	    if (!cvtr) throw atdUtil::InvalidParameterException(getName(),
		    "unsupported child element",elname);
	    cvtr->fromDOMElement((DOMElement*)child);
	    setConverter(cvtr);
	    nconverters++;
	}
    }
}

DOMElement* Variable::toDOMParent(
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

DOMElement* Variable::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


