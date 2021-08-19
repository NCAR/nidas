// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#ifndef NIDAS_CORE_SAMPLEOUTPUT_H
#define NIDAS_CORE_SAMPLEOUTPUT_H

#include "Sample.h"
#include "SampleClient.h"
#include "Parameter.h"
#include "IOStream.h"
#include "HeaderSource.h"
#include "ConnectionRequester.h"

// #include <nidas/util/McSocket.h>

namespace nidas { namespace core {

class DSMConfig;
class SampleTag;

/**
 * Interface of an output stream of samples.
 */
class SampleOutput: public SampleClient, public IOChannelRequester, public DOMable
{
public:

    virtual ~SampleOutput() {}

    /**
     * Get pointer to SampleOutput that was cloned. Will be NULL
     * if this SampleOutput is an un-cloned original.
     */
    virtual SampleOutput* getOriginal() const = 0;

    virtual void setName(const std::string& val) = 0;

    virtual const std::string& getName() const = 0;

    virtual bool isRaw() const = 0;

    /**
     * Some SampleOutputs don't send out all the Samples that
     * they receive.  At configuration time, one can use
     * this method to request the SampleTags that should be
     * output from a SampleOutput. SampleOutput will own the
     * pointer.
     *
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void addRequestedSampleTag(SampleTag* tag) = 0;

    virtual std::list<const SampleTag*> getRequestedSampleTags() const = 0;

    /**
     * Some SampleOutputs like to be informed of what SampleTags
     * they will be receiving from their SampleSources before they
     * make a connection. Users of SampleOutputs should call
     * this method before calling requestConnection().
     *
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void addSourceSampleTag(const SampleTag* tag) = 0;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void addSourceSampleTags(const std::list<const SampleTag*>& tags) = 0;

    virtual std::list<const SampleTag*> getSourceSampleTags() const = 0;

    /**
     * Request a connection, of this SampleOutput, but don't wait for it.
     * The SampleConnectionRequester will be notified via a call back to
     * SampleConnectionRequester::connected(SampleOutput*,SampleOutput*)
     * where the first SampleOutput points to the SampleOutput
     * of the original request, and the second is often a
     * new instance of a SampleOutput with a new IOChannel connection.
     * Or the two pointers may point to the same SampleOutput.
     *
     * @throws nidas::util::IOException
     **/
    virtual void requestConnection(SampleConnectionRequester*) = 0;

    /**
     * Derived classes implement this to indicate whether a
     * connection should be requested again if one fails.
     * @ return: -1 do not resubmit a connection request
     *      >=0 number of seconds to wait before submitting a request
     *          (0 means ASAP)
     */
    virtual int getReconnectDelaySecs() const = 0;

    virtual void setReconnectDelaySecs(int val) = 0;

    virtual int getFd() const = 0;

    virtual IOChannel* getIOChannel() const = 0;

    /**
     * Plain raw write, typically only used to write an initial
     * header.
     *
     * @throws nidas::util::IOException
     **/
    virtual size_t write(const void* buf, size_t len) = 0;

    /**
     * @throws nidas::util::IOException
     **/
    virtual void close() = 0;

    virtual void setHeaderSource(HeaderSource* val) = 0;

    virtual void setDSMConfig(const DSMConfig* val) = 0;

    virtual const DSMConfig* getDSMConfig() const = 0;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void setLatency(float val) = 0;

    virtual float getLatency() const = 0;

protected:

    virtual SampleOutput* clone(IOChannel* iochannel) = 0;

};

/**
 * Implementation of portions of SampleOutput.
 */
class SampleOutputBase: public SampleOutput
{
public:

    SampleOutputBase();

    /**
     * Create a SampleOutput with a connected IOChannel.
     * @param rqstr: If non-NULL, who to call to notify that
     *     the connection has dropped.
     */
    SampleOutputBase(IOChannel* iochan,SampleConnectionRequester* rqstr=0);

    ~SampleOutputBase();

    /**
     * Set the IOChannel for this SampleOutput.
     * This IOChannel is not yet connected.
     * The IOChannel must be connected before the SampleOutput
     * is used for I/O, using the requestConnection() method,
     * or by IOChannel::connect() followed by
     * SampleOutput::connected(IOChannel*).
     */
    virtual void setIOChannel(IOChannel* val);

