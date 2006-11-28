/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/VariableConverter.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;
// using namespace xercesc;

namespace n_u = nidas::util;

#include <sstream>
#include <iostream>

/*
 * Add a parameter to my map, and list.
 */
void VariableConverter::addParameter(Parameter* val)
{
    map<string,Parameter*>::iterator pi = parameters.find(val->getName());
    if (pi == parameters.end()) {
        parameters[val->getName()] = val;
	constParameters.push_back(val);
    }
    else {
	// parameter with name exists. If the pointers aren't equal
	// delete the old parameter.
	Parameter* p = pi->second;
	if (p != val) {
	    // remove it from constParameters list
	    list<const Parameter*>::iterator cpi = constParameters.begin();
	    for ( ; cpi != constParameters.end(); ) {
		if (*cpi == p) cpi = constParameters.erase(cpi);
		else ++cpi;
	    }
	    delete p;
	    pi->second = val;
	    constParameters.push_back(val);
	}
    }
}

const Parameter* VariableConverter::getParameter(const std::string& name) const
{
    map<string,Parameter*>::const_iterator pi = parameters.find(name);
    if (pi == parameters.end()) return 0;
    return pi->second;
}

Linear::Linear(): calFile(0),calTime(0)
{
}

Linear::Linear(const Linear& x):calFile(0),calTime(0)
{
    if (x.calFile) calFile = new CalFile(*x.calFile);
}

Linear* Linear::clone() const
{
    return new Linear(*this);
}

Linear::~Linear()
{
    delete calFile;
}

void Linear::setCalFile(CalFile* val)
{
    calFile = val;
}

std::string Linear::toString() const
{
    ostringstream ost;

    ost << "linear slope=" << slope << " intercept=" << intercept <<
    	" units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

Polynomial::Polynomial() : coefs(0),calFile(0),calTime(0)
{
}

/*
 * Copy constructor.
 */
Polynomial::Polynomial(const Polynomial& x):
	VariableConverter(x),
	coefvec(),coefs(0),ncoefs(0),calFile(0)
{
    setCoefficients(x.getCoefficients());
    if (x.calFile) calFile = new CalFile(*x.calFile);
}

Polynomial* Polynomial::clone() const
{
    return new Polynomial(*this);
}

Polynomial::~Polynomial()
{
    delete [] coefs; delete calFile;
}

void Polynomial::setCalFile(CalFile* val)
{
    calFile = val;
}

std::string Polynomial::toString() const
{
    ostringstream ost;

    ost << "poly coefs=";
    for (unsigned int i = 0; i < coefvec.size(); i++)
	ost << coefvec[i] << ' ';
    ost << " units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

VariableConverter* VariableConverter::createFromString(const std::string& str)
	throw(n_u::InvalidParameterException)
{
    istringstream ist(str);

    string which;
    ist >> which;
    VariableConverter* converter = 0;
    if (!which.compare("linear")) converter = new Linear();
    else if (!which.compare("poly")) converter = new Polynomial();
    else throw n_u::InvalidParameterException("VariableConverter","fromString",str);

    converter->fromString(str);
    return converter;
}

void Linear::fromString(const std::string& str)
	throw(n_u::InvalidParameterException)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which.compare("linear"))
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

void Polynomial::fromString(const std::string& str)
	throw(n_u::InvalidParameterException)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which.compare("poly"))
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
    return 0;
}

void VariableConverter::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    if (!attr.getName().compare("units"))
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
	    calf->fromDOMElement((xercesc::DOMElement*)child);
	    setCalFile(calf);
	}
	else if (elname == "parameter") {
	    Parameter* parameter =
	    Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
	else throw n_u::InvalidParameterException(xnode.getNodeName(),
		"unknown child element",elname);
    }
}

xercesc::DOMElement* VariableConverter::toDOMParent(
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

xercesc::DOMElement* VariableConverter::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

void Linear::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
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
	    if (!aname.compare("slope") || !aname.compare("intercept")) {
		istringstream ist(aval);
		float fval;
		ist >> fval;
		if (ist.fail())
		    throw n_u::InvalidParameterException("linear",aname,
		    	aval);
		if (!aname.compare("slope")) setSlope(fval);
		else if (!aname.compare("intercept")) setIntercept(fval);
	    }
	}
    }
}

void Polynomial::setCoefficients(const vector<float>& vals) 
{
    coefvec = vals;
    delete [] coefs;
    ncoefs = vals.size();
    if (ncoefs < 2) ncoefs = 2;
    coefs = new float[ncoefs];
    coefs[0] = 0.0;
    coefs[1] = 1.0;
    for (unsigned int i = 0; i < vals.size(); i++) coefs[i] = vals[i];
}

void Polynomial::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
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
	    if (!aname.compare("coefs")) {
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


float Linear::convert(dsm_time_t t,float val)
{
    if (calFile) {
        while(t >= calTime) {
            float d[2];
            try {
                int n = calFile->readData(d,sizeof d/sizeof(d[0]));
                if (n == 2) {
                    setIntercept(d[0]);
                    setSlope(d[1]);
                }
                calTime = calFile->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e)
            {
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                setIntercept(floatNAN);
                setSlope(floatNAN);
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                setIntercept(floatNAN);
                setSlope(floatNAN);
                calTime = LONG_LONG_MAX;
            }
        }
    }
    return val * slope + intercept;
}

float Polynomial::convert(dsm_time_t t,float val)
{
    if (calFile && t > calTime) {
        auto_ptr<float> d(new float[ncoefs]);
        size_t n = 0;
        while(t >= calTime) {
            try {
                n = calFile->readData(d.get(),ncoefs);
                calTime = calFile->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e) {}
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                for (unsigned int i = 0; i < ncoefs; i++) d.get()[i] = floatNAN;
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                for (unsigned int i = 0; i < ncoefs; i++) d.get()[i] = floatNAN;
                calTime = LONG_LONG_MAX;
            }
        }
        if (n == ncoefs) {
            vector<float> vals(d.get(),d.get()+ncoefs);
            setCoefficients(vals);
        }
    }
    double result = 0.0;
    for (int i = ncoefs - 1; i > 0; i--) {
        result += coefs[i];
        result *= val;
    }
    result += coefs[0];
    return result;
}

