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
	_psiConvert(68.94757),_sequenceNumber(0),_outOfSequence(0),
        _partialFirst(false), _partialSecond(false), _gotOne(false),
        _prevPartNBytes(0)
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
    // Update the message length based on number of channels requested
    setMessageParameters((_nchannels + 1) * sizeof(int),
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

    int nvalsin;
    SampleT<float>* outs;
    float* dout;
    int bytesTaken = 0;
        int valsTaken = 0;
        int vals2Take = 0; 

    if (_partialFirst || _partialSecond) {
        // we have part of a sample saved from before
        // take as much of the rest from the start of this sample
        if (_partialFirst) {
            dout = _firstPrevious->getDataPtr();
        } else {
            dout = _secondPrevious->getDataPtr(); 
        }

        nvalsin = slen/sizeof(float);

        // See if the previous sample was broken mid value
        if (_prevPartNBytes > 0)
        {
            float* ddout;
            ddout = dout + _nPrevSampVals; 
            for (int bn = _prevPartNBytes; bn < (int)sizeof(float); bn++) {
#if __BYTE_ORDER == __BIG_ENDIAN
	        _prevPartial.bytes[(3-bn)] = *input++;
#else
                _prevPartial.bytes[bn] = *input++;
#endif
                nvalsin--;
                bytesTaken++;
            }
            *ddout  = _prevPartial.fval;
            _prevPartNBytes = 0;
            _prevPartial.fval = 0;
            _nPrevSampVals++;
        }
        int ipout = _nPrevSampVals;
        //int valsTaken = 0;
        if (nvalsin + _nPrevSampVals >= _nchannels) {
            vals2Take = _nchannels;
            _nPrevSampVals = 0;
        } else {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                    "%s Unexpected situation of two in samples who don't create an out sample",
                    getName().c_str());
            vals2Take = nvalsin + _nPrevSampVals;
            _nPrevSampVals = vals2Take;
        }
        for ( ; ipout < vals2Take; ipout++) {
#if __BYTE_ORDER == __BIG_ENDIAN
	    union flip d;
	    d.bytes[3] = *input++;
	    d.bytes[2] = *input++;
	    d.bytes[1] = *input++;
	    d.bytes[0] = *input++;
	    dout[ipout] = d.fval;
#else
            ::memcpy(dout+ipout,input,4);
            input += 4;
#endif
            valsTaken++;
        }
        // Do we have at least one full sample?
        if (vals2Take == _nchannels) {
            if (_partialFirst) 
                results.push_back(_firstPrevious);
            else if (_partialSecond) 
                results.push_back(_secondPrevious);
            _gotOne = true;
            *input++;*input++; // skip size indicator bytes

            slen = slen - (valsTaken*sizeof(float)) - bytesTaken - 2; // -2 for size indicator bytes
            nvalsin = (slen - 5) / sizeof(float); 
            if (nvalsin > _nchannels) {
                // More than two full samples - should not see this
                n_u::Logger::getInstance()->log(LOG_WARNING,
                "More than 2 out samples found in %s, vt=%u,slen=%u,nvalsin=%u",
                 getName().c_str(),valsTaken,slen,nvalsin);
                _gotOne = _partialFirst = _partialSecond = false; // Reset 
                return true;  
            }
        } else
            return false; // We still only have a partial sample

    } else 
        // Should be at the beginning of a new sample
        nvalsin = (slen - 5) / sizeof(float);

    // eliminate any REALLY incomplete records
    if ((slen < 1 || *input++ != 1) || slen < 5) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
                    "Very incomplete sample from %s, slen=%u, inp=%d, vt=%u", 
                    getName().c_str(),slen,*(input-1),valsTaken);
        if (_partialFirst && _gotOne) {
            _partialFirst = _gotOne = false;
            return true;
        } else if (_partialSecond && _gotOne) {
            _partialSecond = _gotOne = false;
            return true;
        } else return false;
    }

    // sequence number is big endian.
    union flip seqnum;
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
        if (!(_outOfSequence++ % 100)) 
		n_u::Logger::getInstance()->log(LOG_WARNING,
                    "%d out of sequence samples from %s, num=%u, expected=%u,slen=%d",
                    _outOfSequence,getName().c_str(),seqnum.lval,_sequenceNumber,slen);

	_sequenceNumber = seqnum.lval;
        return false;
    }

    // No history of non-split samples that are too big, but just in case
    if (nvalsin > _nchannels) nvalsin = _nchannels;

    outs = getSample<float>(_nchannels);

    // If we're in the middle of an input sample then we
    // need to fudge the time of the next output sample
    if (_gotOne) {
        if (_partialFirst) {
            outs->setTimeTag(_firstPrevious->getTimeTag() 
                               + (dsm_time_t) _msecPeriod*MSECS_PER_SEC);
            _secondPrevious = outs;
            _partialSecond = true; //Might be complete we'll check later
            _partialFirst = false; // It's been sent
        } else if (_partialSecond) {
            outs->setTimeTag(_secondPrevious->getTimeTag() 
                               + (dsm_time_t) _msecPeriod*MSECS_PER_SEC);
            _firstPrevious = outs;
            _partialFirst = true;   // Might be complete we'll check later
            _partialSecond = false; // It's been sent
        } else {
            // Should not get here
            n_u::Logger::getInstance()->log(LOG_WARNING,
                  "Bad logic from %s", getName().c_str());
        }
    } else {
        outs->setTimeTag(samp->getTimeTag());
        _firstPrevious = outs;  
        _partialFirst = true;
    }

    outs->setId(_sampleId);
    dout = outs->getDataPtr();

    // data values in format 8 are little endian floats
    int iout;
    for (iout = 0; iout < nvalsin; iout++) {
#if __BYTE_ORDER == __BIG_ENDIAN
	union flip d;
	d.bytes[3] = *input++;
	d.bytes[2] = *input++;
	d.bytes[1] = *input++;
	d.bytes[0] = *input++;
	dout[iout] = d.fval;
#else
        ::memcpy(dout+iout,input,4);
	input += 4;
#endif
        _nPrevSampVals++;
    }
    if (iout < _nchannels) {
        // Partial sample - check to see if it broke mid-value
        if ((slen-1) % sizeof(float)) {
            _prevPartNBytes = (slen - 1) % sizeof(float); 

       bytesTaken = 0;

            for (int bn = 0; bn < _prevPartNBytes; bn++) {
                bytesTaken++;
#if __BYTE_ORDER == __BIG_ENDIAN
	        _prevPartial.bytes[(3-bn)] = *input++;
#else
                _prevPartial.bytes[bn] = *input++;
#endif
            }
        }

        // we should be good to go just check if we had a full sample before
        if (_gotOne) {
            _gotOne = false;
            return true;
        } else
            return false;
    }

    _gotOne = _partialFirst = _partialSecond = false; // Reset 
    _nPrevSampVals = 0;
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
