// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include "VariableConverter.h"
#include "Variable.h"
#include "DSMSensor.h"
#include "CalFile.h"
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#include <sstream>
#include <iostream>


namespace 
{
    const int POLY_MAX_NUM_COEFS = 6;
}

VariableConverter::VariableConverter():
    _units(),
    _parameters(),
    _constParameters(),
    _variable(0),
    _calFile(0),
    _handler(0)
{}

VariableConverter::VariableConverter(const VariableConverter& x):
    DOMable(),
    _units(x._units),
    _parameters(),_constParameters(),
    _variable(x._variable),
    _calFile(0),
    _handler(0)
{
    const list<const Parameter*>& params = x.getParameters();
    list<const Parameter*>::const_iterator pi;
    for (pi = params.begin(); pi != params.end(); ++pi) {
        const Parameter* parm = *pi;
	Parameter* newp = parm->clone();
	addParameter(newp);
    }
    if (x._calFile) _calFile = new CalFile(*x._calFile);
    // Handler is explicitly not copied.
}


VariableConverter::
~VariableConverter()
{
    list<const Parameter*>::iterator pi;
    for (pi = _constParameters.begin(); pi != _constParameters.end(); ++pi)
    {
        delete *pi;
    }
    delete _calFile;
}


VariableConverter& VariableConverter::operator=(const VariableConverter& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        _units = rhs._units;
        _variable = rhs._variable;
        const list<const Parameter*>& params = rhs.getParameters();
        list<const Parameter*>::const_iterator pi;
        for (pi = params.begin(); pi != params.end(); ++pi) {
            const Parameter* parm = *pi;
            Parameter* newp = parm->clone();
            addParameter(newp);
        }
    }
    if (rhs._calFile) _calFile = new CalFile(*rhs._calFile);
    // Handler is not copied, and since this is a "new" converter
    // essentially, explicitly reset it.
    _handler = 0;
    return *this;
}

const DSMSensor* VariableConverter::getDSMSensor() const
{
    const Variable* var;
    if (!(var = getVariable())) return 0;

    const SampleTag* tag;
    if (!(tag = var->getSampleTag())) return 0;

    const DSMSensor* snsr;
    if (!(snsr = tag->getDSMSensor())) return 0;
    return snsr;
}

const DSMConfig* VariableConverter::getDSMConfig() const
{
    const DSMSensor* snsr;
    if (!(snsr = getDSMSensor())) return 0;
    return snsr->getDSMConfig();
}

/*
 * Add a parameter to my map, and list.
 */
void VariableConverter::addParameter(Parameter* val)
{
    map<string,Parameter*>::iterator pi = _parameters.find(val->getName());
    if (pi == _parameters.end()) {
        _parameters[val->getName()] = val;
	_constParameters.push_back(val);
    }
    else {
	// parameter with name exists. If the pointers aren't equal
	// delete the old parameter.
	Parameter* p = pi->second;
	if (p != val) {
	    // remove it from constParameters list
	    list<const Parameter*>::iterator cpi = _constParameters.begin();
	    for ( ; cpi != _constParameters.end(); ) {
		if (*cpi == p) cpi = _constParameters.erase(cpi);
		else ++cpi;
	    }
	    delete p;
	    pi->second = val;
	    _constParameters.push_back(val);
	}
    }
}

const Parameter* VariableConverter::getParameter(const std::string& name) const
{
    map<string,Parameter*>::const_iterator pi = _parameters.find(name);
    if (pi == _parameters.end()) return 0;
    return pi->second;
}

VariableConverter* VariableConverter::createFromString(const std::string& str)
{
    istringstream ist(str);

    string which;
    ist >> which;
    VariableConverter* converter = 0;
    if (which == "linear") converter = new Linear();
    else if (which == "poly") converter = new Polynomial();
    else throw n_u::InvalidParameterException("VariableConverter","fromString",str);

    converter->fromString(str);
    return converter;
}

VariableConverter* VariableConverter::createVariableConverter(
	XDOMElement& xchild)
{
    const string& elname = xchild.getNodeName();
    if (elname == "linear") return new Linear();
    else if (elname == "poly") return new Polynomial();
    else if (elname == "converter") {
        const string& classattr = xchild.getAttributeValue("class");
        DOMable* domable = 0;
        try {
            domable = DOMObjectFactory::createObject(classattr);
        }
        catch (const n_u::Exception& e) {
            throw n_u::InvalidParameterException(xchild.getNodeName(),
                classattr,e.what());
        }
        VariableConverter* converter =
            dynamic_cast<VariableConverter*>(domable);
        if (!converter) {
            throw n_u::InvalidParameterException(
                elname + ": " + classattr + " is not a VariableConverter");
            delete domable;
        }
        return converter;
    }
    else throw n_u::InvalidParameterException("VariableConverter","unknown type",elname);
    return 0;
}

