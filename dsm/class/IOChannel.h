/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_IOCHANNEL_H
#define DSM_IOCHANNEL_H

#include <ConnectionRequester.h>

#include <dsm_sample.h>
#include <DOMable.h>

#include <atdUtil/IOException.h>
#include <atdUtil/Inet4Address.h>

#include <set>

namespace dsm {

class SampleTag;
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

    /*
     * The requestNum number is used when establishing McSocket
     * connections and is ignored otherwise.
     */
    virtual void setRequestNumber(int val) {}

    virtual int getRequestNumber() const { return -1; }

    virtual bool isNewFile() const { return false; }

    /**
     * After the IOChannel is configured, a user of IOChannel calls
     * requestConnection to get things started. It is like opening
     * a device, but in the case of sockets, it just starts the process
     * of establishing a connection to the remote host.
     * Only when the ConnectionRequester::connected() method
     * is called back is the channel actually open and ready for IO.
     * The IOChannel* returned by ConnectionRequester::connected
     * may be another instance of an IOChannel.
     */
    virtual void requestConnection(ConnectionRequester*)
    	throw(atdUtil::IOException) = 0;

    /**
     * Establish a connection. On return, the connection has been
     * established. It returns a new instance of an IOChannel.
     */
    virtual IOChannel* connect() throw(atdUtil::IOException) = 0;

    /**
     * Derived classes must provide clone.
     */
    virtual IOChannel* clone() const = 0;

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
    virtual size_t getBufferSize() const throw() { return 8192; }

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

    /**
     * Default flush implementation does nothing.
     */
    virtual void flush() throw(atdUtil::IOException) {}

    virtual void close() throw(atdUtil::IOException) = 0;

    virtual int getFd() const = 0;

    static IOChannel* createIOChannel(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException);

    /**
     * Request that derived class open a new file, with a name
     * based on a time. This should be implemented by derived
     * classes which write to disk files. Other derived classes
     * (e.g. sockets) can just use this default implementation -
     * basically ignoring the request.
     * @param t Time to use when creating file name.
     * @param exact Use exact time when creating file name, else
     *        the time is adjusted to an even time interval.
     */
    virtual dsm_time_t createFile(dsm_time_t t,bool exact)
    	throw(atdUtil::IOException)
    {
        return LONG_LONG_MAX;
    }

    /**
     * An IOChannel often needs to know what samples it is providing
     * I/O for.  It can use the sample ids to lookup up stuff that is
     * may need in the Project tree, like DSMConfig, DSMSensor, etc.
     * Right now this is mostly useful for the FileSet IOChannel,
     * which needs to match some string tokens like "{SITE}".
     */
    virtual void addSampleTag(const SampleTag* val) 
    {
        sampleTags.insert(val);
    }

    virtual const std::set<const SampleTag*>& getSampleTags() const
    {
        return sampleTags;
    }

private:

    std::set<const SampleTag*> sampleTags;
};

}

#endif
