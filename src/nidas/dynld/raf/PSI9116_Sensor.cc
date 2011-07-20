/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/raf/PSI9116_Sensor.h>
#include <nidas/core/TCPSocketIODevice.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/core/DSMTime.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/DSMEngine.h>

#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PSI9116_Sensor)

PSI9116_Sensor::PSI9116_Sensor():
	_msecPeriod(0),_nchannels(0),_sampleId(0),
	_psiConvert(68.94757),_sequenceNumber(0),_outOfSequence(0)
{
}

IODevice* PSI9116_Sensor::buildIODevice() throw(n_u::IOException)
{
    TCPSocketIODevice* dev = new TCPSocketIODevice();
    dev->setTcpNoDelay(true);   // don't combine packets
    dev->setKeepAliveIdleSecs(10);
    return dev;
}

string PSI9116_Sensor::sendCommand(const string& cmd,int readlen)
	throw(n_u::IOException)
{

    char ibuf[32];
    DLOG(("sending cmd=") << cmd);

    write(cmd.c_str(),cmd.length());

    if (readlen == 0) readlen = sizeof(ibuf);
    else readlen = std::min(readlen,(signed)sizeof(ibuf));

    size_t res = 0;

    try {
	res = read(ibuf,readlen,MSECS_PER_SEC/4);
    }
    catch (const n_u::IOTimeoutException& e) {
	NLOG((e.what()) << ", cmd=\"" << cmd << "\"");
        return "";
    }
    catch (const n_u::IOException& e) {
	NLOG((e.what()) << ", cmd=\"" << cmd << "\"");
	// throw n_u::IOException(getName(),
	//   string("error responding to \"") + cmd + "\" command");
        return "";
    }
    if (res != 1) {
	ostringstream ost;
	ost << res;
	NLOG((n_u::IOException(getName(),"open",
	    string("not responding to \"") + cmd + "\" command, len=" + ost.str()).what()));
    }

    if (ibuf[0] != 'A') {
	ostringstream ost;
        ost << "len=" << res << ", \"";
        for (unsigned int i = 0; i < res; i++) {
	    if (isprint(ibuf[i])) ost << ibuf[i];
	    else ost << hex << "0x" << (int) ibuf[i];
        }
        ost << "\"";
	NLOG((n_u::IOException(getName(),"open",
	    string("not responding to \"") + cmd + "\" command, response=" + ost.str()).what()));
    }

    return string(ibuf,res);
}

/* start the stream, only read back 1 char, after that it's data */
void PSI9116_Sensor::startStreams() throw(n_u::IOException)
{
    sendCommand("c 01 0",1);
    _sequenceNumber = 0;
}

void PSI9116_Sensor::stopStreams() throw(n_u::IOException)
{
    try {
        sendCommand("c 02 0");
    }
    catch (const n_u::IOException& e) {
    }

    char ibuf[32];

    for (int i = 0; i < 50; i++) {
        // cerr << "looking for timeout" << endl;
	try {
	    read(ibuf,sizeof(ibuf),MSECS_PER_SEC/10);
	}
	catch (const n_u::IOTimeoutException& e) {
	    DLOG(("got timeout"));
	    break;
	}
	catch (const n_u::IOException& e) {
	    DLOG(("got IOException: ") << e.what());
	    break;
	}
    }
}

void PSI9116_Sensor::open(int flags)
        throw(n_u::IOException,n_u::InvalidParameterException)
{
    // Update the message length based on number of channels requested.
    // The PSI sends a one-byte stream number (binary 1), followed by a
    // 4-byte big-endian sequence number, a 4-byte little-endian float
    // for each configured channel, and (an undocumented, big-endian)
    // 2-byte total sample length value at the end.
    // We use the initial 0x1 as the message separator, so the message length
    // does not include it.
    setMessageParameters((_nchannels + 1) * sizeof(int) + 2,
                getMessageSeparator(),getMessageSeparatorAtEOM());

    CharacterSensor::open(flags);

    stopStreams();

    sendCommand("A");

    ostringstream cmdstream;
    cmdstream << "v01101 " << _psiConvert;
    sendCommand(cmdstream.str());

    int stream = 1;	// stream 1,2 or 3
    int sync = 1;	// 0=hardware trigger, 1=software clock
    int format = 8;	// 8=binary little-endian floats
    int nsamples = 0;	// 0=continuous

    // Build "c 00" command to send

    // hex bit value selecting desired channels
    unsigned int chans = 0;
    for (int i = 0; i < _nchannels; i++) chans = chans * 2 + 1;

    cmdstream.str("");
    cmdstream.clear();
    cmdstream << "c 00 " << stream << ' ' <<
    	hex << setw(4) << setfill('0') << chans << ' ' <<
        dec << setfill(' ') << sync << ' ' <<
	_msecPeriod <<  ' ' <<
	format <<  ' ' <<
	nsamples;

    sendCommand(cmdstream.str());

    startStreams();

    // parse inet::hostname:port so we can get the hostname
    string devname = getDeviceName();
    int addrtype;
    string hostname;
    int port;
    SocketIODevice::parseAddress(devname,addrtype,hostname,port);

    DSMEngine::getInstance()->registerSensorWithXmlRpc(hostname,this);
}

