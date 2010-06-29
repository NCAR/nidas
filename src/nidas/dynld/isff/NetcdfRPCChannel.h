/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifdef HAS_NC_SERVER_RPC_H

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

using namespace nidas::core;

class NcVarGroupFloat;

/**
 * A perversion of a simple IOChannel.  This sends data
 * to a nc_server via RPC calls.
 */
class NetcdfRPCChannel: public IOChannel {

public:

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
    void requestConnection(IOChannelRequester* rqstr)
    	throw(nidas::util::IOException);

    /**
     * 
     */
    IOChannel* connect() throw(nidas::util::IOException);

    virtual bool isNewFile() const { return false; }

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"read","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"default write","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"default write","not supported");
    }

    /**
     * Send a data record to the RPC server.
    */
    void write(const Sample*) throw (nidas::util::IOException);

    /**
     * Send a data record to the RPC server.
    */
    void write(datarec_float*) throw (nidas::util::IOException);

    void close() throw (nidas::util::IOException);

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
    const int getFileLength() const { return _fileLength; }

    void setFileLength(int val) { _fileLength = val; }

    void flush() throw(nidas::util::IOException);

    void setRPCTimeout(int secs);

    int getRPCTimeout() const;

    void setRPCBatchPeriod(int val);

    int getRPCBatchPeriod() const;

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    /**
     * Add a sample tag. This NetcdfRPCChannel will send samples
     * of this type to the netcdf server.  NetcdfRPCChannel does not
     * own the SampleTag.
     */
    void addSampleTag(const SampleTag*);

    std::list<const SampleTag*> getSampleTags() const
    {
        return _sampleTags;
    }

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


    void writeHistory(const std::string&) throw (nidas::util::IOException);

    void nonBatchWrite(datarec_float*) throw (nidas::util::IOException);

    NcVarGroupFloat* getNcVarGroupFloat(
    	const std::vector<ParameterT<int> >& dims,
		const SampleTag* stag);

private:

    std::string _name;

    std::string _server;

    /** file name, usually contains date format descriptors, see man cftime */
    std::string _fileNameFormat;

    std::string _directory;

    std::string _cdlFileName;

    float _fillValue;

    int _fileLength;

    CLIENT* _clnt;

    int _connectionId;

    int _rpcBatchPeriod;

    struct timeval _rpcWriteTimeout;

    struct timeval _rpcOtherTimeout;

    struct timeval _rpcBatchTimeout;

    int _ntry;

    static const int NTRY = 10;

    time_t _lastFlush;

    std::map<dsm_sample_id_t,NcVarGroupFloat*> _groupById;

    std::map<dsm_sample_id_t,int> _stationIndexById;

    std::list<NcVarGroupFloat*> _groups;
    
    std::list<const SampleTag*> _sampleTags;

    /**
     * Deltat in seconds of the time variable in the NetCDF file.
     */
    int _timeInterval;

};

class NcVarGroupFloat {
public:
    NcVarGroupFloat(const std::vector<ParameterT<int> >& dims,
    	const SampleTag* stag,float fillValue);
    
    ~NcVarGroupFloat();

    const std::vector<const Variable*>& getVariables()
    {
        return _sampleTag.getVariables();
    }

    const std::vector<ParameterT<int> >& getDimensions()
    {
        return _dimensions;
    }

protected:

    friend class NetcdfRPCChannel;

    void connect(NetcdfRPCChannel* conn,float fillValue)
	  throw(nidas::util::IOException);

    void write(NetcdfRPCChannel* conn,const Sample* samp,
    	int stationNumber) throw(nidas::util::IOException);

private:

    std::vector<ParameterT<int> > _dimensions;

    SampleTag _sampleTag;

    datarec_float _rec;

    int _weightsIndex;

    float _fillValue;

private:

    NcVarGroupFloat(const NcVarGroupFloat&);	// prevent copying

    NcVarGroupFloat& operator =(const NcVarGroupFloat&);// prevent assignment

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
#endif  // HAS_NC_SERVER_RPC_H
