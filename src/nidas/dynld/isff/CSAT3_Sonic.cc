/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/CSAT3_Sonic.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <sstream>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CSAT3_Sonic)

CSAT3_Sonic::CSAT3_Sonic():
	windInLen(12),	// two bytes each for u,v,w,tc,diag, and 0x55aa
	totalInLen(12),
	windNumOut(0),
	spdIndex(-1),
	dirIndex(-1),
	spikeIndex(-1),
	windSampleId(0),
	nttsave(-2),
	counter(0),
        _rate(0),
        _oversample(false)
{
    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}

CSAT3_Sonic::~CSAT3_Sonic()
{
}

void CSAT3_Sonic::stopSonic() throw(n_u::IOException)
{
    setMessageLength(1);
    setMessageSeparator("");
    setMessageParameters(); // does the ioctl

    clearBuffer();
    for (int i = 0; i < 10; i++) {
        DLOG(("%s: sending &",getName().c_str()));
        write("&",1);
        // clear whatever junk may be in the buffer til a timeout
        try {
            for (int i = 0; i < 10; i++) {
                readBuffer(MSECS_PER_SEC + 10);
                clearBuffer();
            }
        }
        catch (const n_u::IOTimeoutException& e) {
            DLOG(("%s: timeout",getName().c_str()));
            break;
        }
    }
}

void CSAT3_Sonic::startSonic() throw(n_u::IOException)
{
    DLOG(("%s: sending D (nocr)",getName().c_str()));
    write("D",1);
    clearBuffer();

    for (int i = 0; i < 5; i++) {
        DLOG(("%s: sending &",getName().c_str()));
        write("&",1);
        try {
            readBuffer(MSECS_PER_SEC);
            int nsamp = 0;
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {
                distributeRaw(samp);
                nsamp++;
            }
            if (nsamp > 0) break;
        }
        catch (const n_u::IOTimeoutException& e) {
        }
        DLOG(("%s: sending D",getName().c_str()));
        write("D",1);
    }
}

string CSAT3_Sonic::querySonic(int &acqrate,char &osc, string& serialNumber, string& revision)
    throw(n_u::IOException)
{
    string result;

    acqrate = 0;
    osc = ' ';
    serialNumber = "unknown";

    for (int j = 0; j < 5; j++) {
        DLOG(("%s: sending ?? CR",getName().c_str()));
        write("??\r",3);    // must send CR
        int timeout = MSECS_PER_SEC * 2;
        for (;;) {
            try {
                readBuffer(timeout);
            }
            catch (const n_u::IOTimeoutException& e) {
                DLOG(("%s: timeout",getName().c_str()));
                break;
            }
            for (Sample* samp = nextSample(); samp; samp = nextSample()) {
                distributeRaw(samp);
                int l = samp->getDataByteLength();
                // strings will not be null terminated
                const char * cp = (const char*)samp->getConstVoidDataPtr();
                if (result.length() == 0)
                    while (*cp && (*cp == 'T' || ::isspace(*cp))) { cp++; l--; }
                result += string(cp,l);
            }
            timeout = MSECS_PER_SEC;
        }
        if (result.length() > 100) break;
    }
    clearBuffer();

    // Version 3 output starts with "ET=", version 4 with "SNXXXX"
    string::size_type fs = std::min(result.find("ET="),result.find("SN"));
    if (fs != string::npos && fs > 0) result = result.substr(fs);
    while (result[result.length() - 1] == '>') result.resize(result.length()-1);

    unsigned int ql = result.length();
    DLOG(("%s: query=",getName().c_str()) << n_u::addBackslashSequences(result) << " result length=" << ql);

    // find and get AQ parameter, e.g. AQ=1.0 (raw sampling rate)
    fs = result.find("AQ=");
    if (fs != string::npos && fs + 3 < ql)
        acqrate = atoi(result.substr(fs+3).c_str());

    // get os parameter, e.g. "os=g"
    // g for 10Hz 6x oversampling, h for 20Hz 3x oversampling, ' ' otherwise
    // For version 4, os=0 means no oversampling
    fs = result.find("os=");
    if (fs != string::npos && fs + 3 < ql) osc = result[fs+3];
            
    // get serial number, e.g. "SN1124" (hopefully is the only string with "SN")
    fs = result.find("SN");
    string::size_type bl = result.find(' ',fs);
    if (fs != string::npos && bl != string::npos)
        serialNumber = result.substr(fs,bl-fs);

    // get software revision, e.g. "rev 3.0f"
    fs = result.find("rev");
    if (fs != string::npos && fs + 4 < ql) {
        bl = result.find(' ',fs+4);
        revision = result.substr(fs+4,bl-fs-4);
    }
    return result;
}

