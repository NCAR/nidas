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
 * CSAT3 sample. Right now these extra values are assumed to be
 * conversions of the voltage from a Campbell krypton hygrometer.
 */
class CSAT3_Sonic: public SonicAnemometer
{
public:

    CSAT3_Sonic();

    ~CSAT3_Sonic();


    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);
protected:

    /**
     * expected input sample length of basic CSAT3 record.
     */
    size_t windInLen;

    /**
     * expected input sample length of basic CSAT3 record,
     * with any additional fields added by NCAR/EOL "serializer" A2D.
     */
    size_t totalInLen;

    /**
     * Requested number of output wind variables.
     */
    int windNumOut;

    /**
     * If user requests wind speed, variable name "spd", its index in the output sample.
     */
    int spdIndex;

    /**
     * If user requests wind direction, variable name "dir", its index in the output sample.
     */
    int dirIndex;

    /**
     * If user requests despike variables, e.g. "uflag","vflag","wflag","tcflag",
     * the index of "uflag" in the output variables.
     */
    int spikeIndex;

    /**
     * Output sample id of the wind sample.
     */
    dsm_sample_id_t windSampleId;

    /**
     * Sample tags of extra "serializer" values.
     */
    std::vector<SampleTag*> extraSampleTags;

    dsm_time_t timetags[2];

    int nttsave;

    int counter;

#if __BYTE_ORDER == __BIG_ENDIAN
    auto_ptr<short> swapBuf;
#endif

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
