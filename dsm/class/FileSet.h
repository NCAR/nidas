/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_FILESET_H
#define DSM_FILESET_H

#include <IOChannel.h>
#include <ConnectionRequester.h>
#include <dsm_sample.h>

#include <atdUtil/FileSet.h>

#include <iostream>

namespace dsm {

/**
 * Implementation of an IOChannel from a atdUtil::FileSet
 */
class FileSet: public IOChannel, public atdUtil::FileSet {

public:

    FileSet():IOChannel(),atdUtil::FileSet() {}

    virtual ~FileSet() {}

    const std::string& getName() const;

    void setDir(const std::string& val);

    void setFileName(const std::string& val);

    std::string FileSet::expandString(const std::string& input);

    std::string FileSet::getTokenValue(const std::string& name);

    void requestConnection(ConnectionRequester* requester,int pseudoPort)
    	throw(atdUtil::IOException);

    IOChannel* clone() { return new FileSet(*this); }

    dsm_sys_time_t FileSet::createFile(dsm_sys_time_t t)
	throw(atdUtil::IOException);

    size_t read(void* buf, size_t len) throw(atdUtil::IOException)
    {
        return atdUtil::FileSet::read(buf,len);
    }
        
    size_t write(const void* buf, size_t len) throw(atdUtil::IOException)
    {
#ifdef DEBUG
	std::cerr << getName() << " write, len=" << len << std::endl;
#endif
        return atdUtil::FileSet::write(buf,len);
    }
        
    void close() throw(atdUtil::IOException)
    {
        atdUtil::FileSet::closeFile();
    }
        
    int getFd() const { return atdUtil::FileSet::getFd(); }
        
    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);
    
protected:

    /**
     * Recognizeable name of this IOChannel - used for informative
     * messages.
     */
    void setName(const std::string& val);

    std::string name;

};

}

#endif
