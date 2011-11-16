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

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/Variable.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Parameter.h>

#include <nidas/util/Logger.h>

#include <sstream>
#include <limits>
#include <algorithm>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

Variable::Variable(): _sampleTag(0),
    _name(),_nameWithoutSite(),_prefix(),_suffix(),_siteSuffix(),
    _station(-1),_longname(),
    _A2dChannel(-1),_units(),
    _type(CONTINUOUS),
    _length(1),
    _converter(0),_parameters(),_constParameters(),
    _missingValue(1.e37),
    _minValue(-numeric_limits<float>::max()),
    _maxValue(numeric_limits<float>::max()),
    _dynamic(false)
{
    _plotRange[0] = floatNAN;
    _plotRange[1] = floatNAN;
}

/* copy constructor */
Variable::Variable(const Variable& x):
    _sampleTag(0),
    _name(x._name),
    _nameWithoutSite(x._nameWithoutSite),
    _prefix(x._prefix),
    _suffix(x._suffix),
    _siteSuffix(x._siteSuffix),
    _station(x._station),
    _longname(x._longname),
    _A2dChannel(x._A2dChannel),
    _units(x._units),
    _type(x._type),
    _length(x._length),
    _converter(0),_parameters(),_constParameters(),
    _missingValue(x._missingValue),
    _minValue(x._minValue),
    _maxValue(x._maxValue),
    _dynamic(x._dynamic)
{
    if (x._converter) _converter = x._converter->clone();
    const list<const Parameter*>& params = x.getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* parm = *pi;
	Parameter* newp = parm->clone();
	addParameter(newp);
    }
    _plotRange[0] = x._plotRange[0];
    _plotRange[1] = x._plotRange[1];
}

/* assignment */
Variable& Variable::operator=(const Variable& x)
{
    // do not assign sampleTag
    if (this != &x) {
        _name = x._name;
        _nameWithoutSite = x._nameWithoutSite;
        _prefix = x._prefix;
        _suffix  = x._suffix;
        _siteSuffix  = x._siteSuffix;
        _station = x._station;
        _longname = x._longname;
        _A2dChannel = x._A2dChannel;
        _units = x._units;
        _type = x._type;
        _length = x._length;
        _missingValue = x._missingValue;
        _minValue = x._minValue;
        _maxValue = x._maxValue;
        _plotRange[0] = x._plotRange[0];
        _plotRange[1] = x._plotRange[1];
        _dynamic = x._dynamic;

        // this invalidates the previous pointer to the converter, hmm.
        // don't want to create a virtual assignment op for converters.
        if (x._converter) {
            delete _converter;
            _converter = x._converter->clone();
        }

        // If a Parameter from x matches in type and name,
        // assign our Parameter to it, otherwise add it.
        // We're trying to keep the pointers to Parameters valid
        // (avoiding deleting and cloning), in case someone
        // has done a getParameters() on this variable.
        const list<const Parameter*>& xparams = x.getParameters();
        list<const Parameter*>::const_iterator xpi;
        for (xpi = xparams.begin(); xpi != xparams.end(); ++xpi) {
            const Parameter* xparm = *xpi;
            ParameterNameTypeComparator comp(xparm);
            list<Parameter*>::iterator pi =
                    std::find_if(_parameters.begin(),_parameters.end(),comp);
            if (pi != _parameters.end()) (*pi)->assign(*xparm);
            else addParameter(xparm->clone());
        }
    }
    return *this;
}

Variable::~Variable()
{
    delete _converter;
    list<Parameter*>::const_iterator pi;
    for (pi = _parameters.begin(); pi != _parameters.end(); ++pi)
    	delete *pi;
}

void Variable::setSiteSuffix(const string& val)
{
    // don't repeat site suffix, in case
    // user has only set full name with setName().
    if (_siteSuffix.length() == 0 && _suffix.length() == 0) {
	unsigned nl = _name.length();
	unsigned vl = val.length();
	if (vl > 0 && nl > vl && _name.substr(nl-vl,vl) == val)
	    _prefix = _name.substr(0,nl-vl);
    }
    _siteSuffix = val;
    _name = _prefix + _suffix + _siteSuffix;
}

void Variable::setSiteAttributes(const Site* site)
{
    if (!site) return;
    _station = site->getNumber();
    if (_station == 0) setSiteSuffix(site->getSuffix());
    else setSiteSuffix("");
}

const Site* Variable::getSite() const
{
    const Site* site = 0;
    if (getStation() > 0)
	site = Project::getInstance()->findSite(getStation());
    if (!site) {
        const SampleTag* stag = getSampleTag();
	if (stag) site = stag->getSite();
    }
    return site;
}

bool Variable::operator == (const Variable& x) const
{
    if (getLength() != x.getLength()) return false;

    bool stnMatch = _station == x._station ||
	_station == -1 || x._station == -1;
    if (!stnMatch) return stnMatch;
    if (_name == x._name) return true;
    if (_station < 0 && x._station == 0)
        return _name == x._nameWithoutSite;
    if (x._station < 0 && _station == 0)
        return x._name == _nameWithoutSite;
    return false;
}

