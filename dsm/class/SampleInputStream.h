/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_SAMPLEINPUTSTREAM_H
#define DSM_SAMPLEINPUTSTREAM_H


// #include <Sample.h>
#include <SampleSource.h>
#include <InputStream.h>
#include <DOMable.h>

#include <atdUtil/Socket.h>
#include <atdUtil/InputFileSet.h>

namespace dsm {

/**
 * A class for serializing Samples from an InputStream.
 */
class SampleInputStream: public SampleSource, public DOMable {

public:
    SampleInputStream();
    virtual ~SampleInputStream();

    void setSocketAddress(atdUtil::Inet4SocketAddress& saddr);

    const atdUtil::Inet4SocketAddress& getSocketAddress() const;

    /**
     * Set my input stream to be from a socket.
     */
    void setSocket(atdUtil::Socket& sock);

    /**
     * Set my input stream to be from a FileSet.
     */
    void setFileSet(atdUtil::InputFileSet& fset);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     */
    void readSamples() throw(dsm::SampleParseException,atdUtil::IOException);

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    Sample* readSample() throw(SampleParseException,atdUtil::IOException);

protected:

    atdUtil::Inet4SocketAddress socketAddress;

    InputStream* inputStream;
    /**
     * Will be non-null if we have previously read part of a sample
     * from the stream.
     */
    Sample* samp;

    /**
     * How many bytes left to read from the stream into the data portion of samp.
     */
    size_t left;

    /**
     * Pointer into the data portion of samp where we will read next.
     */
    char* dptr;

};

}

#endif
