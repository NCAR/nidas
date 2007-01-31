/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_GOESXMTR_H
#define NIDAS_DYNLD_ISFF_GOESXMTR_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>
#include <nidas/util/SerialPort.h>
#include <nidas/util/UTime.h>

#include <string>
#include <iostream>
#include <vector>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * Support for a GOES transmitter, implemented as an IOChannel.
 */

class GOESXmtr: public IOChannel {

public:

    /**
     * Constructor.
     */
    GOESXmtr();

    /**
     * Copy constructor.
     */
    GOESXmtr(const GOESXmtr&);

    /**
     * Destructor.
     */
    virtual ~GOESXmtr();

    const std::string& getName() const
    {
	return port.getName();
    }

    void setName(const std::string& val)
    {
	port.setName(val);
    }

    void setId(unsigned long val)
    {
        id = val;
    }

    unsigned long getId() const
    {
        return id;
    }
    
    void setChannel(int val)
    {
        channel = val;
    }

    int getChannel() const
    {
        return channel;
    }
    
    /**
     * Set the transmission interval.
     * @param val Interval, in seconds.
     */
    void setXmitInterval(long val)
    {
        xmitInterval = val;
    }

    int getXmitInterval() const
    {
        return xmitInterval;
    }
    
    /**
     * Set the transmission offset.
     * @param val Offset, in seconds.
     */
    void setXmitOffset(long val)
    {
        xmitOffset = val;
    }

    int getXmitOffset() const
    {
        return xmitOffset;
    }
    
    /**
     * Set the RF baud rate
     * @param val RF baud, in bits/sec.
     */
    void setRFBaud(long val)
    {
        rfBaud = val;
    }

    int getRFBaud() const
    {
        return rfBaud;
    }
    
    /**
     * Request a connection.
     */
    void requestConnection(ConnectionRequester* rqstr)
    	throw(nidas::util::IOException);

    /**
     * 
     */
    IOChannel* connect() throw(nidas::util::IOException);

    virtual void open() throw(nidas::util::IOException);

    /**
     * Initialize tranmitter.
     */
    virtual void init() throw(nidas::util::IOException) = 0;

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException) = 0;

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    	= 0;

    /**
     * Queue a sample for writing to a GOES transmitter.
    */
    virtual void transmitData(const nidas::util::UTime& at,
    	int configid,const Sample*) throw (nidas::util::IOException) = 0;

    virtual unsigned long checkId() throw(nidas::util::IOException) = 0;

    /**
     * Check transmitter clock, and correct it if necessary.
     * @return transmitter clock minus system clock, in milliseconds.
     */
    virtual int checkClock() throw(nidas::util::IOException) = 0;

    virtual void reset() throw(nidas::util::IOException) = 0;

    /**
     * Request that transmitter status be printed to an output stream.
     */
    virtual void printStatus() throw() = 0;

    virtual void flush() throw (nidas::util::IOException) 
    {
        port.flushBoth();
    }

    virtual void close() throw (nidas::util::IOException);

    int getFd() const
    {
        return port.getFd();
    }

    void setStatusFile(const std::string& val)
    {
        statusFile = val;
    }

    const std::string& getStatusFile() const {
        return statusFile;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    nidas::util::SerialPort port;

private:

    unsigned long id;
    
    int channel;

    int xmitInterval;

    int xmitOffset;

    int rfBaud;

    std::string statusFile;

};


}}}	// namespace nidas namespace dynld namespace isff

#endif
