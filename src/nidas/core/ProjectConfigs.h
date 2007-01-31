/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_PROJECTCONFIGS_H
#define NIDAS_CORE_PROJECTCONFIGS_H

#include <nidas/util/UTime.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/core/XMLException.h>
#include <nidas/core/Project.h>

// #include <xercesc/dom/DOMDocument.hpp>
// #include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>
// #include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <list>

namespace nidas { namespace core {

class ProjectConfig 
{
public:

    ProjectConfig();

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    const std::string& getXMLName() const { return xmlName; }

    void setXMLName(const std::string& val) { xmlName = val; }

    const nidas::util::UTime& getBeginTime() const { return beginTime; }

    void setBeginTime(const nidas::util::UTime& val) { beginTime = val; }

    const nidas::util::UTime& getEndTime() const { return endTime; }

    void setEndTime(const nidas::util::UTime& val) { endTime = val; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);
    
    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node) const
    		throw(xercesc::DOMException);

private:

    std::string name;

    std::string xmlName;

    nidas::util::UTime beginTime;

    nidas::util::UTime endTime;

};

/**
 * Sample time tag comparator.
 */
class ProjectConfigTimeComparator {
public:
    /**
     * Return true if x is less than y.
     */
    inline bool operator() (const ProjectConfig* x, const ProjectConfig *y)
        const {
        return x->getBeginTime() < y->getEndTime();
    }
};

/**
 */
class ProjectConfigs {
public:
    ProjectConfigs();
    ~ProjectConfigs();

    const ProjectConfig* getConfig(const nidas::util::UTime& begin) const;

    const std::list<const ProjectConfig*>& getConfigs() const;

    void addConfig(ProjectConfig* val)
        throw(nidas::util::InvalidParameterException);

    void removeConfig(const ProjectConfig* val);

    /**
     * Convienence routine for getting a Project*, given
     * a ProjectsConfig XML file name, and a time.
     */
    static Project* getProject(const std::string& xmlFileName,
        const nidas::util::UTime& begin)
        throw(nidas::core::XMLException,
		nidas::util::InvalidParameterException);

    void parseXML(const std::string& xmlFileName)
        throw(nidas::core::XMLException,
		nidas::util::InvalidParameterException);

    void writeXML(const std::string& xmlFileName)
        throw(XMLException,nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);
    
    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node) const
    		throw(xercesc::DOMException);


private:

    std::list<const ProjectConfig*> constConfigs;

    std::list<ProjectConfig*> configs;

};

}}	// namespace nidas namespace core

#endif
