/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-09-10 07:54:23 -0600 (Sat, 10 Sep 2005) $

    $LastChangedRevision: 2879 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/CSAT3_Sonic.h $

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

    /**
     * @param val Krypton hygrometer Kw parameter from sensor calibration.
     */
    void setKryptonKw(float val)
    {
        krypton.setKw(val);
    }

    float getKryptonKw() const
    {
        return krypton.getKw();
    }

    /**
     * @param val Krypton hygrometer V0 value in millivolts.
     */
    void setKryptonV0(float val)
    {
        krypton.setV0(val);
    }

    float getKryptonV0() const
    {
        return krypton.getV0();
    }

    /**
     * @param val Pathlength of krypton hygrometer sensor, in cm.
     */
    void setKryptonPathLength(float val)
    {
        krypton.setPathLength(val);
    }

    float getKryptonPathLength() const
    {
        return krypton.getPathLength();
    }

    /**
     * @param val Bias (g/m^3) to be removed from hygrometer data values.
     */
    void setKryptonBias(float val)
    {
        krypton.setBias(val);
    }

    float getKryptonBias() const
    {
        return krypton.getBias();
    }

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
     * Requested number of output krypton hygrometer variables.
     */
    int kh2oNumOut;

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
     * Output sample id of the kyrpton hygrometer sample.
     */
    dsm_sample_id_t kh2oSampleId;

    dsm_time_t timetags[2];

    int nttsave;

    int counter;

    CS_Krypton krypton;

    int kh2oOut;

#if __BYTE_ORDER == __BIG_ENDIAN
    auto_ptr<short> swapBuf;
#endif

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
