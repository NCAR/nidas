/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_OUTPUT_H
#define DSM_OUTPUT_H

#include <atdUtil/McSocket.h>
#include <DOMable.h>
#include <DSMTime.h>

namespace dsm {

class DSMService;

/**
 * A virtual base class for writing data.
 */
class Output : public DOMable {

public:

    virtual const std::string& getName() const = 0;
    /**
     * After the Output is configured, a user of Output calls requestConnection
     * to get things started. It is like opening a device, but in the case
     * of sockets, it just starts the process of accepting or connecting.
     */
    virtual void requestConnection(atdUtil::SocketAccepter* service,
    	int pseudoPort) throw(atdUtil::IOException) = 0;

    /**
     * Derived classes must provide clone.
     */
    virtual Output* clone() const = 0;

    /**
     * When the socket connection has been established, this offer
     * method will be called.  Actual socket outputs must override this.
     */
    virtual void offer(atdUtil::Socket* sock) throw(atdUtil::Exception);

    /**
     * Return suggested buffer length.
     */
    virtual size_t getBufferSize() { return 8192; }

    /**
     * Physical write method which must be implemented in derived
     * classes. Returns the number of bytes written, which
     * may be less than the number requested.
     */
    virtual size_t write(const void* buf, size_t len)
    	throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

    virtual int getFd() const = 0;

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

    static Output* fromOutputDOMElement(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException);
};

}

#endif
