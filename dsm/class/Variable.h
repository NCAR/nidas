/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/
#ifndef VARIABLE_H
#define VARIABLE_H

#include <DOMable.h>

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
    Variable() {}

    virtual ~Variable() {}

    virtual void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    virtual void setLongName(const std::string& val) { longname = val; }

    const std::string& getLongName() const { return longname; }

    virtual void setUnits(const std::string& val) { units = val; }

    const std::string& getUnits() const { return units; }

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
};

}

#endif
