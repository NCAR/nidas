/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_SAMPLEOUTPUTSTREAM_H
#define DSM_SAMPLEOUTPUTSTREAM_H


#include <Sample.h>
#include <SampleClient.h>
#include <SampleParseException.h>
#include <OutputStream.h>

#include <atdUtil/Socket.h>
#include <DSMOutputFileSet.h>


namespace dsm {

class DSMConfig;
/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleClient, public DOMable {

public:
    SampleOutputStream();
    virtual ~SampleOutputStream();

    virtual void setDSMConfig(DSMConfig* val) { dsmConfig = val; }

    virtual DSMConfig* getDSMConfig() { return dsmConfig; }

    virtual void close() throw(atdUtil::IOException);

    void setSocketAddress(atdUtil::Inet4SocketAddress& saddr);

    const atdUtil::Inet4SocketAddress& getSocketAddress() const;

    /**
     * A DSMService that starts running with a socket connection
     * will call this method.
     */
    void setSocket(atdUtil::Socket& sock);

    void setFileSet(atdUtil::OutputFileSet& fset);

    /**
     * Establish our own connection.
     */
    virtual void connect() throw(atdUtil::IOException);

    bool receive(const Sample *s)
	throw(SampleParseException, atdUtil::IOException);

    /**
     * Called by external object to reset this SampleClient. 
     * For example, re-open a socket.
     */
    virtual void reset(Sample *s) throw(atdUtil::IOException) {}

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    DSMConfig* dsmConfig;

    /** Do we need to keep track of sample time tags,
     * as when writing to time-tagged archive files,
     * or can we just simply write the samples.
     */
    enum type { SIMPLE, TIMETAG_DEPENDENT } type;

    OutputStream* outputStream;

    atdUtil::Inet4SocketAddress socketAddress;

    dsm_sys_time_t fullSampleTimetag;

    dsm_sys_time_t t0day;

    dsm_sys_time_t nextFileTime;

    dsm_sys_time_t tsampLast;

    int questionableTimetags;

};

}

#endif
