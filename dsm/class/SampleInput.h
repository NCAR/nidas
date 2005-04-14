/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_SAMPLEINPUT_H
#define DSM_SAMPLEINPUT_H


#include <SampleSource.h>
#include <IOStream.h>
#include <ConnectionRequester.h>

namespace dsm {

class DSMConfig;
class DSMSensor;

/**
 * Interface of an input stream of samples.
 */
class SampleInput: public SampleSource, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleInput() {}

    virtual SampleInput* clone() const = 0;

    virtual std::string getName() const = 0;

    virtual bool isRaw() const = 0;

    virtual void setPseudoPort(int val) = 0;

    virtual int getPseudoPort() const = 0;

    virtual void addSensor(DSMSensor* sensor) = 0;

    virtual void requestConnection(DSMService*)
        throw(atdUtil::IOException) = 0;

    /**
     * If an SampleInput is associated with one DSM, when
     * getDSMConfig() should return a non-null pointer to
     * a DSMConfig.
     */
    virtual const DSMConfig* getDSMConfig() const = 0;

    virtual void setDSMConfig(const DSMConfig* val) = 0;

    virtual atdUtil::Inet4Address getRemoteInet4Address() const = 0;

    virtual void init() throw() = 0;

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my SampleClients.
     * This will perform only one physical read of the underlying device
     * and so is appropriate to use when a select() has determined
     * that there is data availabe on our file descriptor.
     */
    virtual void readSamples() throw(atdUtil::IOException) = 0;

    /**
     * Blocking read of the next sample from the buffer. The caller must
     * call freeReference on the sample when they're done with it.
     */
    virtual Sample* readSample() throw(atdUtil::IOException) = 0;

    virtual size_t getUnrecognizedSamples() const = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

};

/**
 * An implementation of a SampleInput, a class for serializing Samples
 * from an IOStream.  
 */
class SampleInputStream: public SampleInput
{

public:

    /**
     * Constructor.
     * @param iochannel The IOChannel that we use for data input.
     *   SampleInput will own the pointer to the IOChannel,
     *   and will delete it in ~SampleInputStream(). If 
     *   it is a null pointer, then it must be set within
     *   the fromDOMElement method.
     */
    SampleInputStream(IOChannel* iochannel = 0);

    /**
     * Copy constructor.  This will do a clone() of the IOChannel.
     */
    SampleInputStream(const SampleInputStream&);

    virtual ~SampleInputStream();

    SampleInput* clone() const;

    std::string getName() const;

    bool isRaw() const { return false; }

    void setPseudoPort(int val);

    int getPseudoPort() const;

    void addSensor(DSMSensor* sensor);

    void requestConnection(DSMService*) throw(atdUtil::IOException);

    void connected(IOChannel* iochan) throw();

    /**
     * If an SampleInput is associated with one DSM, when
     * getDSMConfig() should return a non-null pointer to
     * a DSMConfig.
     */
    virtual const DSMConfig* getDSMConfig() const { return dsm; }

    virtual void setDSMConfig(const DSMConfig* val) { dsm = val; }

    atdUtil::Inet4Address getRemoteInet4Address() const;

    void init() throw();

    /**
     * Read a buffer of data, serialize the data into samples,
     * and distribute() samples to the receive() method of my
     * SampleClients and DSMSensors.
     * This will perform only one physical read of the underlying
     * IOChannel and so is appropriate to use when a select()
     * has determined that there is data available on our file
     * descriptor.
     */
    void readSamples() throw(atdUtil::IOException);

    /**
     * Read the next sample from the InputStream. The caller must
     * call freeReference on the sample when they're done with it.
     * This method may perform zero or more reads of the IOChannel.
     */
    Sample* readSample() throw(atdUtil::IOException);

    size_t getUnrecognizedSamples() const { return unrecognizedSamples; }

    void close() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    DSMService* service;

    const DSMConfig* dsm;

    IOChannel* iochan;

    IOStream* iostream;

    int pseudoPort;

    std::map<unsigned long int, DSMSensor*> sensorMap;

    atdUtil::Mutex sensorMapMutex;

private:

    /**
     * Will be non-null if we have previously read part of a sample
     * from the stream.
     */
    Sample* samp;

    /**
     * How many bytes left to read from the stream into the data
     * portion of samp.
     */
    size_t leftToRead;

    /**
     * Pointer into the data portion of samp where we will read next.
     */
    char* dptr;

    size_t unrecognizedSamples;

};

}

#endif
