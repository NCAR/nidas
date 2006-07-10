/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-06-11 12:37:27 -0600 (Sun, 11 Jun 2006) $

    $LastChangedRevision: 3390 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/Project.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_PROJECTCONFIGS_H
#define NIDAS_CORE_PROJECTCONFIGS_H

#include <nidas/util/UTime.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/core/XMLException.h>

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

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    const nidas::util::UTime& getBeginTime() const { return beginTime; }

    void setBeginTime(const nidas::util::UTime& val) { beginTime = val; }

    const nidas::util::UTime& getEndTime() const { return endTime; }

    void setEndTime(const nidas::util::UTime& val) { endTime = val; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);
    
private:

    std::string name;

    nidas::util::UTime beginTime;

    nidas::util::UTime endTime;

};

/**
 */
class ProjectConfigs {
public:
    ProjectConfigs();
    ~ProjectConfigs();

    void parseXML(const std::string& xmlFileName)
        throw(nidas::core::XMLException,
		nidas::util::InvalidParameterException);

    const ProjectConfig* getConfig(const nidas::util::UTime& begin) const;

    const std::list<const ProjectConfig*>& getConfigs() const
    {
        return constConfigs;
    }

    void addConfig(ProjectConfig* cfg) {
        configs.push_back(cfg);
        constConfigs.push_back(cfg);
    }

private:

    std::list<const ProjectConfig*> constConfigs;

    std::list<ProjectConfig*> configs;


};

}}	// namespace nidas namespace core

#endif
