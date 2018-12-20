// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_FILESET_H
#define NIDAS_CORE_FILESET_H

#include "IOChannel.h"
#include "FsMount.h"

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

    int getReconnectDelaySecs() const
    {
        return 30;
    }

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

    /**
     * @throws nidas::util::IOException
     **/
    void requestConnection(IOChannelRequester* requester);

    /**
     * @throws nidas::util::IOException
     **/
    IOChannel* connect();

    /**
     * @throws nidas::util::IOException
     **/
    void setNonBlocking(bool val)
    {
        if (val) PLOG(("%s: setNonBlocking(true) not implemented",
                    getName().c_str()));
    }

    /**
     * @throws nidas::util::IOException
     **/
    bool isNonBlocking() const
    {
        return false;
    }

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

    /**
     * @throws nidas::util::IOException
     **/
    dsm_time_t createFile(dsm_time_t t, bool exact);

    /**
     * @throws nidas::util::IOException
     **/
    size_t read(void* buf, size_t len)
    {
        return _fset->read(buf,len);
    }

    /**
     * @throws nidas::util::IOException
     **/
    size_t write(const void* buf, size_t len)
    {
#ifdef DEBUG
	std::cerr << getName() << " write, len=" << len << std::endl;
#endif
        return _fset->write(buf,len);
    }

    /**
     * @throws nidas::util::IOException
     **/
    size_t write(const struct iovec* iov, int iovcnt)
    {
        return _fset->write(iov,iovcnt);
    }

    /**
     * @throws nidas::util::IOException
     **/
    void close();

    int getFd() const {
        return _fset->getFd();
    }

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement* node);

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

    /**
     * @throws nidas::util::IOException
     **/
    long long getFileSize() const
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
     *
     * @throws nidas::util::InvalidParameterException
     **/
    static FileSet* getFileSet(const std::list<std::string>& filenames);

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
