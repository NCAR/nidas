/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#ifndef DSM_SAMPLEOUTPUT_H
#define DSM_SAMPLEOUTPUT_H


#include <Sample.h>
#include <SampleClient.h>
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

    virtual void setDSMConfig(const DSMConfig* val) = 0;

    virtual const DSMConfig* getDSMConfig() const = 0;

    virtual void setDSMService(const DSMService*) = 0;

    virtual const DSMService* getDSMService() const = 0;

    virtual void requestConnection(SampleConnectionRequester*)
    	throw(atdUtil::IOException) = 0;

    virtual void connected(IOChannel* sock) throw() = 0;

    virtual int getFd() const = 0;

    virtual void init() throw() = 0;

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

    void setDSMConfig(const DSMConfig* val);

    const DSMConfig* getDSMConfig() const;

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

    const DSMConfig* dsm;

    const DSMService* service;

    SampleConnectionRequester* connectionRequester;

    /** Do we need to keep track of sample time tags,
     * as when writing to time-tagged archive files,
     * or can we just simply write the samples.
     */
    enum type { SIMPLE, TIMETAG_DEPENDENT } type;

    dsm_time_t nextFileTime;

};

}

#endif
