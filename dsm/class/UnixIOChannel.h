/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
 ********************************************************************

*/

#ifndef DSM_UNIXIOCHANNEL_H
#define DSM_UNIXIOCHANNEL_H

#include <IOChannel.h>

#include <fcntl.h>

#include <string>
#include <iostream>

namespace dsm {

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
    	throw(atdUtil::IOException)
    {
        rqstr->connected(this);
    }

    /**
     * Pretty simple, we're connected already.
     */
    IOChannel* connect() throw(atdUtil::IOException)
    {
	return this;
    }

    virtual bool isNewFile() const { return newFile; }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (atdUtil::IOException)
    {
        ssize_t res = ::read(fd,buf,len);
	if (res < 0) throw atdUtil::IOException(getName(),"read",errno);
	newFile = false;
	return res;
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (atdUtil::IOException)
    {
        ssize_t res = ::write(fd,buf,len);
	if (res < 0) throw atdUtil::IOException(getName(),"write",errno);
	newFile = false;
	return res;
    }

    void close() throw (atdUtil::IOException)
    {
        int res = ::close(fd);
	if (res < 0) throw atdUtil::IOException(getName(),"close",errno);
    }

    int getFd() const
    {
        return fd;
    }

    const std::string& getName() const { return name; }

    void setName(const std::string& val) { name = val; }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
    {
        throw atdUtil::InvalidParameterException(
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

}

#endif
