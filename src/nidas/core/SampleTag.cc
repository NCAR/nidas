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
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Variable.h>

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleTag::SampleTag():
	_id(0),_sampleId(0),_sensorId(0),_station(0),
        _rate(0.0),_processed(true),_dsm(0),_sensor(0) {}

/* copy constructor */
SampleTag::SampleTag(const SampleTag& x):
	_id(x._id),_sampleId(x._sampleId),_sensorId(x._sensorId),
	_suffix(x._suffix),
	_station(x._station),
	_rate(x._rate),_processed(x._processed),
	_dsm(x._dsm),
	_sensor(x._sensor),
	_scanfFormat(x._scanfFormat),
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
    for (vector<Variable*>::const_iterator vi = _variables.begin();
    	vi != _variables.end(); ++vi) delete *vi;

    list<Parameter*>::const_iterator pi;
    for (pi = _parameters.begin(); pi != _parameters.end(); ++pi)
    	delete *pi;
}

void SampleTag::addVariable(Variable* var)
	throw(n_u::InvalidParameterException)
{
    _variables.push_back(var);
    _constVariables.push_back(var);
    var->setSampleTag(this);
}

void SampleTag::setSuffix(const std::string& val)
{
    _suffix = val;
    for (vector<Variable*>::const_iterator vi = _variables.begin();
    	vi != _variables.end(); ++vi) {
	Variable* var = *vi;
	var->setSuffix(_suffix);
    }

}

void SampleTag::setSiteAttributes(const Site* site)
{
    _station = site->getNumber();
    for (vector<Variable*>::const_iterator vi = _variables.begin();
    	vi != _variables.end(); ++vi) {
	Variable* var = *vi;
	var->setSiteAttributes(site);
    }
}

const std::vector<const Variable*>& SampleTag::getVariables() const
{
    return _constVariables;
}

VariableIterator SampleTag::getVariableIterator() const
{
    return VariableIterator(this);
}

unsigned int SampleTag::getDataIndex(const Variable* var) const
{
    unsigned int i = 0;
    std::vector<const Variable*>::const_iterator vi = _constVariables.begin();
    for ( ; vi != _constVariables.end(); ++vi) {
        if (*vi == var) return i;
        i += (*vi)->getLength();
    }
    return UINT_MAX;
}

const Parameter* SampleTag::getParameter(const string& name) const
{
    list<const Parameter*>::const_iterator pi;
    for (pi = _constParameters.begin(); pi != _constParameters.end(); ++pi) {
        const Parameter* param = *pi;
    	if (param->getName() == name) return param;
    }
    return 0;
}

const Site* SampleTag::getSite() const 
{
    const Site* site = 0;
    const DSMConfig* dsm = getDSMConfig();
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
    if (_dsm) site = _dsm->getSite();

    string suffix;

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
		if (ist.fail() || rate < 0.0)            
                {
                    ostringstream ost;
                    ost << "sample id=" << getDSMId() << ',' << getSpSId();
		    throw n_u::InvalidParameterException(ost.str(),
		    	aname,aval);
                }
		setRate(rate);
	    }
	    else if (aname == "period") {
		float period;
		ist >> period;
		if (ist.fail() || period < 0.0) {
                    ostringstream ost;
                    ost << "sample id=" << GET_DSM_ID(getId()) << ',' << GET_SPS_ID(getId());
		    throw n_u::InvalidParameterException(ost.str(),
		    	aname,aval);
                }
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
		    if (ist.fail()) {
                        ostringstream ost;
                        ost << "sample id=" << GET_DSM_ID(getId()) << ',' << GET_SPS_ID(getId());
			throw n_u::InvalidParameterException(ost.str(),
			    aname,aval);
                    }
		}
		setProcessed(process);
		// cerr << "processed=" << process << endl;
            }
	    else if (aname == "suffix")
	    	suffix = aval;
            else {
                ostringstream ost;
                ost << "sample id=" << getDSMId() << ',' << getSpSId();
                throw n_u::InvalidParameterException(ost.str(),
		    	"unknown attribute",aname);
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
	    if (nvars == _variables.size()) var = new Variable();
	    else var = _variables[nvars];

	    if (site) var->setSiteAttributes(site);

            var->setSampleTag(this);

	    if (nvars == _variables.size()) addVariable(var);

	    var->fromDOMElement((xercesc::DOMElement*)child);

            VariableConverter* cvtr = var->getConverter();
            if (_sensor && cvtr && cvtr->getCalFile())
                cvtr->getCalFile()->setDSMSensor(_sensor);
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
	    if (ist.fail() || promptrate < 0.0 || (getRate() != 0 && getRate() != promptrate)) {
                ostringstream ost;
                ost << "sample id=" << GET_DSM_ID(getId()) << ',' << GET_SPS_ID(getId());
                throw n_u::InvalidParameterException(ost.str(),
                    "prompt rate", xchild.getAttributeValue("rate"));
            }
            setRate(promptrate);
        }
	else {
            ostringstream ost;
            ost << "sample id=" << GET_DSM_ID(getId()) << ',' << GET_SPS_ID(getId());
            throw n_u::InvalidParameterException(ost.str(),
		"unknown child element of sample",elname);
        }
    }
    if (suffix.length() > 0) setSuffix(suffix);
}

xercesc::DOMElement* SampleTag::toDOMParent(xercesc::DOMElement* parent,bool complete) const
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("sample"));
    parent->appendChild(elem);
    return toDOMElement(elem,complete);
}

xercesc::DOMElement* SampleTag::toDOMElement(xercesc::DOMElement* elem,bool complete) const
    throw(xercesc::DOMException)
{
    if (complete) return 0; // not supported yet

    dsm_sample_id_t id = getId();
    ostringstream ost;
    ost << id;
    XDOMElement xelem(elem);
    xelem.setAttributeValue("id",ost.str());
    for (VariableIterator vi = getVariableIterator(); vi.hasNext(); ) {
        const Variable* var = vi.next();
        var->toDOMParent(elem,complete);
    }
    return elem;
}