void VariableConverter::fromDOMElement(const xercesc::DOMElement* node)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    if (attr.getName() == "units")
		setUnits(attr.getValue());
	}
    }
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (elname == "calfile") {
	    CalFile* calf = new CalFile();
            assert(getDSMSensor());
            // CalFile may need to know its DSM in order to
            // expand $DSM in a file path.
            calf->setDSMSensor(getDSMSensor());
	    calf->fromDOMElement((xercesc::DOMElement*)child);
	    setCalFile(calf);
	}
	else if (elname == "parameter") {
            const Dictionary* dict = 0;
            if (getDSMSensor()) dict = &getDSMSensor()->getDictionary();
	    Parameter* parameter =
	    Parameter::createParameter((xercesc::DOMElement*)child,dict);
	    addParameter(parameter);
	}
	else throw n_u::InvalidParameterException(xnode.getNodeName(),
		"unknown child element",elname);
    }
}


void VariableConverter::setCalFile(CalFile* val)
{
    _calFile = val;
}


void
VariableConverter::
abortCalFile(const std::string& what)
{
    WLOG(("") << _calFile->getCurrentFileName() << ": " << what);
    reset();
    delete _calFile;
    _calFile = 0;
}


void VariableConverter::readCalFile(dsm_time_t t) throw()
{
    if (!_calFile)
    {
        return;
    }
    /*
     * The algorithm is the same for all subclasses of VariableConverter:
     * read calfile records until the time of the next record succeeds the
     * given time.  At that point, parse the record for coefficients or
     * whatever kind of calibration values are needed.  Rather than require
     * each kind of converter to implement this algorithm, the subclasses
     * implement only the parsing of the record.  If the record cannot be
     * parsed, then throw an exception and reset the converter back to a
     * "null" or "invalid" calibration.  So the subclass-specific parts are
     * the extraction of information from the record, and the reset to the
     * "null" calibration.
     */
    int i = 0;
    while (t >= _calFile->nextTime().toUsecs())
    {
        try {
            n_u::UTime calTime;
            _calFile->readCF(calTime, 0, 0);
            // Wait until after readCF() in case file has not been opened
            // yet, in which case the returned filename is empty.
            if (++i == 1)
            {
                DLOG(("In ") << _calFile->getCurrentFileName()
                     << ", looking for time "
                     << n_u::UTime(t).format(true, "%Y%m%d,%H:%M:%S"));
            }
            // There is a new record to be handled, see if the handler
            // wants it, and if not, pass it on to the parseFields()
            // method.
            if (!_handler || !_handler->handleCalFileRecord(_calFile))
                parseFields(_calFile);
        }
        catch(const n_u::EOFException& e)
        {
            // Why don't we close the file here?
        }
        catch(const n_u::IOException& e)
        {
            abortCalFile(e.what());
            break;
        }
        catch(const n_u::ParseException& e)
        {
            abortCalFile(e.what());
            break;
        }
    }
}


void
VariableConverter::
setCalFileHandler(CalFileHandler* cfh)
{
    _handler = cfh;
}


Linear::Linear():
    _slope(1.0),_intercept(0.0)
{
}

Linear* Linear::clone() const
{
    return new Linear(*this);
}

void
Linear::
reset()
{
    setIntercept(floatNAN);
    setSlope(floatNAN);
}

void
Linear::
parseFields(CalFile* cf)
{
    float d[2];
    int n = cf->getFields(0, 2, d);

    // Originally this code only changed intercept or slope if it had been
    // specifically set in the calfile record.  However, that seems to
    // contradict the understanding that polynomial coefficients default to
    // an identity function unless set, ie, all coefficients are zero
    // except the slope is 1.  On the other hand, the readCF() API
    // specifically says it returns nan for any coefficients not included
    // in the record.  So what should slope be set to if not specified on
    // the command line: nan, 1.0, or previous value?

    if (n > 0) setIntercept(d[0]);
    if (n > 1) setSlope(d[1]);
}

double Linear::convert(dsm_time_t t,double val)
{
    readCalFile(t);
    return val * _slope + _intercept;
}

std::string Linear::toString() const
{
    ostringstream ost;

    ost << "linear slope=" << _slope << " intercept=" << _intercept <<
    	" units=\"" << getUnits() << "\"";
    return ost.str();
}

void Linear::fromString(const std::string& str)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which != "linear")
    	throw n_u::InvalidParameterException("linear","fromString",str);

    char cstr[256];
    float val;

    ist.getline(cstr,sizeof(cstr),'=');
    const char* cp;
    for (cp = cstr; *cp == ' '; cp++);

    ist >> val;
    if (ist.eof() || ist.fail())
    	throw n_u::InvalidParameterException("linear","fromString",str);
    if (!strcmp(cp,"slope")) setSlope(val);
    else if (!strcmp(cp,"intercept")) setIntercept(val);
    else throw n_u::InvalidParameterException("linear",cstr,str);

    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);

    ist >> val;
    if (ist.eof() || ist.fail())
    	throw n_u::InvalidParameterException("linear","fromString",str);
    if (!strcmp(cp,"slope")) setSlope(val);
    else if (!strcmp(cp,"intercept")) setIntercept(val);
    else throw n_u::InvalidParameterException("linear",cstr,str);

    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);
    if (!strcmp(cp,"units")) {
	ist.getline(cstr,sizeof(cstr),'"');
	ist.getline(cstr,sizeof(cstr),'"');
	setUnits(string(cstr));
    }
}

