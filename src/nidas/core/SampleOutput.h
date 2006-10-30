/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-22 14:51:10 -0600 (Mon, 22 May 2006) $

    $LastChangedRevision: 3363 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleOutput.h $
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEOUTPUT_H
#define NIDAS_CORE_SAMPLEOUTPUT_H


#include <nidas/core/Sample.h>
#include <nidas/core/SampleInputHeader.h>
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
class SampleOutput: public SampleClient, public ConnectionRequester, public DOMable
{
public:

    virtual ~SampleOutput() {}

    virtual SampleOutput* clone(IOChannel* iochannel=0) const = 0;

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;

    virtual bool isRaw() const = 0;

    virtual void addSampleTag(const SampleTag*) = 0;

    virtual const std::set<const SampleTag*>& getSampleTags() const = 0;

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

    virtual void connected(IOChannel* ioc) throw() = 0;

    virtual int getFd() const = 0;

    virtual IOChannel* getIOChannel() const = 0;


    virtual void init() throw(nidas::util::IOException) = 0;

    /**
     * Plain raw write, typically only used to write an initial
     * header.
     */
    virtual void write(const void* buf, size_t len)
    	throw(nidas::util::IOException) = 0;

    virtual void close() throw(nidas::util::IOException) = 0;

    virtual void setHeaderSource(HeaderSource* val) = 0;

    virtual void setDSMConfig(const DSMConfig* val) = 0;

    virtual const DSMConfig* getDSMConfig() const = 0;

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

    const std::set<const SampleTag*>& getSampleTags() const;

    /**
     * Request a connection, but don't wait for it.  Requester will be
     * notified via SampleConnectionRequester interface when the connection
     * has been made.
     */
    void requestConnection(SampleConnectionRequester*)
                 throw(nidas::util::IOException);

    void connected(IOChannel* output) throw();

    /**
     * Request a connection, and wait for it.
     */
    void connect() throw(nidas::util::IOException);

    void disconnect() throw(nidas::util::IOException);

    int getFd() const;

    void init() throw();

    void close() throw(nidas::util::IOException);

    dsm_time_t getNextFileTime() const { return nextFileTime; }

    void createNextFile(dsm_time_t) throw(nidas::util::IOException);

    /**
     * Raw write method, typically used to write the initial
     * header.
     */
    void write(const void* buf, size_t len)
    	throw(nidas::util::IOException);

#ifdef OLD_HEADER_WAY
    void setHeader(const SampleInputHeader& hdr) {
        header = hdr;
    }
#endif

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

    IOChannel* getIOChannel() const { return iochan; }

    void setHeaderSource(HeaderSource* val)
    {
        headerSource = val;
    }

    void setDSMConfig(const DSMConfig* val)
    {
        dsm = val;
    }

    const DSMConfig* getDSMConfig() const
    {
        return dsm;
    }

protected:

    /**
     * Set the IOChannel for this output.
     */
    virtual void setIOChannel(IOChannel* val);

    SampleConnectionRequester* getSampleConnectionRequester()
    {
        return connectionRequester;
    }

    std::string name;

private:

    IOChannel* iochan;

    std::set<const SampleTag*> sampleTags;

    SampleConnectionRequester* connectionRequester;

    dsm_time_t nextFileTime;

#ifdef OLD_HEADER_WAY
    SampleInputHeader header;
#endif

    HeaderSource* headerSource;

    const DSMConfig* dsm;
};

}}	// namespace nidas namespace core

#endif
