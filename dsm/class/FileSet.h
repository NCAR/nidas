/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_FILESET_H
#define DSM_FILESET_H

#include <IOChannel.h>
#include <DSMConfig.h>
#include <ConnectionRequester.h>
#include <dsm_sample.h>
#include <FsMount.h>

#include <atdUtil/FileSet.h>

#include <iostream>

namespace dsm {

/**
 * Implementation of an IOChannel from a atdUtil::FileSet
 */
class FileSet: public IOChannel, public atdUtil::FileSet {

public:

    FileSet():IOChannel(),atdUtil::FileSet(),requester(0),mount(0) {}

    /**
     * Copy constructor.
     */
    FileSet(const FileSet& x):
    	IOChannel(x),atdUtil::FileSet(x),requester(0),mount(x.mount) {}

    ~FileSet() { delete mount; }

    bool isNewFile() const { return atdUtil::FileSet::isNewFile(); }

    const std::string& getName() const;

    void setFileName(const std::string& val);

    void requestConnection(ConnectionRequester* requester)
    	throw(atdUtil::IOException);

    IOChannel* connect() throw(atdUtil::IOException);

    /**
     * FileSet will own the FsMount.
     */
    void setMount(FsMount* val) { mount = val; }

    /**
     * This method is called by FsMount when it is done.
     */
    void mounted();

    /**
     * Clone myself.
     */
    FileSet* clone() const
    {
        return new FileSet(*this);
    }

    /**
     * Search for the dsm corresponding to the first of my sample tags.
     */
    const DSMConfig* firstDSM();

    dsm_time_t createFile(dsm_time_t t,bool exact)
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
        
    void close() throw(atdUtil::IOException);

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

    ConnectionRequester* requester;

    FsMount* mount;

};

}

#endif