string CSAT3_Sonic::sendRateCommand(const char* cmd)
    throw(n_u::IOException)
{
    DLOG(("%s: sending %s (nocr)",getName().c_str(),cmd));
    write(cmd,2);
    int timeout = MSECS_PER_SEC * 4;

    string result;
    for (int i = 0; ; i++) {
        try {
            readBuffer(timeout);
        }
        catch (const n_u::IOTimeoutException& e) {
            DLOG(("%s: timeout",getName().c_str()));
            break;
        }
        for (Sample* samp = nextSample(); samp; samp = nextSample()) {
            distributeRaw(samp);
            // strings will not be null terminated
            const char * cp = (const char*)samp->getConstVoidDataPtr();
            result += string(cp,samp->getDataByteLength());
        }
    }
    clearBuffer();
    while (result[result.length() - 1] == '>') result.resize(result.length()-1);
    return result;
}

const char* CSAT3_Sonic::getRateCommand(int rate,bool oversample)
{
    struct acqSigTable {
        int rate;
        bool oversample;
        const char* cmd;
    };
    static const struct acqSigTable acqSigCmds[] = {
        {1,false,"A2"},
        {2,false,"A5"},
        {3,false,"A6"},
        {5,false,"A7"},
        {6,false,"A8"},
        {10,false,"A9"},
        {12,false,"Aa"},
        {15,false,"Ab"},
        {20,false,"Ac"},
        {30,false,"Ad"},
        {60,false,"Ae"},
        {10,true,"Ag"},
        {20,true,"Ah"},
    };

    int nr =  (signed)(sizeof(acqSigCmds)/sizeof(acqSigCmds[0]));
    for (int i = 0; i < nr; i++) {
        if (acqSigCmds[i].rate == rate &&
            (oversample == acqSigCmds[i].oversample)) {
            return acqSigCmds[i].cmd;
        }
    }
    return 0;
}

void CSAT3_Sonic::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    DSMSerialSensor::open(flags);

    const char* rateCmd = 0;
    DLOG(("%s: _rate=%d",getName().c_str(),_rate));
    if (_rate > 0) {
        rateCmd = getRateCommand(_rate,_oversample);
        if (!rateCmd) {
            ostringstream ost;
            ost << "rate=" << _rate << " Hz not supported with oversample=" << _oversample;
            throw n_u::InvalidParameterException(getName(),
                "sample rate",ost.str());
        }
    }


    int ml = getMessageLength();
    string sep = getMessageSeparator();
    bool eom = getMessageSeparatorAtEOM();

    stopSonic();

    setMessageLength(0);
    setMessageSeparator(">");   // sonic prompt
    setMessageSeparatorAtEOM(true);
    setMessageParameters(); // does the ioctl

    // put in "terminal" mode
    DLOG(("%s: sending T (nocr)",getName().c_str()));
    write("T",1);
    usleep(USECS_PER_SEC);

    int acqrate = 0;
    string serialNumber = "unknown";
    char osc = ' ';
    string revision;

    string query = querySonic(acqrate,osc,serialNumber,revision);
    DLOG(("%s: AQ=%d,os=%c,serial number=",getName().c_str(),acqrate,osc) << serialNumber << " rev=" << revision);

    // Is current sonic rate OK?  If requested rate is 0, don't change.
    bool rateOK = _rate == 0;
    if (!_oversample && acqrate == _rate) rateOK = true;
    if (_oversample && acqrate == 60) {
        if (_rate == 10 && osc == 'g') rateOK = true;
        if (_rate == 20 && osc == 'h') rateOK = true;
    }

    string rateResult;
    if (!rateOK) {
        assert(rateCmd != 0);
        rateResult = sendRateCommand(rateCmd);
        query = querySonic(acqrate,osc,serialNumber,revision);
        DLOG(("%s: AQ=%d,os=%c,serial number=",getName().c_str(),acqrate,osc) << serialNumber << " rev=" << revision);
    }

    if ((!rateOK || serialNumber != _serialNumber) && _sonicLogFile.length() > 0) {
        n_u::UTime now;
        string fname = getDSMConfig()->expandString(_sonicLogFile);
        ofstream fst(fname.c_str(),ios_base::out | ios_base::app);
        fst << "csat3: " << getName() << ' ' << serialNumber << ' ' << revision << endl;
        fst << "time: " << now.format(true,"%Y %m %d %H:%M:%S") << endl;
        n_u::trimString(rateResult);
        if (rateResult.length() > 0) fst << rateResult << endl;
        n_u::trimString(query);
        fst << query << endl;
        fst << "##################" << endl;
        if (fst.fail()) ELOG(("%s: write failed",_sonicLogFile.c_str()));
        fst.close();
        _serialNumber = serialNumber;
    }

    setMessageLength(ml);
    setMessageSeparator(sep);
    setMessageSeparatorAtEOM(eom);
    setMessageParameters(); // does the ioctl
    startSonic();
}

