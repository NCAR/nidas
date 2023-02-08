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

#ifndef NIDAS_DYNLD_ISFF_NETCDFRPCCHANNEL_H
#define NIDAS_DYNLD_ISFF_NETCDFRPCCHANNEL_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Parameter.h>

#include <nc_server_rpc.h>

#include <string>
#include <iostream>
#include <vector>

namespace nidas { namespace dynld { namespace isff {

class NcVarGroupFloat;

/**
 * A perversion of a simple IOChannel.  This sends data
 * to a nc_server via RPC calls.
 */
class NetcdfRPCChannel: public nidas::core::IOChannel {

public:
    using IOChannelRequester = nidas::core::IOChannelRequester;
    using IOChannel = nidas::core::IOChannel;
    using SampleTag = nidas::core::SampleTag;
    template <typename T>
    using ParameterT = nidas::core::ParameterT<T>;
    using dsm_sample_id_t = nidas::core::dsm_sample_id_t;

    /**
     * Constructor.
     */
    NetcdfRPCChannel();

    /**
     * Destructor.
     */
    ~NetcdfRPCChannel();

    /**
     * Clone invokes default copy constructor.
     */
    NetcdfRPCChannel* clone() const { return new NetcdfRPCChannel(*this); }

    /**
     * Request a connection.
     */
    void requestConnection(IOChannelRequester* rqstr);

    /**
     * 
     */
    IOChannel* connect();

    void setNonBlocking(bool val __attribute__ ((unused)))
    {
        // ignore for now.
    }

    bool isNonBlocking() const override
    {
        return true;
    }

    virtual bool isNewFile() const { return false; }

    /**
     * Basic read is not implemented. Always throws IOException.
     */
    size_t read(void*, size_t);

    /**
     * Basic write is not implemented. Always throws IOException.
     */
    size_t write(const void*, size_t);

    /**
     * Basic write is not implemented. Always throws IOException.
     */
    size_t write(const struct iovec*, int);

    /**
     * Send a data record to the RPC server.
    */
    void write(const nidas::core::Sample*);

    /**
     * Send a data record to the RPC server.
    */
    void write(datarec_float*);

    void close();

    int getFd() const
    {
        return -1;
    }

    const std::string& getName() const
    {
        return _name;
    }

    void setName(const std::string& val);

    const std::string& getServer() const { return _server; }

    void setServer(const std::string& val);

    const std::string& getFileNameFormat() const { return _fileNameFormat; }

    void setFileNameFormat(const std::string& val);

    const std::string& getDirectory() const { return _directory; }

    void setDirectory(const std::string& val);

    const std::string& getCDLFileName() const { return _cdlFileName; }

    void setCDLFileName(const std::string& val) { _cdlFileName = val; }

    void setFillValue(float val) { _fillValue = val; }

    float getFillValue() const { return _fillValue; }

    /**
     * DeltaT in seconds for the time variable in the NetCDF file.
     * Common value is 300 seconds.
     */
    void setTimeInterval(int val)
    {
        _timeInterval = val;
    }

    int getTimeInterval() const
    {
        return _timeInterval;
    }

    /**
     * File length, in seconds.
     */
    int getFileLength() const { return _fileLength; }

    void setFileLength(int val) { _fileLength = val; }

    /**
     * Do an RPC call to fetch the last error string from the
     * nc_server.
     */
    void checkError();

    void setRPCTimeout(int secs);

    int getRPCTimeout() const;

    /**
     * Batch requests to nc_server for this length of time in seconds.
     */
    void setRPCBatchPeriod(int val);

    int getRPCBatchPeriod() const;

    void fromDOMElement(const xercesc::DOMElement* node);

    /**
     * Add a sample tag. This NetcdfRPCChannel will send samples
     * of this type to the netcdf server.
     */
    void addSampleTag(const SampleTag*);

    std::list<const SampleTag*> getSampleTags() const
    {
        return _constSampleTags;
    }

    void writeGlobalAttr(const std::string& name, const std::string& value);

    void writeGlobalAttr(const std::string& name, int value);

private:

    friend class NcVarGroupFloat;

    CLIENT* getRPCClient() { return _clnt; }

    int getConnectionId() const { return _connectionId; }

    struct timeval& getRPCWriteTimeoutVal();

    struct timeval& getRPCOtherTimeoutVal();

    struct timeval& getRPCBatchTimeoutVal();

protected:

    /**
     * Copy constructor.
     */
    NetcdfRPCChannel(const NetcdfRPCChannel&);

    void writeHistory(const std::string&);

    void nonBatchWrite(datarec_float*);

    NcVarGroupFloat* getNcVarGroupFloat(
    	const std::vector<ParameterT<int> >& dims,
		const SampleTag* stag);

private:

    std::string _name;

    std::string _server;

    /**
     * file name, typically containing date format descriptors.
     */
    std::string _fileNameFormat;

    std::string _directory;

    std::string _cdlFileName;

    float _fillValue;

    int _fileLength;

    CLIENT* _clnt;

    /**
     * Connection token returned by nc_server.
     */
    int _connectionId;

    int _rpcBatchPeriod;

    struct timeval _rpcWriteTimeout;

    struct timeval _rpcOtherTimeout;

    struct timeval _rpcBatchTimeout;

    int _ntry;

    static const int NTRY = 10;

    time_t _lastNonBatchWrite;

    std::map<dsm_sample_id_t, NcVarGroupFloat*> _groupById;

    std::map<dsm_sample_id_t, int> _stationIndexById;

    std::list<NcVarGroupFloat*> _groups;
    
    std::list<SampleTag*> _sampleTags;

    std::list<const SampleTag*> _constSampleTags;

    /**
     * Deltat in seconds of the time variable in the NetCDF file.
     */
    int _timeInterval;

    /** Assignment not supported. */
    NetcdfRPCChannel& operator=(const NetcdfRPCChannel&);

};

class NcVarGroupFloat {
public:
    NcVarGroupFloat(const std::vector<nidas::core::ParameterT<int> >& dims,
    	const nidas::core::SampleTag* stag,float fillValue);

    ~NcVarGroupFloat();

    const std::vector<const nidas::core::Variable*>& getVariables() const
    {
        return _sampleTag.getVariables();
    }

    const std::vector<nidas::core::ParameterT<int> >& getDimensions()
    {
        return _dimensions;
    }

    /**
     * Return the delta-T of all the variables in this group.
     */
    double getInterval() const { return _interval; }

protected:

    friend class NetcdfRPCChannel;

    void connect(NetcdfRPCChannel* conn,float fillValue);

    void write(NetcdfRPCChannel* conn,const nidas::core::Sample* samp,
               int stationNumber);

private:

    std::vector<nidas::core::ParameterT<int> > _dimensions;

    nidas::core::SampleTag _sampleTag;

    datarec_float _rec;

    int _weightsIndex;

    float _fillValue;

    /**
     * Delta-T of all the variable in this group. Should divide evenly into the
     * _timeInterval of the NetcdfRPCChannel, which is the desired interval of the
     * 'time' variable in the NetCDF files.
     */
    double _interval;

private:

    NcVarGroupFloat(const NcVarGroupFloat&);	// prevent copying

    NcVarGroupFloat& operator =(const NcVarGroupFloat&);// prevent assignment

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
