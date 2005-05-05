/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_SAMPLEINPUT_H
#define DSM_SAMPLEINPUT_H


#include <SampleSource.h>
#include <IOStream.h>
#include <ConnectionRequester.h>
#include <SampleSorter.h>

namespace dsm {

class DSMConfig;
class DSMSensor;

/**
 * Interface of an input SampleSource from one or more DSMs.
 */
class SampleInput: public SampleSource
{
public:

    virtual ~SampleInput() {}

    virtual std::string getName() const = 0;

    /**
     * What DSMConfigs are associated with this SampleInput.
     */
    virtual const std::list<const DSMConfig*>& getDSMConfigs() const = 0;

    virtual void setDSMConfigs(const std::list<const DSMConfig*>& val) = 0;

    virtual void addDSMConfig(const DSMConfig* val) = 0;

    // virtual size_t getUnrecognizedSamples() const = 0;

    /**
     * Client wants samples from the process() method of the
     * given DSMSensor.
     */
    virtual void addProcessedSampleClient(SampleClient*,DSMSensor*) = 0;

    virtual void removeProcessedSampleClient(SampleClient*,DSMSensor*) = 0;

};


class SampleInputMerger: public SampleInput , protected SampleClient
{
public:
    SampleInputMerger();

    virtual ~SampleInputMerger();

    std::string getName() const { return name; }

    /**
     * What DSMConfigs are associated with this SampleInput.
     */
    const std::list<const DSMConfig*>& getDSMConfigs() const { return dsms; }

    void addDSMConfig(const DSMConfig* val) { dsms.push_back(val); }

    void setDSMConfigs(const std::list<const DSMConfig*>& val) { dsms = val; }

    void addInput(SampleInput* input);

    void removeInput(SampleInput* input);

    void addProcessedSampleClient(SampleClient*,DSMSensor*);

    void removeProcessedSampleClient(SampleClient*,DSMSensor*);

    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw();

    bool receive(const Sample*) throw();

protected:

    std::string name;

    std::list<const DSMConfig*> dsms;

    std::map<unsigned long int, DSMSensor*> sensorMap;

    atdUtil::Mutex sensorMapMutex;

    SampleSorter sorter1;

    SampleSorter sorter2;

    size_t unrecognizedSamples;

};


/**
 * Extension of the interface to a SampleInput providing the
 * methods needed to establish the connection to the source
 * of samples (socket or files) and actually read samples
 * from the connection.
 */
class SampleInputReader: public SampleInput, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleInputReader() {}

    virtual void setPseudoPort(int val) = 0;

    virtual int getPseudoPort() const = 0;

    virtual void requestConnection(DSMService*)
        throw(atdUtil::IOException) = 0;

    virtual atdUtil::Inet4Address getRemoteInet4Address() const = 0;

    virtual void init() throw(atdUtil::IOException) = 0;

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

    virtual void close() throw(atdUtil::IOException) = 0;

};

/**
 * An implementation of a SampleInputReader.
 */
class SampleInputStream: public SampleInputReader
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

    virtual SampleInputStream* clone() const;

    std::string getName() const;

    // bool isRaw() const { return false; }

    void setPseudoPort(int val) { pseudoPort = val; }

    int getPseudoPort() const { return pseudoPort; }

    /**
     * What DSMConfigs are associated with this SampleInput.
     */
    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
	return dsms;
    }

    void setDSMConfigs(const std::list<const DSMConfig*>& val)
    {
	dsms = val;
    }

    void addDSMConfig(const DSMConfig* val)
    {
        dsms.push_back(val);
    }

    void addProcessedSampleClient(SampleClient*,DSMSensor*);

    void removeProcessedSampleClient(SampleClient*,DSMSensor*);

    void requestConnection(DSMService*) throw(atdUtil::IOException);

    void connected(IOChannel* iochan) throw();

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

    void newFile() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    /**
     * Service that has requested my input.
     */
    DSMService* service;

    std::list<const DSMConfig*> dsms;

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
