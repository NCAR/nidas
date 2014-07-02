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

#ifndef NIDAS_CORE_DATASETS_H
#define NIDAS_CORE_DATASETS_H

#include <nidas/util/InvalidParameterException.h>
#include "XMLException.h"
#include "XMLParser.h"

#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMException.hpp>

#include <list>

namespace nidas { namespace core {

/**
 * A Dataset is a named collection of parameters, that are used in
 * data processing. In addition to a name, a long-winded description,
 * a resolution in seconds, there are one or more name=value string
 * parameters which are typically set in a Unix process environment.
 */
class Dataset 
{
public:

    Dataset();

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    const std::string& getDescription() const { return _desc; }

    void setDescription(const std::string& val) { _desc = val; }

    unsigned int getResolutionSecs() const { return _resSecs; }

    void setResolutionSecs(unsigned int val) { _resSecs = val; }

    /**
     * Add an environment variable to this Dataset.  The
     * actual process environment is not effected. After doing
     * addEnvironmentVariable() one or more times, use Dataset::putenv()
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
     * environment variables in the Dataset.
     */
    void putenv() const;

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);
    
    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node) const
    		throw(xercesc::DOMException);

private:

    std::string _name;

    std::string _desc;

    unsigned int _resSecs;

    std::map<std::string,std::string> _envVars;

};

/**
 * A collection of Datasets. This collection is typically created by
 * parsing an XML file containing a <datasets> element and one or more
 * <dataset> elements.
 */
class Datasets {
public:
    Datasets();

    const Dataset& getDataset(const std::string& name) const
        throw(nidas::util::InvalidParameterException);

    std::list<Dataset> getDatasets() const;

    void addDataset(const Dataset& val)
        throw(nidas::util::InvalidParameterException);

    void removeDataset(const Dataset& val);

    void parseXML(const std::string& xmlFileName,bool verbose=true)
        throw(nidas::core::XMLException,
		nidas::util::InvalidParameterException);

    void writeXML(const std::string& xmlFileName)
        throw(nidas::core::XMLException,nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);
    
    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node) const
    		throw(xercesc::DOMException);


private:

    std::string _xmlName;

    std::map<std::string, Dataset> _datasetsByName;

};

}}	// namespace nidas namespace core

#endif
