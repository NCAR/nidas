/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_NETCDFRPCCHANNEL_H
#define NIDAS_DYNLD_ISFF_NETCDFRPCCHANNEL_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>

// #include <rpc/rpc.h>

#include <nidas/dynld/isff/nc_server_rpc.h>

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
     * Copy constructor.
     */
    NetcdfRPCChannel(const NetcdfRPCChannel&);

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
    void requestConnection(ConnectionRequester* rqstr)
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
        return name;
    }

    void setName(const std::string& val);

    const std::string& getServer() const { return server; }

    void setServer(const std::string& val);

    const std::string& getFileNameFormat() const { return fileNameFormat; }

    void setFileNameFormat(const std::string& val);

    const std::string& getDirectory() const { return directory; }

    void setDirectory(const std::string& val);

    const std::string& getCDLFileName() const { return cdlFileName; }

    void setCDLFileName(const std::string& val) { cdlFileName = val; }

    void setFillValue(float val) { fillValue = val; }

    float getFillValue() const { return fillValue; }

    /**
     * File length, in seconds.
     */
    const int getFileLength() const { return fileLength; }

    void setFileLength(int val) { fileLength = val; }

    void flush() throw(nidas::util::IOException);

    void setRPCTimeout(int secs);

    int getRPCTimeout() const;

    void setRPCBatchPeriod(int val);

    int getRPCBatchPeriod() const;

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);
    
private:

    friend class NcVarGroupFloat;

    CLIENT* getRPCClient() { return clnt; }

    int getConnectionId() const { return connectionId; }

    struct timeval& getRPCWriteTimeoutVal();

    struct timeval& getRPCOtherTimeoutVal();

    struct timeval& getRPCBatchTimeoutVal();

protected:

    void writeHistory(const std::string&) throw (nidas::util::IOException);

    void nonBatchWrite(datarec_float*) throw (nidas::util::IOException);

    NcVarGroupFloat* getNcVarGroupFloat(
    	const std::vector<ParameterT<int> >& dims,
		const SampleTag* stag);

private:

    std::string name;

    std::string server;

    /** file name, usually contains date format descriptors, see man cftime */
    std::string fileNameFormat;

    std::string directory;

    std::string cdlFileName;

    float fillValue;

    int fileLength;

    CLIENT* clnt;

    int connectionId;

    int rpcBatchPeriod;

    struct timeval rpcWriteTimeout;

    struct timeval rpcOtherTimeout;

    struct timeval rpcBatchTimeout;

    int ntry;

    static const int NTRY = 10;

    time_t lastFlush;

    std::map<dsm_sample_id_t,NcVarGroupFloat*> groupById;

    std::map<dsm_sample_id_t,int> stationIndexById;

    std::list<NcVarGroupFloat*> groups;

};

class NcVarGroupFloat {
public:
    NcVarGroupFloat(const std::vector<ParameterT<int> >& dims,
    	const SampleTag* stag,float fillValue);
    
    ~NcVarGroupFloat();

    const std::vector<const Variable*>& getVariables()
    {
        return sampleTag.getVariables();
    }

    const std::vector<ParameterT<int> >& getDimensions()
    {
        return dimensions;
    }

protected:

    friend class NetcdfRPCChannel;

    void connect(NetcdfRPCChannel* conn,float fillValue)
	  throw(nidas::util::IOException);

    void write(NetcdfRPCChannel* conn,const Sample* samp,
    	int stationNumber) throw(nidas::util::IOException);

private:

    std::vector<ParameterT<int> > dimensions;

    SampleTag sampleTag;

    datarec_float rec;

    int weightsIndex;

    float fillValue;

private:

    NcVarGroupFloat(const NcVarGroupFloat&);	// prevent copying

    NcVarGroupFloat& operator =(const NcVarGroupFloat&);// prevent assignment

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
