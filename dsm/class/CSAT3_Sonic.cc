/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-03 14:24:50 -0700 (Fri, 03 Feb 2006) $

    $LastChangedRevision: 3262 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/SonicAnemometer.cc $

*/

#include <CSAT3_Sonic.h>

#include <sstream>

using namespace dsm;
using namespace std;


CREATOR_FUNCTION(CSAT3_Sonic)

CSAT3_Sonic::CSAT3_Sonic():
	windInLen(12),	// two bytes each for u,v,w,tc,diag, and 0x55aa
	totalInLen(12),
	windNumOut(0),
	spdIndex(-1),
	dirIndex(-1),
	spikeIndex(-1),
	windSampleId(0),
	kh2oSampleId(0),
	nttsave(-2),
	counter(0),kh2oOut(-1)
{
}

CSAT3_Sonic::~CSAT3_Sonic()
{
}

void CSAT3_Sonic::addSampleTag(SampleTag* stag)
    throw(atdUtil::InvalidParameterException)
{
    if (getSampleTags().size() > 1)
        throw atdUtil::InvalidParameterException(getName() +
		" can only create two samples (wind and kh2o)");

    /*
     * nvars
     * 5	u,v,w,tc,diag
     * 7	u,v,w,tc,diag,spd,dir
     * 9	u,v,w,tc,diag,uflag,vflag,wflag,tcflag
     * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
     * 2	kh2oV,kh2o
     */
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
		if (var->getName().substr(0,3) == "spd")
		    spdIndex = i;
		else if (var->getName().substr(0,3) == "dir")
		    dirIndex = i;
	    }
	}
	if (spdIndex < 0 || dirIndex < 0)
	    throw atdUtil::InvalidParameterException(getName() +
	      " cannot find speed or direction variables");
	break;
    case 2:
	kh2oSampleId = stag->getId();
	kh2oNumOut = nvars;
	totalInLen = 14;	// additional 16-bit a2d value for kh2o voltage
	break;
    default:
        throw atdUtil::InvalidParameterException(getName() +
      " unsupported number of variables. Must be: u,v,w,tc,diag,[4xflags][kh2o,kh2oV]");
    }

    SonicAnemometer::addSampleTag(stag);

#if __BYTE_ORDER == __BIG_ENDIAN
    swapBuf.reset(new short[totalInLen/2]);
#endif
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
    if (dinptr[inlen-2] != '\x55' || dinptr[inlen-1] != '\xaa') return false;

    if (inlen > totalInLen) inlen = totalInLen;

#if __BYTE_ORDER == __BIG_ENDIAN
    /* Swap bytes of input. Campbell output is little endian */
    swab(dinptr,(char *)swapBuf.get(),inlen);
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
	    uvwtd[i] = win[i] * scale[j = range[i]];

	    /* Screen NaN encodings of wind components */
	    if (j == 0)
	      switch (win[i]) {
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

    if (kh2oSampleId > 0 && inlen >= windInLen + 2) {
	SampleT<float>* hsamp = getSample<float>(kh2oNumOut);
	hsamp->setTimeTag(samp->getTimeTag());
	hsamp->setId(kh2oSampleId);

	unsigned short counts = ((const unsigned short*) win)[5];
	float volts;
	float kh2o;
	if (counts < 65500) {
	    volts = counts * 0.0001;
	    /* low krypton voltages are usually bad --
	     * remove all less than 10mV */
	    if (volts < 0.01) kh2o = floatNAN;
	    else kh2o = krypton.convert(volts);
	}
	else {
	    volts = floatNAN;
	    kh2o = floatNAN;
	}

	hsamp->getDataPtr()[0] = volts;
	hsamp->getDataPtr()[1] = kh2o;
	results.push_back(hsamp);
    }
    return true;
}

void CSAT3_Sonic::fromDOMElement(const xercesc::DOMElement* node)
    throw(atdUtil::InvalidParameterException)
{

    DSMSerialSensor::fromDOMElement(node);

    static struct ParamSet {
        const char* name;	// parameter name
        void (CSAT3_Sonic::* setFunc)(float);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "kryptonKw",		&CSAT3_Sonic::setKryptonKw },
	{ "kryptonV0",		&CSAT3_Sonic::setKryptonV0 },
	{ "kryptonPathLength",	&CSAT3_Sonic::setKryptonPathLength },
	{ "kryptonBias",	&CSAT3_Sonic::setKryptonBias },
    };

    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw atdUtil::InvalidParameterException(getName(),
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
	(this->*paramSet[i].setFunc)(param->getNumericValue(0));
    }
}