/* Stop data streams, set valve position to PURGE */
void PSI9116_Sensor::startPurge() throw(n_u::IOException)
{
    // flip valves to PURGE (via LEAK/CHECK)
    stopStreams();
    sendCommand("w1201",1);
    sendCommand("w0C01",1);
}

/* Set valve position back to RUN from PURGE, then restart data streams */
void PSI9116_Sensor::stopPurge() throw(n_u::IOException)
{
    // flip valves back to RUN (via LEAK/CHECK)
    sendCommand("w0C00",1);
    sendCommand("w1200",1);
    startStreams();
}

void PSI9116_Sensor::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSensor::addSampleTag(stag);

    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException(getName(),
		"sample",
		"current version does not support more than 1 sample");

    
    _sampleId = stag->getId();
    const vector<const Variable*>& vars = stag->getVariables();

    _nchannels = 0;
    for (unsigned int i = 0; i < vars.size(); i++) {
	const Variable* var = vars[i];
	_nchannels += var->getLength();
	
	if (!var->getUnits().compare("mb") ||
	    !var->getUnits().compare("mbar") ||
	    !var->getUnits().compare("hPa"))
		_psiConvert = 68.94757;
	else throw n_u::InvalidParameterException(getName(),
		    "units",
		    string("unknown units: \"") + var->getUnits() + "\"");
    }

    _msecPeriod = (int) rint(MSECS_PER_SEC / stag->getRate());
}

bool PSI9116_Sensor::process(const Sample* samp,list<const Sample*>& results)
	throw()
{
    assert(samp->getType() == CHAR_ST);
    int slen = samp->getDataLength();

    const char* input = (const char*) samp->getConstVoidDataPtr();

    if (slen < 1 || *input++ != 1) return false;
    if (slen < 5) return false;

    union flip {
	unsigned int lval;
	float fval;
	char bytes[4];
    } seqnum;

    // sequence number is big endian.
#if __BYTE_ORDER == __BIG_ENDIAN
    ::memcpy(&seqnum,input,sizeof(seqnum));
    input += 4;
#else
    seqnum.bytes[3] = *input++;
    seqnum.bytes[2] = *input++;
    seqnum.bytes[1] = *input++;
    seqnum.bytes[0] = *input++;
#endif

    if (_sequenceNumber == 0) _sequenceNumber = seqnum.lval;
    else if (seqnum.lval != ++_sequenceNumber) {
        if (!(_outOfSequence++ % 1000))
		n_u::Logger::getInstance()->log(LOG_WARNING,
    "%d out of sequence samples from %s, num=%u, expected=%u,slen=%d",
		    _outOfSequence,getName().c_str(),seqnum.lval,_sequenceNumber,slen);

	_sequenceNumber = seqnum.lval;
        return false;
    }

    int nvalsin = (slen - 5) / sizeof(float);
    if (nvalsin > _nchannels) nvalsin = _nchannels;

    SampleT<float>* outs = getSample<float>(_nchannels);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(_sampleId);
    float* dout = outs->getDataPtr();

    // data values in format 8 are little endian floats
    int iout;
    for (iout = 0; iout < nvalsin; iout++) {
#if __BYTE_ORDER == __BIG_ENDIAN
	union flip d;
	seqnum.bytes[3] = *input++;
	seqnum.bytes[2] = *input++;
	seqnum.bytes[1] = *input++;
	seqnum.bytes[0] = *input++;
	dout[iout] = d.fval;
#else
        ::memcpy(dout+iout,input,4);
	input += 4;
#endif
    }
    for ( ; iout < _nchannels; iout++) dout[iout] = floatNAN;

    results.push_back(outs);
    return true;
}

void PSI9116_Sensor::executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
        throw()
{
    string action = "null";
    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct) {
        action = string(params["action"]);
    }
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray) {
        action = string(params[0]["action"]);
    }

    try {
        if (action == "startPurge") startPurge();
        else if (action == "stopPurge") stopPurge();
        else {
            string errmsg = "XmlRpc error: " + getName() + ": no such action " + action;
            PLOG(("") << errmsg);
            result = errmsg;
            return;
        }
    }
    catch(const nidas::util::IOException& e) {
        string errmsg = "XmlRpc error: " + action + ": " + getName() + ": " + e.what();
        PLOG(("") << errmsg);
        result = errmsg;
        return;
    }
    result = string("Success");
}
