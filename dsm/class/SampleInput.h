/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_SAMPLEINPUT_H
#define DSM_SAMPLEINPUT_H


#include <SampleSource.h>
#include <InputStream.h>

#include <atdUtil/McSocket.h>

namespace dsm {

class DSMConfig;

/**
 * Interface of an input stream of samples.
 */
class SampleInput: public SampleSource, public DOMable
{
public:
    SampleInput() {};

    virtual ~SampleInput() {}

    virtual SampleInput* clone() = 0;

    virtual void requestConnection(atdUtil::SocketAccepter*)
        throw(atdUtil::IOException) = 0;

    virtual void setPseudoPort(int val) = 0;

    virtual int getPseudoPort() const = 0;

    virtual void offer(atdUtil::Socket* sock) = 0;

    virtual int getFd() const = 0;

    virtual void init() = 0;

    virtual void close() throw(atdUtil::IOException);

    void setDSMConfig(const DSMConfig* val) { dsm = val; }

    const DSMConfig* getDSMConfig() const { return dsm; }

private:
    const DSMConfig* dsm;
};

/**
 * An implementation of a SampleInput, a class for serializing Samples
 * from an InputStream.  
 */
class SampleInputStream: public SampleInput
{

public:

    /**
     * Constructor.
     */
    SampleInputStream();

    /**
     * Copy constructor.
     */
    SampleInputStream(const SampleInputStream&);

    virtual ~SampleInputStream();

    SampleInput* clone();

    void setPseudoPort(int val);

    int getPseudoPort() const;

    void requestConnection(atdUtil::SocketAccepter*)
            throw(atdUtil::IOException);

    void offer(atdUtil::Socket* sock);

    void init();

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     */
    virtual void readSamples() throw(dsm::SampleParseException,atdUtil::IOException);

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    virtual Sample* readSample() throw(SampleParseException,atdUtil::IOException);

    virtual void close() throw(atdUtil::IOException);

    int getFd() const;

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    Input* input;
    InputStream* inputStream;

    int pseudoPort;

    /**
     * Will be non-null if we have previously read part of a sample
     * from the stream.
     */
    Sample* samp;

    /**
     * How many bytes left to read from the stream into the data
     * portion of samp.
     */
    size_t left;

    /**
     * Pointer into the data portion of samp where we will read next.
     */
    char* dptr;

};

}

#endif
