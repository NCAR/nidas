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

#ifndef NIDAS_CORE_UNIXIOCHANNEL_H
#define NIDAS_CORE_UNIXIOCHANNEL_H

#include <nidas/core/IOChannel.h>

#include <fcntl.h>
#include <unistd.h>  // read(), write(), close(), ...

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
    UnixIOChannel(const std::string& name, int fd):
        _name(name),_fd(fd),_newInput(true) {}

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

    virtual bool isNewInput() const { return _newInput; }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::read(_fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"read",errno);
	_newInput = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        ssize_t res = ::write(_fd,buf,len);
	if (res < 0) throw nidas::util::IOException(getName(),"write",errno);
	_newInput = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
        ssize_t res = ::writev(_fd,iov,iovcnt);
	if (res < 0) throw nidas::util::IOException(getName(),"write",errno);
	_newInput = false;
	return res;
    }

    void close() throw (nidas::util::IOException)
    {
        int res = ::close(_fd);
	if (res < 0) throw nidas::util::IOException(getName(),"close",errno);
    }

    int getFd() const
    {
        return _fd;
    }

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException)
    {
        throw nidas::util::InvalidParameterException(
		"UnixIOChannel::fromDOMElement not supported");
    }

protected:

    /**
     * Constructor.
     */
    UnixIOChannel(const UnixIOChannel& x): IOChannel(x),
        _name(x._name),_fd(x._fd),_newInput(x._newInput)
    {
    }

    std::string _name;

    int _fd;

    bool _newInput;
};

}}	// namespace nidas namespace core

#endif
