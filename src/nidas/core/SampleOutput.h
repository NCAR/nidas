/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEOUTPUT_H
#define NIDAS_CORE_SAMPLEOUTPUT_H

#include <nidas/core/Sample.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/SampleSorter.h>
#include <nidas/core/IOStream.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/core/ConnectionRequester.h>

// #include <nidas/util/McSocket.h>

namespace nidas { namespace core {

class DSMConfig;

/**
 * Interface of an output stream of samples.
 */
class SampleOutput: public SampleClient, public IOChannelRequester, public DOMable
{
public:

    virtual ~SampleOutput() {}

    virtual SampleOutput* clone(IOChannel* iochannel=0) const = 0;

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;

    virtual bool isRaw() const = 0;

    /**
     * SampleOutput does not own the pointer.
     */
    virtual void addSampleTag(const SampleTag*) = 0;

    virtual const std::list<const SampleTag*>& getSampleTags() const = 0;

    /**
     * Request a connection, of this SampleOutput, but don't wait for it.
     * The SampleConnectionRequester will be notified via a call back to
     * SampleConnectionRequester::connected(SampleOutput*,SampleOutput*)
     * where the first SampleOutput points to the SampleOutput
     * of the original request, and the second is often a
     * new instance of a SampleOutput with a new IOChannel connection.
     * Or the two pointers may point to the same SampleOutput.
     */
    virtual void requestConnection(SampleConnectionRequester*)
    	throw(nidas::util::IOException) = 0;

    /**
     * Request a connection, and wait for it.
     */
    virtual void connect() throw(nidas::util::IOException) = 0;

    virtual void disconnect() throw(nidas::util::IOException) = 0;

    virtual int getFd() const = 0;

    virtual IOChannel* getIOChannel() const = 0;

    virtual void init() throw(nidas::util::IOException) = 0;

    /**
     * Plain raw write, typically only used to write an initial
     * header.
     */
    virtual size_t write(const void* buf, size_t len)
    	throw(nidas::util::IOException) = 0;

    virtual void close() throw(nidas::util::IOException) = 0;

    virtual void setHeaderSource(HeaderSource* val) = 0;

    virtual void setDSMConfig(const DSMConfig* val) = 0;

    virtual const DSMConfig* getDSMConfig() const = 0;

    virtual long long getNumReceivedBytes() const { return 0; }

    virtual long long getNumReceivedSamples() const { return 0; }

    virtual dsm_time_t getLastReceivedTimeTag() const { return 0LL; }

protected:

};

/**
 * Implementation of connect/disconnect portions of SampleOutput.
 */
class SampleOutputBase: public SampleOutput
{
public:

    SampleOutputBase(IOChannel* iochan=0);

    /**
     * Copy constructor.
     */
    SampleOutputBase(const SampleOutputBase&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SampleOutputBase(const SampleOutputBase&,IOChannel*);

    virtual ~SampleOutputBase();

    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    bool isRaw() const { return false; }

    void addSampleTag(const SampleTag*);

    const std::list<const SampleTag*>& getSampleTags() const;

    /**
     * Request a connection, but don't wait for it.  Requester will be
     * notified via SampleConnectionRequester interface when the connection
     * has been made.
     */
    void requestConnection(SampleConnectionRequester*)
                 throw(nidas::util::IOException);

    /**
     * Implementation of IOChannelRequester::connected().
     * How an IOChannel indicates that it has received a connection.
     */
    void connected(IOChannel* output) throw();

    /**
     * Request a connection, and wait for it.
     */
    void connect() throw(nidas::util::IOException);

    /**
     * Close the IOChannel and notify whoever did the
     * requestConnection that it is time to disconnect,
     * perhaps because of an IOException.
     */
    void disconnect() throw(nidas::util::IOException);

    int getFd() const;

    void init() throw();

    void close() throw(nidas::util::IOException);

    dsm_time_t getNextFileTime() const { return _nextFileTime; }

    void createNextFile(dsm_time_t) throw(nidas::util::IOException);

    /**
     * Raw write method, typically used to write the initial
     * header.
     */
    size_t write(const void* buf, size_t len)
    	throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    IOChannel* getIOChannel() const { return _iochan; }

    void setHeaderSource(HeaderSource* val)
    {
        _headerSource = val;
    }

    void setDSMConfig(const DSMConfig* val)
    {
        _dsm = val;
    }

    const DSMConfig* getDSMConfig() const
    {
        return _dsm;
    }

    long long getNumReceivedSamples() const { return _nsamples; }

    size_t getNumDiscardedSamples() const { return _nsamplesDiscarded; }

    dsm_time_t getLastReceivedTimeTag() const { return _lastTimeTag; }

    void setLastReceivedTimeTag(dsm_time_t val) { _lastTimeTag = val; }

    /**
     * Add a parameter to this DSMSensor. DSMSensor
     * will then own the pointer and will delete it
     * in its destructor. If a Parameter exists with the
     * same name, it will be replaced with the new Parameter.
     */
    void addParameter(Parameter* val);

    /**
     * Get list of parameters.
     */
    const std::list<const Parameter*>& getParameters() const
    {
	return _constParameters;
    }

    /**
     * Fetch a parameter by name. Returns a NULL pointer if
     * no such parameter exists.
     */
    const Parameter* getParameter(const std::string& name) const;


protected:

    void incrementNumOutputSamples() { _nsamples++; }

    size_t incrementDiscardedSamples() { return _nsamplesDiscarded++; }

    /**
     * Set the IOChannel for this output.
     */
    virtual void setIOChannel(IOChannel* val);

    SampleConnectionRequester* getSampleConnectionRequester()
    {
        return _connectionRequester;
    }

    std::string name;

private:

    IOChannel* _iochan;

    std::list<const SampleTag*> _sampleTags;

    SampleConnectionRequester* _connectionRequester;

    dsm_time_t _nextFileTime;

    HeaderSource* _headerSource;

    const DSMConfig* _dsm;

    long long _nsamples;

    size_t _nsamplesDiscarded;

    dsm_time_t _lastTimeTag;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> _parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

};

}}	// namespace nidas namespace core

#endif
