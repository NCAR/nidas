/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_CSAT3_SONIC_H
#define NIDAS_DYNLD_ISFF_CSAT3_SONIC_H

#include <nidas/dynld/isff/SonicAnemometer.h>
#include <nidas/dynld/isff/CS_Krypton.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A class for making sense of data from a Campbell Scientific Inc
 * CSAT3 3D sonic anemometer.
 * This also supports records which have been altered by an NCAR/EOL
 * "serializer" A2D, which digitizes voltages at the reporting rate
 * of the sonic and inserts additional 2-byte A2D counts in the
 * CSAT3 sample.
 */
class CSAT3_Sonic: public SonicAnemometer
{
public:

    CSAT3_Sonic();

    ~CSAT3_Sonic();

    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void validate()
            throw(nidas::util::InvalidParameterException);

    /**
     * No correction for path curvature is needed on the CSAT,
     * so this method just returns an unchanged tc.
     */
    float correctTcForPathCurvature(float tc,
            float u, float v, float w);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    void stopSonic() throw(nidas::util::IOException);

    void startSonic() throw(nidas::util::IOException);

    std::string querySonic(int& acqrate, char& osc, std::string& serialNumber,
        std::string& revsion)
        throw(nidas::util::IOException);

    const char* getRateCommand(int rate,bool overSample);

    std::string sendRateCommand(const char* cmd)
        throw(nidas::util::IOException);

    /**
     * expected input sample length of basic CSAT3 record.
     */
    size_t _windInLen;

    /**
     * expected input sample length of basic CSAT3 record,
     * with any additional fields added by NCAR/EOL "serializer" A2D.
     */
    size_t _totalInLen;

    /**
     * Requested number of output wind variables.
     */
    int _windNumOut;

    /**
     * "Logical" sonic diagnostic, 0 if all diagnostic flags are 0, otherwise 1.
     */
    int _ldiagIndex;

    /**
     * If user requests wind speed, variable name "spd", its index in the output sample.
     */
    int _spdIndex;

    /**
     * If user requests wind direction, variable name "dir", its index in the output sample.
     */
    int _dirIndex;

    /**
     * If user requests despike variables, e.g. "uflag","vflag","wflag","tcflag",
     * the index of "uflag" in the output variables.
     */
    int _spikeIndex;

    /**
     * Output sample id of the wind sample.
     */
    dsm_sample_id_t _windSampleId;

    /**
     * Sample tags of extra "serializer" values.
     */
    std::vector<const SampleTag*> _extraSampleTags;

    dsm_time_t _timetags[2];

    int _nttsave;

    int _counter;

#if __BYTE_ORDER == __BIG_ENDIAN
    std::vector<short> _swapBuf;
#endif

    /**
     * Index transform vector for wind components.
     * Used for unusual sonic orientations, as when the sonic
     * is hanging down, when the usual sonic w axis becomes the
     * new u axis, u becomes w and v becomes -v.
     */
    int _tx[3];

    /**
     * Wind component sign conversion. Also used for unusual sonic
     * orientations, as when the sonic is hanging down, and the sign
     * of v is flipped.
     */
    int _sx[3];

    int _rate;

    bool _oversample;

    std::string _serialNumber;

    std::string _sonicLogFile;

    /**
     * This is a limit for the inter-sample delta-T. If the delta-T is greater
     * than this value, then the saved values of the previous two sample
     * time tags are discarded, and the correction for the 2 sample
     * internal CSAT3 buffer is restarted, resulting in a discard of the
     * previous 2 samples.
     */
    int _gapDtUsecs;

    /**
     * Last time tag.
     */
    dsm_time_t _ttlast;

    /**
     * Set winds and virtual temperature to NaN if
     * diagnostic value is non-zero?
     */
    bool _nanIfDiag;
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
