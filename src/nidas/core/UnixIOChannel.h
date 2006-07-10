/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_UNIXIOCHANNEL_H
#define NIDAS_CORE_UNIXIOCHANNEL_H

#include <nidas/core/IOChannel.h>

#include <fcntl.h>

#include <string>
#include <iostream>

namespace nidas { namespace core {

/**
 * Simple implementation of an IOChannel, over an opened file descriptor.
 */
class UnixIOChannel: public IOChannel {

public:

    /**
     * Constructor.
     */
    UnixIOChannel(const std::string& name, int fdarg): fd(fdarg) {}

    /**
     * Destructor. Does not close the device.
     */
    ~UnixIOChannel() {}

    /**
     * Clone invokes default copy constructor.
     */
    UnixIOChannel* clone() const { return new UnixIOChannel(*this); }

    /**
     * RequestConnection just returns connected immediately.
     */
    void requestConnection(ConnectionRequester* rqstr)
    	throw(nidas::util::IOException)
    {
        rqstr->connected(this);
    }

    /**
     * Pretty simple, we're connected already.
     */
    IOChannel* connect() throw(nidas::util::IOException)
    {
	return this;
    }

    virtual bool isNewFile() const { return newFile; }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::read(fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"read",errno);
	newFile = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::write(fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"write",errno);
	newFile = false;
	return res;
    }

    void close() throw (nidas::util::IOException)
    {
        int res = ::close(fd);
	if (res < 0) throw nidas::util::IOException(getName(),"close",errno);
    }

    int getFd() const
    {
        return fd;
    }

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException)
    {
        throw nidas::util::InvalidParameterException(
		"UnixIOChannel::fromDOMElement not supported");
    }

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException)
    {
        return 0;
    }

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException)
    {
        return 0;
    }
    
protected:

    std::string name;

    int fd;

    bool firstRead;

    bool newFile;
};

}}	// namespace nidas namespace core

#endif
