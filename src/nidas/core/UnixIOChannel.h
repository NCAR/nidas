/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
    void requestConnection(IOChannelRequester* rqstr)
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

    virtual bool isNewInput() const { return newInput; }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::read(fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"read",errno);
	newInput = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::write(fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"write",errno);
	newInput = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
        ssize_t res = ::writev(fd,iov,iovcnt);
	if (res < 0) throw nidas::util::IOException(getName(),"write",errno);
	newInput = false;
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

protected:

    std::string name;

    int fd;

    bool firstRead;

    bool newInput;
};

}}	// namespace nidas namespace core

#endif
