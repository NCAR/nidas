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
#include <SampleInputHeader.h>
#include <SampleClient.h>
#include <SampleSorter.h>
#include <IOStream.h>
#include <ConnectionRequester.h>

// #include <atdUtil/McSocket.h>

namespace dsm {

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
     * Requester will be notified via
     * SampleConnectionRequester::connected(SampleOutput*)
     * method when the connection has been made.  The SampleOutput
     * returned by SampleConnectionRequester::connected(SampleOutput*)
     * can be the same or a another instance of a SampleOutput.
     */
    virtual void requestConnection(SampleConnectionRequester*)
    	throw(atdUtil::IOException) = 0;

    /**
     * Request a connection, and wait for it.
     */
    virtual void connect() throw(atdUtil::IOException) = 0;

    virtual void disconnect() throw(atdUtil::IOException) = 0;

    virtual void connected(IOChannel* ioc) throw() = 0;

    virtual int getFd() const = 0;

    virtual void init() throw(atdUtil::IOException) = 0;

    /**
     * Plain raw write, typically only used to write an initial
     * header.
     */
    virtual void write(const void* buf, size_t len)
    	throw(atdUtil::IOException) = 0;

    virtual void close() throw(atdUtil::IOException) = 0;

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
                 throw(atdUtil::IOException);

    void connected(IOChannel* output) throw();

    /**
     * Request a connection, and wait for it.
     */
    void connect() throw(atdUtil::IOException);

    void disconnect() throw(atdUtil::IOException);

    int getFd() const;

    void init() throw();

    void close() throw(atdUtil::IOException);

    dsm_time_t getNextFileTime() const { return nextFileTime; }

    void createNextFile(dsm_time_t) throw(atdUtil::IOException);

    /**
     * Raw write method, typically used to write the initial
     * header.
     */
    void write(const void* buf, size_t len)
    	throw(atdUtil::IOException);

    void setHeader(const SampleInputHeader& hdr) {
        header = hdr;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);
                                                                                
    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    /**
     * Set the IOChannel for this output.
     */
    virtual void setIOChannel(IOChannel* val);

    IOChannel* getIOChannel() { return iochan; }

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

    SampleInputHeader header;

};

/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleOutputBase
{
public:

    SampleOutputStream(IOChannel* iochan=0);

    /**
     * Copy constructor.
     */
    SampleOutputStream(const SampleOutputStream&);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SampleOutputStream(const SampleOutputStream&,IOChannel*);

    virtual ~SampleOutputStream();

    SampleOutputStream* clone(IOChannel* iochannel=0) const;

    /**
     * Get the IOStream of this SampleOutputStream.
     * SampleOutputStream owns the pointer and
     * will delete the IOStream in its destructor.
     * The IOStream is available after the
     * call to init() and before close() (or the destructor).
     */
    IOStream* getIOStream() { return iostream; }

    void init() throw();

    void close() throw(atdUtil::IOException);

    bool receive(const Sample *s) throw();

    void finish() throw();

    void write(const void* buf, size_t len)
    	throw(atdUtil::IOException);

protected:

    bool write(const Sample* samp) throw(atdUtil::IOException);

    IOStream* iostream;

private:

    size_t nsamplesDiscarded;
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

    /**
     * Copy constructor, with a new IOChannel.
     */
    SortedSampleOutputStream(const SortedSampleOutputStream&,IOChannel*);

    virtual ~SortedSampleOutputStream();

    SortedSampleOutputStream* clone(IOChannel* iochannel=0) const;

    void init() throw();

    bool receive(const Sample *s) throw();

    /**
     * Set length of SampleSorter, in milliseconds.
     */
    void setSorterLengthMsecs(int val)
    {
        sorterLengthMsecs = val;
    }

    int getSorterLengthMsecs() const
    {
        return sorterLengthMsecs;
    }

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);


protected:

    SampleSorter* sorter;

    SampleOutputStreamProxy proxy;

    /**
     * Length of SampleSorter, in milli-seconds.
     */
    int sorterLengthMsecs;

private:
};

}

#endif
