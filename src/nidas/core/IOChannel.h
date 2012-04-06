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

#ifndef NIDAS_CORE_IOCHANNEL_H
#define NIDAS_CORE_IOCHANNEL_H

#include <nidas/core/ConnectionInfo.h>

#include <nidas/core/Datagrams.h>
#include <nidas/core/DOMable.h>

#include <nidas/core/Sample.h>

#include <nidas/util/IOException.h>
#include <nidas/util/Inet4Address.h>

#include <sys/uio.h>

#include <set>

namespace nidas { namespace core {

class DSMService;
class DSMConfig;
class IOChannel;

/**
 *  Interface for an object that requests connections to Inputs
 *  or Outputs.
 */
class IOChannelRequester
{
public:
    virtual ~IOChannelRequester() {}
    virtual IOChannelRequester* connected(IOChannel*) throw() = 0;
};

/**
 * A channel for Input or Output of data.
 */
class IOChannel : public DOMable {

public:

    IOChannel();

    IOChannel(const IOChannel& x):DOMable(),_dsm(x._dsm),_conInfo() {}

    IOChannel& operator=(const IOChannel& rhs)
    {
        if (&rhs != this) {
            *(DOMable*) this = rhs;
            _dsm = rhs._dsm;
        }
        return *this;
    }

    virtual ~IOChannel() {}

    /**
     * Derived classes must provide clone.
     */
    virtual IOChannel* clone() const = 0;

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;

    /*
     * The requestType is used when establishing McSocket
     * connections and is ignored otherwise.
     */
    virtual void setRequestType(enum McSocketRequest) {}

    virtual enum McSocketRequest getRequestType() const { return (enum McSocketRequest)0; }

    /**
     * Some IOChannels, namely FileSet, which opens successive files,
     * need to indicate when a read is from the start of a new file.
     * This method is used by code which may need to do special things at the
     * beginning of a file, like read a SampleInputHeader.
     */
    virtual bool isNewInput() const { return false; }

    /**
     * After the IOChannel is configured, a user of IOChannel calls
     * requestConnection to get things started. It is like opening
     * a device, but in the case of server sockets, it just starts
     * a thread to wait on connections.
     * Only when the IOChannelRequester::connected() method
     * is called back is the channel actually open and ready for IO.
     * The IOChannel* returned by IOChannelRequester::connected
     * may be another instance of an IOChannel.
     */
    virtual void requestConnection(IOChannelRequester*)
    	throw(nidas::util::IOException) = 0;

    virtual int getReconnectDelaySecs() const
    {
        return 10;
    }

    /**
     * Establish a connection. On return, the connection has been
     * established. It may return a new instance of an IOChannel.
     */
    virtual IOChannel* connect()
        throw(nidas::util::IOException,nidas::util::UnknownHostException) = 0;

    /**
     * What is the IP address of the host at the other end
     * of the connection. If this IOChannel is not a socket
     * then return INADDR_ANY, using the default constructor
     * of an Inet4Address.  Socket subclasses should override
     * this.
     */
    virtual const ConnectionInfo& getConnectionInfo() const
    {
        return _conInfo;
    }

    virtual void setConnectionInfo(const ConnectionInfo& val)
    {
        _conInfo = val;
    }

    /**
     * Return suggested buffer length.
     */
    virtual size_t getBufferSize() const throw() { return 8192; }

    /**
     * Physical read method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t read(void* buf, size_t len) throw(nidas::util::IOException) = 0;

    /**
     * Physical write method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t write(const void* buf, size_t len)
    	throw(nidas::util::IOException) = 0;

    /**
     * Physical write method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t write(const struct iovec* iov, int iovcnt)
    	throw(nidas::util::IOException) = 0;

    /**
     * Default flush implementation does nothing.
     */
    virtual void flush() throw(nidas::util::IOException) {}

    virtual void close() throw(nidas::util::IOException) = 0;

    virtual int getFd() const = 0;

    static IOChannel* createIOChannel(const xercesc::DOMElement* node)
            throw(nidas::util::InvalidParameterException);

    /**
     * Request that an IOChannel open a new file, with a name
     * based on a time. This should be implemented by derived
     * classes which write to disk files. Other derived classes
     * (e.g. sockets) can just use this default implementation -
     * basically ignoring the request.
     * @param t Time to use when creating file name.
     * @param exact Use exact time when creating file name, else
     *        the time is adjusted to an even time interval.
     */
    virtual dsm_time_t createFile(dsm_time_t,bool)
    	throw(nidas::util::IOException)
    {
        return LONG_LONG_MAX;
    }

    /**
     * Should the NIDAS header be written to this IOChannel?
     * NIDAS headers are not written to DatagramSockets, because
     * there is no guarantee they will get there.
     */
    virtual bool writeNidasHeader() const { return true; }

    /**
     * What DSM is this IOChannel connected to?
     */
    virtual void setDSMConfig(const DSMConfig* val) 
    {
        _dsm = val;
    }

    /**
     * What DSM is this IOChannel connected to? May be NULL.
     */
    virtual const DSMConfig* getDSMConfig() const 
    {
        return _dsm;
    }

private:
    
    const DSMConfig* _dsm;

    ConnectionInfo _conInfo;

};

}}	// namespace nidas namespace core

#endif