void CSAT3_Sonic::validate()
    throw(n_u::InvalidParameterException)
{
    SonicAnemometer::validate();

    const std::list<const SampleTag*> tags= getSampleTags();

    if (tags.size() > 2 || tags.size() < 1)
        throw n_u::InvalidParameterException(getName() +
		" can only create two samples (wind and extra)");

    std::list<const SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
	const SampleTag* stag = *si;
	/*
	 * nvars
	 * 5	u,v,w,tc,diag
	 * 7	u,v,w,tc,diag,spd,dir
	 * 9	u,v,w,tc,diag,uflag,vflag,wflag,tcflag
	 * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
	 */
	if (stag->getSampleId() == 1) {
	    _rate = (int)rint(stag->getRate());
	    size_t nvars = stag->getVariables().size();
	    switch(nvars) {
	    case 5:
	    case 9:
		windSampleId = stag->getId();
		windNumOut = nvars;
		if (nvars == 9) spikeIndex = 5;
		break;
	    case 11:
	    case 7:
		windSampleId = stag->getId();
		windNumOut = nvars;
		if (nvars == 11) spikeIndex = 7;
		{
		    VariableIterator vi = stag->getVariableIterator();
		    for (int i = 0; vi.hasNext(); i++) {
			const Variable* var = vi.next();
			const string& vname = var->getName();
			if (vname.length() > 2 && vname.substr(0,3) == "spd")
			    spdIndex = i;
			else if (vname.length() > 2 && vname.substr(0,3) == "dir")
			    dirIndex = i;
		    }
		}
		if (spdIndex < 0 || dirIndex < 0)
		    throw n_u::InvalidParameterException(getName() +
		      " CSAT3 cannot find speed or direction variables");
		break;
	    default:
		throw n_u::InvalidParameterException(getName() +
	      " unsupported number of variables. Must be: u,v,w,tc,diag,[spd,dir][4xflags]]");
	    }
	}
	else {
	    extraSampleTags.push_back(stag);
	    totalInLen += 2;	// 2 bytes for each additional input
	}
    }
#if __BYTE_ORDER == __BIG_ENDIAN
    swapBuf.reset(new short[totalInLen/2]);
#endif
}

float CSAT3_Sonic::correctTcForPathCurvature(float tc,
	float u, float v, float w)
{
    // no correction necessary. CSAT outputs speed of sound
    // that is already corrected for path curvature.
    return tc;
}

bool CSAT3_Sonic::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    size_t inlen = samp->getDataByteLength();
    if (inlen < windInLen) return false;	// not enough data

    const char* dinptr = (const char*) samp->getConstVoidDataPtr();
    // check for correct termination bytes
#ifdef DEBUG
    cerr << "inlen=" << inlen << ' ' << hex << (int)dinptr[inlen-2] <<
    	',' << (int)dinptr[inlen-1] << dec << endl;
#endif

    // Sometimes a serializer is connected to a sonic, in which
    // case it generates records longer than 12 bytes.
    // Check here that the record ends in 0x55 0xaa.
    if (dinptr[inlen-2] != '\x55' || dinptr[inlen-1] != '\xaa') return false;

    if (inlen > totalInLen) inlen = totalInLen;

#if __BYTE_ORDER == __BIG_ENDIAN
    /* Swap bytes of input. Campbell output is little endian */
    swab(dinptr,(char *)swapBuf.get(),inlen-2);     // dont' swap 0x55 0xaa
    const short* win = swapBuf.get();
#else
    const short* win = (const short*) dinptr;