void Linear::fromDOMElement(const xercesc::DOMElement* node)
{

    // do base class fromDOMElement
    VariableConverter::fromDOMElement(node);

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    if (aname == "slope" || aname == "intercept") {
		istringstream ist(aval);
		float fval;
		ist >> fval;
		if (ist.fail())
		    throw n_u::InvalidParameterException("linear",aname,
		    	aval);
		if (aname == "slope") setSlope(fval);
		else if (aname == "intercept") setIntercept(fval);
	    }
	}
    }
}


Polynomial::Polynomial() :
    _coefs()
{
    float tmpcoefs[] = { 0.0, 1.0 };
    setCoefficients(vector<float>(tmpcoefs,tmpcoefs+2));
}

Polynomial* Polynomial::clone() const
{
    return new Polynomial(*this);
}

void Polynomial::setCoefficients(const std::vector<float>& vals) 
{
    setCoefficients(&(vals[0]), vals.size());
}

void Polynomial::setCoefficients(const float* fp, unsigned int n)
{
    if (_coefs.size() != n) _coefs.resize(n);

    for (unsigned int i = 0; i < n; i++) _coefs[i] = fp[i];
}


void
Polynomial::
reset()
{
    float d[POLY_MAX_NUM_COEFS];
    int n = 2;
    for (int i = 0; i < n; i++) d[i] = floatNAN;
    setCoefficients(d, n);
}


void
Polynomial::
parseFields(CalFile* cf)
{
    float d[POLY_MAX_NUM_COEFS];
    int n = 0;

    n_u::UTime calTime = cf->getCurrentTime();
    n = cf->getFields(0, POLY_MAX_NUM_COEFS, d);
    DLOG(("") << n << " coefficients read from cal file '"
         << cf->getCurrentFileName()
         << "' at time " << n_u::UTime(calTime).format(true, "%Y%m%d,%H:%M:%S"));
    DLOG(("Cal file '") << _calFile->getCurrentFileName() << "' next time: "
         << n_u::UTime(cf->nextTime()).format(true, "%Y%m%d,%H:%M:%S"));

    if (n == POLY_MAX_NUM_COEFS)
    {
        WLOG(("") << cf->getCurrentFileName()
             << ": possible overrun of coefficients at line "
             << cf->getLineNumber() << ", max allowed=" << POLY_MAX_NUM_COEFS);
    }
    // Similar to the questions in Linear::parseFields(), what happens on a
    // blank line?  Should that reset the coefficients to nans or to
    // defaults?  Unlike Linear, at least the Polynomial conversion only
    // uses as many coefficients as have been set.

    // However, if none have been set, then should the number of
    // coefficients be set to zero?  In that case, eval(x) returns zero and
    // not nan or x.  Does that should right?  I suppose if the default is
    // that all coefficients are zero, that makes sense.  But in fact the
    // constructor sets the defaults to 0, 1, so eval(x) ==> x.

    if (n > 0) setCoefficients(d, n);
}


double Polynomial::convert(dsm_time_t t,double val)
{
    readCalFile(t);
    return eval(val,&_coefs[0],_coefs.size());
}


std::string Polynomial::toString() const
{
    ostringstream ost;

    ost << "poly coefs=";
    for (size_t i = 0; i < _coefs.size(); i++)
	ost << _coefs[i] << ' ';
    ost << " units=\"" << getUnits() << "\"";
    return ost.str();
}

void Polynomial::fromString(const std::string& str)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which != "poly")
    	throw n_u::InvalidParameterException("poly","fromString",str);

    char cstr[256];
    float val;

    ist.getline(cstr,sizeof(cstr),'=');
    const char* cp;
    for (cp = cstr; *cp == ' '; cp++);

    if (ist.eof() || ist.fail())
    	throw n_u::InvalidParameterException("poly","fromString",str);
    vector<float> vals;
    if (!strcmp(cp,"coefs")) {
        for(;;) {
	    ist >> val;
	    if (ist.fail()) break;
	    vals.push_back(val);
	}
	setCoefficients(vals);
    }
    else throw n_u::InvalidParameterException("poly",cstr,str);
	    
    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);
    if (!strcmp(cp,"units")) {
	ist.getline(cstr,sizeof(cstr),'"');
	ist.getline(cstr,sizeof(cstr),'"');
	setUnits(string(cstr));
    }
}

void Polynomial::fromDOMElement(const xercesc::DOMElement* node)
{
    // do base class fromDOMElement
    VariableConverter::fromDOMElement(node);

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    vector<float> fcoefs;
	    if (aname == "coefs") {
		istringstream ist(aval);
		for (;;) {
		    float fval;
		    if (ist.eof()) break;
		    ist >> fval;
		    // cerr << "fval=" << fval << " fail=" << ist.fail() << " eof=" << ist.eof() << endl;
		    if (ist.fail())
			throw n_u::InvalidParameterException("poly",aname,
			    aval);
		    fcoefs.push_back(fval);
		}
		setCoefficients(fcoefs);
	    }
	}
    }
}