    SampleOutput* getOriginal() const
    {
        return _original;
    }

    void setName(const std::string& val) { _name = val; }

    const std::string& getName() const { return _name; }

    bool isRaw() const { return false; }

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void addRequestedSampleTag(SampleTag* tag);

    std::list<const SampleTag*> getRequestedSampleTags() const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void addSourceSampleTag(const SampleTag* tag);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void addSourceSampleTags(const std::list<const SampleTag*>& tags);

    std::list<const SampleTag*> getSourceSampleTags() const;

    /**
     * Request a connection, but don't wait for it.  Requester will be
     * notified via SampleConnectionRequester interface when the connection
     * has been made.  It is not necessary to call this method
     * if a SampleOutput is constructed with a connected IOChannel.
     *
     * @throws nidas::util::IOException
     **/
    void requestConnection(SampleConnectionRequester*);

    /**
     * Implementation of IOChannelRequester::connected().
     * How an IOChannel calls back to a SampleConnectionRequester
     * that it is connected. The ConnectionRequester can then
     * return a clone of itself if the IOChannel is a new
     * instance.
     */
    SampleOutput* connected(IOChannel* ochan);

    /**
     * Base class implementation. If setReconnectDelaySecs() has been
     * called on this SampleOutput, then that value will be returned.
     * Otherwise this will return the value of the IOChannel.
     */
    int getReconnectDelaySecs() const;

    void setReconnectDelaySecs(int val);

    int getFd() const;

    /**
     * @throws nidas::util::IOException
     **/
    void close();

    dsm_time_t getNextFileTime() const { return _nextFileTime; }

    /**
     * @throws nidas::util::IOException
     **/
    void createNextFile(dsm_time_t);

    /**
     * Raw write method, typically used to write the initial
     * header.
     *
     * @throws nidas::util::IOException
     **/
    size_t write(const void* buf, size_t len);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement* node);

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

    size_t getNumDiscardedSamples() const { return _nsamplesDiscarded; }

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

    /**
     * Set desired latency, providing some control
     * over the response time vs buffer efficiency tradeoff.
     * Setting a latency of 1/10 sec means buffer
     * data in the IOStream for a 1/10 sec, then send the data
     * to the physical device.
     * @param val Latency, in seconds.
     *
     * @throws nidas::util::InvalidParameterException
     **/
    void setLatency(float val)
    {
        _latency = val;
    }

    float getLatency() const { return _latency; }

protected:
    /**
     * Protected copy constructor, with a new, connected IOChannel.
     */
    SampleOutputBase(SampleOutputBase&,IOChannel*);

    size_t incrementDiscardedSamples() { return _nsamplesDiscarded++; }

    SampleConnectionRequester* getSampleConnectionRequester()
    {
        return _connectionRequester;
    }

    std::string _name;

    /**
     * Close the IOChannel and notify whoever did the
     * requestConnection that it is time to disconnect,
     * perhaps because of an IOException. This is typically
     * called in the receive() method of a SampleOutput
     * if it gets an IOException when writing data.
     * A common implementation of a connection requester
     * is to delete this SampleOutput after the removing
     * it from the sample stream. So the caller method should
     * immediately return after calling disconnect().
     *
     * @throws nidas::util::IOException
     **/
    void disconnect();

private:

    mutable nidas::util::Mutex _tagsMutex;

    std::list<SampleTag*> _requestedTags;

    std::list<const SampleTag*> _constRequestedTags;

    IOChannel* _iochan;

    SampleConnectionRequester* _connectionRequester;

    dsm_time_t _nextFileTime;

    HeaderSource* _headerSource;

    const DSMConfig* _dsm;

    size_t _nsamplesDiscarded;

    /**
     * Map of parameters by name.
     */
    std::map<std::string,Parameter*> _parameters;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

    std::list<const SampleTag*> _sourceTags;

    /**
     * Pointer to the SampleOutput that I was cloned from.
     */
    SampleOutput* _original;

    float _latency;

    int _reconnectDelaySecs;

    /**
     * No copy.
     */
    SampleOutputBase(const SampleOutputBase&);

    /**
     * No assignment.
     */
    SampleOutputBase& operator=(const SampleOutputBase&);

};

}}	// namespace nidas namespace core

#endif
