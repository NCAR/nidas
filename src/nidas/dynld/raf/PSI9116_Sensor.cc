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

#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PSI9116_Sensor)

PSI9116_Sensor::PSI9116_Sensor():
	msecPeriod(0),nchannels(0),sampleId(0),
	psiConvert(68.94757),sequenceNumber(0),outOfSequence(0)
{
}

IODevice* PSI9116_Sensor::buildIODevice() throw(n_u::IOException)
{
    TCPSocketIODevice* dev = new TCPSocketIODevice();
    dev->setTcpNoDelay(true);
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
    CharacterSensor::open(flags);

    stopStreams();

    sendCommand("A");

    ostringstream cmdstream;
    cmdstream << "v01101 " << psiConvert;
    sendCommand(cmdstream.str());

    int stream = 1;	// stream 1,2 or 3
    int sync = 1;	// 0=hardware trigger, 1=software clock
    int format = 8;	// 8=binary little-endian floats
    int nsamples = 0;	// 0=continuous

    unsigned int chans = 0;
    for (int i = 0; i < nchannels; i++) chans = chans * 2 + 1;

    cmdstream.str("");
    cmdstream.clear();
    cmdstream << "c 00 " << stream << ' ' <<
    	hex << setw(4) << setfill('0') <<
    	chans << dec << setfill(' ') << ' ' <<
	sync << ' ' <<
	msecPeriod <<  ' ' <<
	format <<  ' ' <<
	nsamples;

    sendCommand(cmdstream.str());

    startStreams();

    sequenceNumber = 0;
}

/* Set valve position to PURGE for a given number of milliseconds */
void PSI9116_Sensor::purge(int msec) throw(n_u::IOException)
{
    // flip valves to PURGE (via LEAK/CHECK)
    stopStreams();
    sendCommand("w1201",1);
    sendCommand("w0C01",1);

    struct timespec nsleep = {
	msec / MSECS_PER_SEC,
	(msec % MSECS_PER_SEC) * NSECS_PER_MSEC};
    nanosleep(&nsleep,0);

    // flip valves back to RUN (via LEAK/CHECK)
    sendCommand("w0C00",1);
    sendCommand("w1200",1);
    startStreams();

    sequenceNumber = 0;
}

void PSI9116_Sensor::addSampleTag(SampleTag* stag)
	throw(n_u::InvalidParameterException)
{
    DSMSensor::addSampleTag(stag);

    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException(getName(),
		"sample",
		"current version does not support more than 1 sample");

    
    sampleId = stag->getId();
    const vector<const Variable*>& vars = stag->getVariables();

    nchannels = 0;
    for (unsigned int i = 0; i < vars.size(); i++) {
	const Variable* var = vars[i];
	nchannels += var->getLength();
	
	if (!var->getUnits().compare("mb") ||
	    !var->getUnits().compare("mbar") ||
	    !var->getUnits().compare("hPa"))
		psiConvert = 68.94757;
	else throw n_u::InvalidParameterException(getName(),
		    "units",
		    string("unknown units: \"") + var->getUnits() + "\"");
    }

    msecPeriod = (int) rint(MSECS_PER_SEC / stag->getRate());
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
	unsigned long lval;
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

    if (sequenceNumber == 0) sequenceNumber = seqnum.lval;
    else if (seqnum.lval != ++sequenceNumber) {
        if (!(outOfSequence++ % 1000))
		n_u::Logger::getInstance()->log(LOG_WARNING,
    "%d out of sequence samples from %s, num=%u, expected=%u,slen=%d",
		    outOfSequence,getName().c_str(),seqnum.lval,sequenceNumber,slen);

	sequenceNumber = seqnum.lval;
    }

    int nvalsin = (slen - 5) / sizeof(float);
    if (nvalsin > nchannels) nvalsin = nchannels;

    SampleT<float>* outs = getSample<float>(nchannels);
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(sampleId);
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
    for ( ; iout < nchannels; iout++) dout[iout] = floatNAN;

    results.push_back(outs);
    return true;
}
