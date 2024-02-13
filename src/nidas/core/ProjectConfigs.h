// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_PROJECTCONFIGS_H
#define NIDAS_CORE_PROJECTCONFIGS_H

#include <nidas/util/UTime.h>
#include <nidas/util/InvalidParameterException.h>
#include "XMLException.h"
#include "XMLParser.h"
#include "Project.h"

// #include <xercesc/dom/DOMDocument.hpp>
// #include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>
// #include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <list>

namespace nidas { namespace core {

class Project;

class ProjectConfig 
{
public:

    ProjectConfig();

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getXMLName() const { return _xmlName; }

    void setXMLName(const std::string& val) { _xmlName = val; }

    /**
     * Returns UTime::MIN if unset.
     */
    const nidas::util::UTime& getBeginTime() const { return _beginTime; }

    void setBeginTime(const nidas::util::UTime& val) { _beginTime = val; }

    /**
     * Returns UTime::MAX if unset.
     */
    const nidas::util::UTime& getEndTime() const { return _endTime; }

    void setEndTime(const nidas::util::UTime& val) { _endTime = val; }

    /**
     * Add an environment variable to this ProjectConfig.  The
     * actual process environment is not effected. After doing
     * addEnvironmentVariable() one or more times, use ProjectConfig::putenv()
     * to update the actual process environment. Set value to an
     * empty string to remove a variable from the environment.
     */
    void addEnvironmentVariable(const std::string& name, const std::string& value);

    std::map<std::string,std::string> getEnvironmentVariables() const
    {
        return _envVars;
    }

    /**
     * Update the process environment with the current list of
     * environment variables in the ProjectConfig.
     */
    void putenv() const;

    /**
     * @param project: the project, by reference.
     *
     * @throws nidas::core::XMLException
     * @throws nidas::util::InvalidParameterException
     **/
    void initProject(Project& project) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMParent(xercesc::DOMElement* parent) const;

    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMElement(xercesc::DOMElement* node) const;

private:

    std::string _name;

    std::string _xmlName;

    nidas::util::UTime _beginTime;

    nidas::util::UTime _endTime;

    std::map<std::string,std::string> _envVars;

};

/**
 * Sample time tag comparator.
 */
class ProjectConfigTimeComparator {
public:
    /**
     * Return true if x is less than y.
     */
    bool operator() (const ProjectConfig* x, const ProjectConfig *y)
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

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    const ProjectConfig* getConfig(const nidas::util::UTime& begin) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    const ProjectConfig* getConfig(const std::string& name) const;

    const std::list<const ProjectConfig*>& getConfigs() const;

    /**
     * Add a project configuration, inserting it
     * in the list, which should be sorted by time.
     * If the beginning time is equal to the beginning time
     * of an existing project configuration, and the end time is
     * prior to or equal to the beginning time of the next
     * configuration, the existing configuration is replaced.
     * Exceptions are thrown if project configuration times
     * are overlapping.
     *
     * @throws nidas::util::InvalidParameterException
     **/
    void addConfigByTime(ProjectConfig* val);

    /**
     * Add a project configuration, replacing an existing
     * configuration if the name matches, otherwise appending
     * the configuration on the list.
     */
    void addConfigByName(ProjectConfig* val)
        throw();

    void removeConfig(const ProjectConfig* val);

    /**
     * @throws nidas::core::XMLException
     * @throws nidas::util::InvalidParameterException
     **/
    void parseXML(const std::string& xmlFileName);

    /**
     * @throws nidas::core::XMLException
     * @throws nidas::util::IOException
     **/
    void writeXML(const std::string& xmlFileName);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);
    
    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMParent(xercesc::DOMElement* parent) const;

    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMElement(xercesc::DOMElement* node) const;

private:

    std::string _xmlName;

    std::list<const ProjectConfig*> _constConfigs;

    std::list<ProjectConfig*> _configs;

};

}}	// namespace nidas namespace core

#endif
