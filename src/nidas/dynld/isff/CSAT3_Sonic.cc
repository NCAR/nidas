/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/dynld/isff/CSAT3_Sonic.h>

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
	counter(0)
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

void CSAT3_Sonic::addSampleTag(SampleTag* stag)
    throw(n_u::InvalidParameterException)
{
    if (getSampleTags().size() > 1)
        throw n_u::InvalidParameterException(getName() +
		" can only create two samples (wind and extra)");

    /*
     * nvars
     * 5	u,v,w,tc,diag
     * 7	u,v,w,tc,diag,spd,dir
     * 9	u,v,w,tc,diag,uflag,vflag,wflag,tcflag
     * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
     */
    if (stag->getSampleId() == 1) {
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
        if (inlen >= windInLen + (i+1) * 2) {
            SampleTag* stag = extraSampleTags[i];
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
            if (parameter->getType() != Parameter::STRING_PARAM ||
                parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),"orientation parameter",
                    "must be one string: \"normal\" (default) or \"down\"");

            if (parameter->getStringValue(0) == "normal") {
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = 1;
                _sx[2] = 1;
            }
            else if (parameter->getStringValue(0) == "down") {
                 /* When the sonic is hanging down, the usual sonic w axis
                  * becomes the new u axis, u becomes w, and v becomes -v. */
                _tx[0] = 2;     // new u is normal w
                _tx[1] = 1;     // v is -v
                _tx[2] = 0;     // new w is normal u
                _sx[0] = 1;
                _sx[1] = -1;    // v is -v
                _sx[2] = 1;
            }
            else if (parameter->getStringValue(0) == "flipped") {
                 /* Sonic flipped over, w becomes -w, v to -v. */
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = -1;
                _sx[2] = -1;
            }
            else
                throw n_u::InvalidParameterException(getName(),"orientation parameter",
                    "must be one string: \"normal\" (default) or \"down\"");
        }
    }
}
