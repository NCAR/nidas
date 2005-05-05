/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <VariableConverter.h>

#include <iomanip>

using namespace dsm;
using namespace std;
using namespace xercesc;

#include <sstream>

std::string Linear::toString() const
{
    ostringstream ost;

    ost << "linear slope=" << slope << " intercept=" << intercept <<
    	" units=\"" << getUnits() << "\"" << endl;
    return ost.str();
}

/*
 * Copy constructor.
 */
Polynomial::Polynomial(const Polynomial& x):
	VariableConverter(x),
	coefvec(),coefs(0),ncoefs(0)
{
    setCoefficients(x.getCoefficients());
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
	throw(atdUtil::InvalidParameterException)
{
    istringstream ist(str);

    string which;
    ist >> which;
    VariableConverter* converter = 0;
    if (!which.compare("linear")) converter = new Linear();
    else if (!which.compare("poly")) converter = new Polynomial();
    else throw atdUtil::InvalidParameterException("VariableConverter","fromString",str);

    converter->fromString(str);
    return converter;
}

void Linear::fromString(const std::string& str)
	throw(atdUtil::InvalidParameterException)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which.compare("linear"))
    	throw atdUtil::InvalidParameterException("linear","fromString",str);

    char cstr[256];
    float val;

    ist.getline(cstr,sizeof(cstr),'=');
    const char* cp;
    for (cp = cstr; *cp == ' '; cp++);

    ist >> val;
    if (ist.eof() || ist.fail())
    	throw atdUtil::InvalidParameterException("linear","fromString",str);
    if (!strcmp(cp,"slope")) setSlope(val);
    else if (!strcmp(cp,"intercept")) setIntercept(val);
    else throw atdUtil::InvalidParameterException("linear",cstr,str);

    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);

    ist >> val;
    if (ist.eof() || ist.fail())
    	throw atdUtil::InvalidParameterException("linear","fromString",str);
    if (!strcmp(cp,"slope")) setSlope(val);
    else if (!strcmp(cp,"intercept")) setIntercept(val);
    else throw atdUtil::InvalidParameterException("linear",cstr,str);

    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);
    if (!strcmp(cp,"units")) {
	ist.getline(cstr,sizeof(cstr),'"');
	ist.getline(cstr,sizeof(cstr),'"');
	setUnits(string(cstr));
    }
}

void Polynomial::fromString(const std::string& str)
	throw(atdUtil::InvalidParameterException)
{
    istringstream ist(str);
    string which;
    ist >> which;
    if (ist.eof() || ist.fail() || which.compare("poly"))
    	throw atdUtil::InvalidParameterException("poly","fromString",str);

    char cstr[256];
    float val;

    ist.getline(cstr,sizeof(cstr),'=');
    const char* cp;
    for (cp = cstr; *cp == ' '; cp++);

    if (ist.eof() || ist.fail())
    	throw atdUtil::InvalidParameterException("poly","fromString",str);
    vector<float> vals;
    if (!strcmp(cp,"coefs")) {
        for(;;) {
	    ist >> val;
	    if (ist.fail()) break;
	    vals.push_back(val);
	}
	setCoefficients(vals);
    }
    else throw atdUtil::InvalidParameterException("poly",cstr,str);
	    
    ist.getline(cstr,sizeof(cstr),'=');
    for (cp = cstr; *cp == ' '; cp++);
    if (!strcmp(cp,"units")) {
	ist.getline(cstr,sizeof(cstr),'"');
	ist.getline(cstr,sizeof(cstr),'"');
	setUnits(string(cstr));
    }
}

VariableConverter* VariableConverter::createVariableConverter(
	const std::string& elname)
{
    if (!elname.compare("linear")) return new Linear();
    else if (!elname.compare("poly")) return new Polynomial();
    return 0;
}

void VariableConverter::fromDOMElement(const DOMElement* node)
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
	    if (!attr.getName().compare("units"))
		setUnits(attr.getValue());
	}
    }
}

DOMElement* VariableConverter::toDOMParent(
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

DOMElement* VariableConverter::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

void Linear::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    // do base class fromDOMElement
    VariableConverter::fromDOMElement(node);

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    if (!aname.compare("slope") || !aname.compare("intercept")) {
		istringstream ist(aval);
		float fval;
		ist >> fval;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException("linear",aname,
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

void Polynomial::fromDOMElement(const DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{
    // do base class fromDOMElement
    VariableConverter::fromDOMElement(node);

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    const string& aname = attr.getName();
	    const string& aval = attr.getValue();
	    vector<float> fcoefs;
	    if (!aname.compare("coefs")) {
		istringstream ist(aval);
		for (;;) {
		    float fval;
		    ist >> fval;
		    if (ist.eof()) break;
		    if (ist.fail())
			throw atdUtil::InvalidParameterException("poly",aname,
			    aval);
		    fcoefs.push_back(fval);
		}
		setCoefficients(fcoefs);
	    }
	}
    }
}