#endif

    /*
     * CSAT3 has an internal two sample buffer, so shift
     * wind time tags backwards by two samples.
     */
    if (nttsave < 0) 
        timetags[nttsave++ + 2] = samp->getTimeTag();
    else {
	SampleT<float>* wsamp = getSample<float>(windNumOut);
	wsamp->setTimeTag(timetags[nttsave]);
	wsamp->setId(windSampleId);

	timetags[nttsave] = samp->getTimeTag();
	nttsave = (nttsave + 1) % 2;

	float* uvwtd = wsamp->getDataPtr();

	unsigned short diag = (unsigned) win[4];
	int range[3];
	range[0] = (diag & 0x0c00) >> 10;
	range[1] = (diag & 0x0300) >> 8;
	range[2] = (diag & 0x00c0) >> 6;
	int cntr = (diag & 0x003f);
	diag = (diag & 0xf000) >> 12;

	if ((++counter % 64) != cntr) diag += 16;
	counter = cntr;

	const float scale[] = {0.002,0.001,0.0005,0.00025};

	int j;
	int nmissing = 0;
	for (int i = 0; i < 3; i++) {
            int ix = _tx[i];
	    uvwtd[i] = _sx[i] * win[ix] * scale[j = range[ix]];

	    /* Screen NaN encodings of wind components */
	    if (j == 0)
	      switch (win[ix]) {
	      case -32768:
	      case 0:
		uvwtd[i] = floatNAN;
		nmissing++;
		break;
	      default:
		break;
	      }
	}

	/*
	 * Documentation also says speed of sound should be a NaN if
	 * ALL the wind components are NaNs.
	 */
	if (nmissing == 3 || win[3] == -32768)
	    uvwtd[3] = floatNAN;
	else {
	    /* convert to speed of sound */
	    float c = (win[3] * 0.001) + 340.0;
	    /* Convert speed of sound to Tc */
	    c /= 20.067;
	    uvwtd[3] = c * c - 273.15;
	}
	uvwtd[4] = diag;

	SonicAnemometer::processSonicData(wsamp->getTimeTag(),
		uvwtd,
		(spdIndex >= 0 ? uvwtd+spdIndex: 0),
		(dirIndex >= 0 ? uvwtd+dirIndex: 0),
		(spikeIndex >= 0 ? uvwtd+spikeIndex: 0));

	results.push_back(wsamp);
    }

    // inlen is now less than or equal to the expected input length
    bool goodterm = dinptr[inlen-2] == '\x55' && dinptr[inlen-1] == '\xaa';

    for (unsigned int i = 0; i < extraSampleTags.size(); i++) {

        if (inlen < windInLen + (i+1) * 2) break;

        const SampleTag* stag = extraSampleTags[i];
        const vector<const Variable*>& vars = stag->getVariables();
        size_t nvars = vars.size();
        SampleT<float>* hsamp = getSample<float>(nvars);
        hsamp->setTimeTag(samp->getTimeTag());
        hsamp->setId(stag->getId());

        unsigned short counts = ((const unsigned short*) win)[i+5];
        float volts;
        if (counts < 65500 && goodterm) volts = counts * 0.0001;
        else volts = floatNAN;

        for (unsigned int j = 0; j < nvars; j++) {
            const Variable* var = vars[j];
            VariableConverter* conv = var->getConverter();
            if (!conv) hsamp->getDataPtr()[j] = volts;
            else hsamp->getDataPtr()[j] =
                conv->convert(hsamp->getTimeTag(),volts);
        }
        results.push_back(hsamp);
    }
    return true;
}

void CSAT3_Sonic::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    SonicAnemometer::fromDOMElement(node);

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "orientation") {
            bool pok = parameter->getType() == Parameter::STRING_PARAM &&
                parameter->getLength() == 1;
            if (pok && parameter->getStringValue(0) == "normal") {
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = 1;
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "down") {
                 /* When the sonic is hanging down, the usual sonic w axis
                  * becomes the new u axis, u becomes w, and v becomes -v. */
                _tx[0] = 2;     // new u is normal w
                _tx[1] = 1;     // v is -v
                _tx[2] = 0;     // new w is normal u
                _sx[0] = 1;
                _sx[1] = -1;    // v is -v
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "flipped") {
                 /* Sonic flipped over, w becomes -w, v becomes -v. */
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = -1;
                _sx[2] = -1;
            }
            else
                throw n_u::InvalidParameterException(getName(),
                    "orientation parameter",
                    "must be one string: \"normal\" (default), \"down\" or \"flipped\"");
        }
        else if (parameter->getName() == "oversample") {
            if (parameter->getType() != Parameter::BOOL_PARAM ||
                parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                        "oversample parameter",
                        "must be boolean true or false");
            _oversample = (int)parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "soniclog") {
            if (parameter->getType() != Parameter::STRING_PARAM ||
                parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                        "soniclog parameter",
                        "must be a string");
            _sonicLogFile = parameter->getStringValue(0);
        }
    }
}
