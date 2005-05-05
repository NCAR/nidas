/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_SAMPLEOUTPUT_H
#define DSM_SAMPLEOUTPUT_H


#include <Sample.h>
#include <SampleClient.h>
#include <SampleSorter.h>
#include <IOStream.h>
#include <ConnectionRequester.h>

// #include <atdUtil/McSocket.h>

namespace dsm {

class DSMConfig;

/**
 * Interface of an output stream of samples.
 */
class SampleOutput: public SampleClient, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleOutput() {}

    virtual SampleOutput* clone() const = 0;

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;

    virtual bool isRaw() const = 0;

    virtual void setPseudoPort(int val) = 0;

    virtual int getPseudoPort() const = 0;

    virtual void setDSMConfigs(const std::list<const DSMConfig*>& val) = 0;

    virtual void addDSMConfig(const DSMConfig*) = 0;

    virtual const std::list<const DSMConfig*>& getDSMConfigs() const = 0;

    // virtual void setDSMService(const DSMService*) = 0;

    // virtual const DSMService* getDSMService() const = 0;

    virtual void requestConnection(SampleConnectionRequester*)
    	throw(atdUtil::IOException) = 0;

    virtual void connected(IOChannel* sock) throw() = 0;

    virtual int getFd() const = 0;

    virtual void init() throw(atdUtil::IOException) = 0;

    virtual void flush() throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

protected:
    
};

/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleOutput
{
public:

    SampleOutputStream();

    /**
     * Copy constructor.
     */
    SampleOutputStream(const SampleOutputStream&);

    virtual ~SampleOutputStream();

    SampleOutput* clone() const;

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    bool isRaw() const { return false; }

    void setPseudoPort(int val);

    int getPseudoPort() const;

    void setDSMConfigs(const std::list<const DSMConfig*>& val);

    void addDSMConfig(const DSMConfig*);

    const std::list<const DSMConfig*>& getDSMConfigs() const;

    void setDSMService(const DSMService*);

    const DSMService* getDSMService() const;

    void requestConnection(SampleConnectionRequester*)
                 throw(atdUtil::IOException);

    void connected(IOChannel* output) throw();

    void init() throw();

    int getFd() const;

    bool receive(const Sample *s) throw();

    size_t write(const Sample* samp) throw(atdUtil::IOException);

    void flush() throw(atdUtil::IOException);

    void close() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    std::string name;

    IOChannel* iochan;

    IOStream* iostream;

    int pseudoPort;

    std::list<const DSMConfig*> dsms;

    const DSMService* service;

    SampleConnectionRequester* connectionRequester;

    dsm_time_t nextFileTime;

};

/**
 * A proxy for a SampleOutputStream. One passes a reference to a
 * SampleOutputStream to the constructor for this proxy.
 * The SampleOutputStreamProxy::receive method
 * will invoke the SampleOutputStream::receive() method not the
 * derived method.
 */
class SampleOutputStreamProxy: public SampleOutputStream
{
public:
    SampleOutputStreamProxy(int dummy,SampleOutputStream& out):
    	outstream(out) {}
    bool receive(const Sample* samp) throw()
    {
	// cerr << "Proxy receive" << endl;
        return outstream.SampleOutputStream::receive(samp);
    }
private:
    SampleOutputStream& outstream;
};

/**
 * A class for serializing Samples on an OutputStream.
 */
class SortedSampleOutputStream: public SampleOutputStream
{
public:

    SortedSampleOutputStream();

    /**
     * Copy constructor.
     */
    SortedSampleOutputStream(const SortedSampleOutputStream&);

    virtual ~SortedSampleOutputStream();

    SampleOutput* clone() const;

    void init() throw();

    bool receive(const Sample *s) throw();

protected:
    bool initialized;

    SampleSorter sorter;

    SampleOutputStreamProxy proxy;
};

}

#endif
