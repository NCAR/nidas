/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_IOCHANNEL_H
#define DSM_IOCHANNEL_H

#include <ConnectionRequester.h>
#include <dsm_sample.h>
#include <DOMable.h>

#include <atdUtil/IOException.h>
#include <atdUtil/Inet4Address.h>

namespace dsm {

class DSMConfig;
class DSMService;

/**
 * A channel for Input or Output of data.
 */
class IOChannel : public DOMable {

public:

    IOChannel();

    virtual ~IOChannel() {}

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;
    /**
     * After the IOChannel is configured, a user of IOChannel calls
     * requestConnection to get things started. It is like opening
     * a device, but in the case of sockets, it just starts the process
     * of establishing a connection to the remote host.
     * The pseudoPort number is used when establishing McSocket
     * connections and is ignored otherwise.
     * Only when the connected() method is called back is the channel
     * actually open and ready for IO.
     */
    virtual void requestConnection(ConnectionRequester*,int pseudoPort)
    	throw(atdUtil::IOException) = 0;

    /**
     * Derived classes must provide clone.
     */
    virtual IOChannel* clone() = 0;

    /**
     * What is the IP address of the host at the other end
     * of the connection. If this IOChannel is not a socket
     * then return INADDR_ANY, using the default constructor
     * of an Inet4Address.  Socket subclasses should override
     * this.
     */
    virtual atdUtil::Inet4Address getRemoteInet4Address() const {
        return atdUtil::Inet4Address();
    }

    /**
     * Return suggested buffer length.
     */
    virtual size_t getBufferSize() const { return 8192; }

    /**
     * Physical read method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t read(void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
     * Physical write method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t write(const void* buf, size_t len)
    	throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

    virtual int getFd() const = 0;

    static IOChannel* createIOChannel(const std::string& type)
            throw(atdUtil::InvalidParameterException);

    /**
     * Request that derived class open a new file, with a name
     * based on a time. This should be implemented by derived
     * classes which write to disk files. Other derived classes
     * (e.g. sockets) can just use this default implementation -
     * basically ignoring the request.
     */
    virtual dsm_sys_time_t createFile(dsm_sys_time_t t) throw(atdUtil::IOException)
    {
        return LONG_LONG_MAX;
    }

    virtual void setDSMConfig(const DSMConfig* val) { dsm = val; }

    virtual const DSMConfig* getDSMConfig() const { return dsm; }

    virtual void setDSMService(const DSMService* val) { service = val; }

    virtual const DSMService* getDSMService() const { return service; }

private:
    const DSMConfig* dsm;

    const DSMService* service;
};

}

#endif
