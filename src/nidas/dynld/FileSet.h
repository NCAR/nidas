/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_FILESET_H
#define NIDAS_DYNLD_FILESET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/ConnectionRequester.h>
#include <nidas/dynld/FsMount.h>

#include <nidas/util/FileSet.h>

#include <iostream>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Implementation of an IOChannel from a nidas::util::FileSet
 */
class FileSet: public IOChannel, public nidas::util::FileSet {

public:

    FileSet():IOChannel(),nidas::util::FileSet(),
        _name("FileSet"),_requester(0),_mount(0) {}

    /**
     * Copy constructor.
     */
    FileSet(const FileSet& x);

    ~FileSet() { delete _mount; }

    /**
     * Set the DSM associated with this FileSet.
     * FileSet needs this in order to substitute
     * tokens like $DSM in the file or directory names.
     */
    void setDSMConfig(const DSMConfig* val);

    bool isNewInput() const { return nidas::util::FileSet::isNewFile(); }

    const std::string& getName() const;

    /**
     * Set the directory portion of the file search path.
     * This may contain environment variables,
     * and tokens like $DSM, $SITE, $AIRCRAFT.
     */
    void setDir(const std::string& val);

    /**
     * Set the file portion of the file search path.
     * This may contain environment variables,
     * and tokens like $DSM, $SITE, $AIRCRAFT.
     */
    void setFileName(const std::string& val);

    void requestConnection(ConnectionRequester* requester)
    	throw(nidas::util::IOException);

    IOChannel* connect() throw(nidas::util::IOException);

    /**
     * FileSet will own the FsMount.
     */
    void setMount(FsMount* val) { _mount = val; }

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

    dsm_time_t createFile(dsm_time_t t,bool exact)
	throw(nidas::util::IOException);

    size_t read(void* buf, size_t len) throw(nidas::util::IOException)
    {
        return nidas::util::FileSet::read(buf,len);
    }
        
    size_t write(const void* buf, size_t len) throw(nidas::util::IOException)
    {
#ifdef DEBUG
	std::cerr << getName() << " write, len=" << len << std::endl;
#endif
        return nidas::util::FileSet::write(buf,len);
    }
        
    void close() throw(nidas::util::IOException);

    int getFd() const { return nidas::util::FileSet::getFd(); }
        
    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:
    /**
     * Recognizeable name of this IOChannel - used for informative
     * messages.
     */
    void setName(const std::string& val);

    std::string _name;

    ConnectionRequester* _requester;

    FsMount* _mount;

};

}}	// namespace nidas namespace core

#endif
