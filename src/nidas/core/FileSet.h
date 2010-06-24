/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-03-26 22:35:58 -0600 (Thu, 26 Mar 2009) $

    $LastChangedRevision: 4548 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/FileSet.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_FILESET_H
#define NIDAS_CORE_FILESET_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/FsMount.h>

#include <nidas/util/FileSet.h>

#include <iostream>

namespace nidas { namespace core {

/**
 * Implementation of an IOChannel using an nidas::util::FileSet
 */
class FileSet: public IOChannel {

public:

    FileSet();

    /**
     * Constructor from a pointer to a nidas::util::FileSet.
     * nidas::core::FileSet will own the pointer.
     */
    FileSet(nidas::util::FileSet* fset);

    ~FileSet();

    // virtual nidas::util::FileSet& getNUFileSet() { return *_fset; }

    bool isNewInput() const { return _fset->isNewFile(); }

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

    void requestConnection(IOChannelRequester* requester)
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
        return _fset->read(buf,len);
    }
        
    size_t write(const void* buf, size_t len) throw(nidas::util::IOException)
    {
#ifdef DEBUG
	std::cerr << getName() << " write, len=" << len << std::endl;
#endif
        return _fset->write(buf,len);
    }
        
    size_t write(const struct iovec* iov, int iovcnt) throw(nidas::util::IOException)
    {
        return _fset->write(iov,iovcnt);
    }
        
    void close() throw(nidas::util::IOException);

    int getFd() const { return _fset->getFd(); }
        
    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    /**
     * Get name of current file.
     */
    const std::string& getCurrentName() const
    {
        return _fset->getCurrentName();
    }

    void setStartTime(const nidas::util::UTime& val)
    {
        _fset->setStartTime(val);
    } 

    nidas::util::UTime getStartTime() const
    {
        return _fset->getStartTime();
    }

    void setEndTime(const nidas::util::UTime& val)
    {
        _fset->setEndTime(val);
    } 

    nidas::util::UTime getEndTime() const
    {
        return _fset->getStartTime();
    }

    /**
     * Set/get the file length in seconds.
     */
    void setFileLengthSecs(int val)
    {
	_fset->setFileLengthSecs(val);
    }

    int getFileLengthSecs() const
    {
	return _fset->getFileLengthSecs();
    }

    void addFileName(const std::string& val)
    {
        _fset->addFileName(val);
    }

    long long getFileSize() const throw(nidas::util::IOException)
    {
        return _fset->getFileSize();
    }

    /**
     * Get last error value. Should be 0. Currently only supported
     * for an output file, to be queried by a system status thread.
     */
    int getLastErrno() const 
    {
        return _fset->getLastErrno();
    }

    /**
     * Convienence function to return a pointer to a nidas::core::FileSet,
     * given a list of files. If one or more of the files have a .bz2 suffix,
     * the FileSet returned will be a nidas::core::Bzip2FileSet. Note that
     * a Bzip2FileSet cannot be used to read a non-compressed file, so
     * one should not mix compressed and non-compressed files in the list.
     */
    static FileSet* getFileSet(const std::list<std::string>& filenames)
	throw(nidas::util::InvalidParameterException);

protected:

    /**
     * Copy constructor.
     */
    FileSet(const FileSet& x);

    nidas::util::FileSet* _fset;

    /**
     * Recognizeable name of this IOChannel - used for informative
     * messages.
     */
    void setName(const std::string& val);

    std::string _name;

    IOChannelRequester* _requester;

    FsMount* _mount;

private:
    /**
     * No assignment.
     */
    FileSet& operator=(const FileSet&);
};

}}	// namespace nidas namespace core

#endif