bool Variable::operator != (const Variable& x) const
{
    return !operator == (x);
}

bool Variable::operator < (const Variable& x) const
{
    bool stnMatch = _station == x._station ||
	_station == -1 || x._station == -1;
    if (stnMatch) return getName().compare(x.getName()) < 0;
    else return _station < x._station;
}

float Variable::getSampleRate() const {
    if (!_sampleTag) return 0.0;
    else return _sampleTag->getRate();
}

const Parameter* Variable::getParameter(const std::string& name) const
{
    std::list<const Parameter*>::const_iterator pi;
    for (pi = _constParameters.begin(); pi != _constParameters.end(); ++pi)
      if ((*pi)->getName().compare(name) == 0)
        return *pi;
    return 0;
}

void Variable::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            const string& aname = attr.getName();
            string aval = Project::getInstance()->expandString(attr.getValue());
	    // get attribute name
	    if (aname == "name")
	    	setPrefix(aval);
	    else if (aname == "longname")
	    	setLongName(aval);
	    else if (aname == "units")
		setUnits(aval);
	    else if (aname == "length") {
	        istringstream ist(aval);
		unsigned int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("variable ") + getName(),aname,aval);
		setLength(val);
	    }
	    else if (aname == "missingValue" ||
                    aname == "minValue" ||
                    aname == "maxValue") {
	        istringstream ist(aval);
		float val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	string("variable") + getName(),aname,aval);
                string sname = aname.substr(0,3);
                if (sname == "mis") setMissingValue(val);
                else if (sname == "min") setMinValue(val);
                else if (sname == "max") setMaxValue(val);
	    }
	    else if (aname == "count") {
                if (aval == "true")
                    setType(Variable::COUNTER);
            }
	    else if (aname == "plotrange") {
                // environment variables are expanded above.
                std::istringstream ist(aval);
                float prange[2] = { -10.0,10.0 };
                // if plotrange value starts with '$' ignore error.
                if (aval.length() < 1 || aval[0] != '$') {
		    int i;
                    for (i = 0; i < 2 ; i++) {
                        if (ist.eof()) break;
                        ist >> prange[i];
                        if (ist.fail()) break;
                    }
                    // Don't throw exception on poorly formatted plotranges
                    if (i < 2)  {
                        n_u::InvalidParameterException e(string("variable ") + getName(),aname,aval);
                        WLOG(("%s",e.what()));
                    }
                }
                setPlotRange(prange[0],prange[1]);
            }
	    else if (aname == "dynamic") {
                istringstream ist(aval);
		bool val;
		ist >> boolalpha >> val;
		if (ist.fail()) {
		    ist.clear();
		    ist >> noboolalpha >> val;
		    if (ist.fail())
			throw n_u::InvalidParameterException(
                            string("variable ") + getName(),aname,aval);
		}
		setDynamic(val);
	    }
	}
    }

    int nconverters = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
	else if (elname == "linear" || elname == "poly" ||
            elname == "converter") {
	    if (nconverters > 0)
	    	throw n_u::InvalidParameterException(getName(),
		    "only one child converter allowed, <linear>, <poly> etc",
		    	elname);
	    VariableConverter* cvtr =
	    	VariableConverter::createVariableConverter(xchild);
	    if (!cvtr) throw n_u::InvalidParameterException(getName(),
		    "unsupported child element",elname);
            cvtr->setVariable(this);
            cvtr->setUnits(getUnits());
	    cvtr->fromDOMElement((xercesc::DOMElement*)child);
	    setConverter(cvtr);
	    nconverters++;
	}
    }
}

xercesc::DOMElement* Variable::toDOMParent(xercesc::DOMElement* parent,bool complete) const
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("variable"));
    parent->appendChild(elem);
    return toDOMElement(elem,complete);
}

xercesc::DOMElement* Variable::toDOMElement(xercesc::DOMElement* elem,bool complete) const
    throw(xercesc::DOMException)
{
    if (complete) return 0; // not supported yet

    XDOMElement xelem(elem);

    string units = getUnits();
    VariableConverter* cvtr = getConverter();
    if (cvtr) units = cvtr->getUnits();

    xelem.setAttributeValue("name",getName());
    if (getLongName().length() > 0)
        xelem.setAttributeValue("longname",getLongName());
    if (units.length() > 0)
        xelem.setAttributeValue("units",units);
    if (getLength() > 1) {
        ostringstream ost;
        ost << getLength();
        xelem.setAttributeValue("length",ost.str());
    }
    if (isDynamic()) {
        xelem.setAttributeValue("dynamic","true");
    }

    float pmin = _plotRange[0];
    if (isnan(pmin)) pmin = -10.0;
    float pmax = _plotRange[1];
    if (isnan(pmax)) pmax = 10.0;
    ostringstream ost;
    ost << pmin << ' ' << pmax;
    xelem.setAttributeValue("plotrange",ost.str());
    return elem;
}
