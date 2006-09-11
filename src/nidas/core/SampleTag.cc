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

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleTag::SampleTag():
	id(0),sampleId(0),sensorId(0),rate(0.0),processed(true),dsm(0) {}

/* copy constructor */
SampleTag::SampleTag(const SampleTag& x):
	id(x.id),sampleId(x.sampleId),sensorId(x.sensorId),
	suffix(x.suffix),
	station(x.station),
	rate(x.rate),processed(x.processed),
	dsm(x.dsm),
	scanfFormat(x.scanfFormat)
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

void SampleTag::setStation(int val)
{
    station = val;
    for (vector<Variable*>::const_iterator vi = variables.begin();
    	vi != variables.end(); ++vi) {
	Variable* var = *vi;
	var->setStation(station);
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

    if (getStation() >= 0) {
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
		unsigned long val;
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
		if (ist.fail() || rate < 0.0)
		    throw n_u::InvalidParameterException("sample",
		    	aname,aval);
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

	    if (site) var->setSiteSuffix(site->getSuffix());

	    var->fromDOMElement((xercesc::DOMElement*)child);
	    if (nvars == variables.size()) addVariable(var);
	    nvars++;
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
	else throw n_u::InvalidParameterException("sample",
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

string SampleTag::expandString(const string& input) const
{
    string::size_type lastpos = 0;
    string::size_type dollar;

    string result;

    while ((dollar = input.find('$',lastpos)) != string::npos) {

        result.append(input.substr(lastpos,dollar-lastpos));
	lastpos = dollar;

	string::size_type openparen = input.find('{',dollar);
	if (openparen != dollar + 1) break;

	string::size_type closeparen = input.find('}',openparen);
	if (closeparen == string::npos) break;

	string token = input.substr(openparen+1,closeparen-openparen-1);
	if (token.length() > 0) {
	    string val = getTokenValue(token);
	    // cerr << "getTokenValue: token=" << token << " val=" << val << endl;
	    result.append(val);
	}
	lastpos = closeparen + 1;
    }

    result.append(input.substr(lastpos));
    // cerr << "input: \"" << input << "\" expanded to \"" <<
    // 	result << "\"" << endl;
    return result;
}

string SampleTag::getTokenValue(const string& token) const
{
    if (token == "PROJECT") return Project::getInstance()->getName();

    if (token == "SYSTEM") return Project::getInstance()->getSystemName();

    if (token == "AIRCRAFT" || token == "SITE") {
	if (dsm) return dsm->getSite()->getName();
	else return "unknown";
    }
        
    if (token == "DSM") {
	if (dsm) return dsm->getName();
	else return "unknown";
    }
        
    if (token == "LOCATION") {
	if (dsm) return dsm->getLocation();
	else return "unknown";
    }

    // if none of the above, try to get token value from UNIX environment
    const char* val = ::getenv(token.c_str());
    if (val) return string(val);
    else return "unknown";
}

