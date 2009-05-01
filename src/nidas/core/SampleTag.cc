/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleTag.h>
#include <nidas/core/Project.h>
#include <nidas/core/CalFile.h>

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleTag::SampleTag():
	id(0),sampleId(0),sensorId(0),station(0),
        rate(0.0),processed(true),dsm(0) {}

/* copy constructor */
SampleTag::SampleTag(const SampleTag& x):
	id(x.id),sampleId(x.sampleId),sensorId(x.sensorId),
	suffix(x.suffix),
	station(x.station),
	rate(x.rate),processed(x.processed),
	dsm(x.dsm),
	scanfFormat(x.scanfFormat),
        _promptString(x._promptString)
{
    const vector<const Variable*>& vars = x.getVariables();
    vector<const Variable*>::const_iterator vi;
    for (vi = vars.begin(); vi != vars.end(); ++vi) {
        const Variable* var = *vi;
	Variable* newv = new Variable(*var);
	addVariable(newv);
    }

    const list<const Parameter*>& params = x.getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* parm = *pi;
	Parameter* newp = parm->clone();
	addParameter(newp);
    }
}

SampleTag::~SampleTag()
{
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) delete *vi;

    list<Parameter*>::const_iterator pi;
    for (pi = parameters.begin(); pi != parameters.end(); ++pi)
    	delete *pi;
}

void SampleTag::addVariable(Variable* var)
	throw(n_u::InvalidParameterException)
{
    variables.push_back(var);
    constVariables.push_back(var);
    var->setSampleTag(this);
}

void SampleTag::setSuffix(const std::string& val)
{
    suffix = val;
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) {
	Variable* var = *vi;
	var->setSuffix(suffix);
    }

}

void SampleTag::setSiteAttributes(const Site* site)
{
    station = site->getNumber();
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) {
	Variable* var = *vi;
	var->setSiteAttributes(site);
    }
}

const std::vector<const Variable*>& SampleTag::getVariables() const
{
    return constVariables;
}

VariableIterator SampleTag::getVariableIterator() const
{
    return VariableIterator(this);
}

const Parameter* SampleTag::getParameter(const string& name) const
{
    list<const Parameter*>::const_iterator pi;
    for (pi = constParameters.begin(); pi != constParameters.end(); ++pi) {
        const Parameter* param = *pi;
    	if (param->getName() == name) return param;
    }
    return 0;
}

const Site* SampleTag::getSite() const 
{
    const Site* site = 0;
    const DSMConfig* dsm = getDSM();
    if (dsm) site = dsm->getSite();
    if (site) return site;

    if (getStation() > 0) {
	site = Project::getInstance()->findSite(getStation());
	if (site) return site;
    }
    if (!dsm) {
	dsm = Project::getInstance()->findDSM(getDSMId());
	if (dsm) site = dsm->getSite();
    }
    return site;
}

void SampleTag::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    const Site* site = 0;
    if (dsm) site = dsm->getSite();

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const std::string& aname = attr.getName();
	    const std::string& aval = attr.getValue();

	    istringstream ist(aval);

	    if (aname == "id") {
		unsigned int val;
		// If you unset the dec flag, then a leading '0' means
		// octal, and 0x means hex.
		ist.unsetf(ios::dec);
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException("sample",
		    	aname,aval);
		setSampleId(val);
		// cerr << "attr=" << aval << " id=" << val << endl;
	    }
	    else if (aname == "rate") {
		float rate;
		ist >> rate;
		if (ist.fail() || rate < 0.0 || (getRate() != 0 && getRate() != rate))
                {
                    cerr << "getRate = " << getRate() << "  rate = " << rate;
		    throw n_u::InvalidParameterException("sample",
		    	aname,aval);
                }
		setRate(rate);
	    }
	    else if (aname == "period") {
		float period;
		ist >> period;
		if (ist.fail() || period < 0.0)
		    throw n_u::InvalidParameterException("sample",
		    	aname,aval);
		setPeriod(period);
	    }
	    else if (aname == "scanfFormat")
		setScanfFormat(aval);
	    else if (aname == "process") {
		bool process;
		ist >> boolalpha >> process;
		if (ist.fail()) {
		    ist.clear();
		    ist >> noboolalpha >> process;
		    if (ist.fail())
			throw n_u::InvalidParameterException("sample",
			    aname,aval);
		}
		setProcessed(process);
		// cerr << "processed=" << process << endl;
            }
	}
    }
    unsigned int nvars = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (elname == "variable") {
	    Variable* var;
	    if (nvars == variables.size()) var = new Variable();
	    else var = variables[nvars];

	    if (site) var->setSiteAttributes(site);

	    var->fromDOMElement((xercesc::DOMElement*)child);
	    if (nvars == variables.size()) addVariable(var);
            VariableConverter* cvtr = var->getConverter();
            if (dsm && cvtr && cvtr->getCalFile())
                cvtr->getCalFile()->setDSMConfig(dsm);
	    nvars++;
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
        else if (elname == "prompt") {
            std::string prompt = xchild.getAttributeValue("string");
            setPromptString(prompt);
            istringstream ist(xchild.getAttributeValue("rate"));
            float promptrate;
            ist >> promptrate;
	    if (ist.fail() || promptrate < 0.0 || (getRate() != 0 && getRate() != promptrate))
                    throw n_u::InvalidParameterException("sample",
                        "prompt rate", xchild.getAttributeValue("rate"));
            setRate(promptrate);
        }
	else throw n_u::InvalidParameterException("sample",
		"unknown child element of sample",elname);
    }
}
