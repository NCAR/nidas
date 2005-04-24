/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef DSM_VARIABLE_H
#define DSM_VARIABLE_H

#include <DOMable.h>
#include <VariableConverter.h>
#include <atdUtil/InvalidParameterException.h>

#include <string>

namespace dsm {
/**
 * Class describing a sampled variable.
 */
class Variable : public DOMable
{

public:

    /**
     * Create a variable.
     */
    Variable();

    virtual ~Variable();

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    void setLongName(const std::string& val) { longname = val; }

    const std::string& getLongName() const { return longname; }

    void setUnits(const std::string& val) { units = val; }

    const std::string& getUnits() const { return units; }

    const VariableConverter* getConverter() const { return converter; }

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    std::string name;

    std::string longname;

    std::string units;

    VariableConverter *converter;

};

}

#endif
