/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_PROPVANE_H
#define NIDAS_DYNLD_ISFF_PROPVANE_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A serial prop vane wind sensor.  These typically report
 * wind speed and direction at about 1 sample/sec.
 * The process method of this class derives the
 * U and V vector components of the wind.
 */
class PropVane: public nidas::dynld::DSMSerialSensor
{
public:

    PropVane();

    ~PropVane();

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    const std::string& getSpeedName() const { return speedName; }

    void setSpeedName(const std::string& val) { speedName = val; }

    const std::string& getDirName() const { return dirName; }

    void setDirName(const std::string& val) { dirName = val; }

    const std::string& getUName() const { return uName; }

    void setUName(const std::string& val) { uName = val; }

    const std::string& getVName() const { return vName; }

    void setVName(const std::string& val) { vName = val; }

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    std::string speedName;

    std::string dirName;

    std::string uName;

    std::string vName;

    /**
     * Index of wind speed in output sample.
     */
    int speedIndex;

    /**
     * Index of wind direction in output sample.
     */
    int dirIndex;

    /**
     * Index of wind u component in output sample.
     */
    int uIndex;

    /**
     * Index of wind v component in output sample.
     */
    int vIndex;

    /**
     * Length of output sample.
     */
    size_t outlen;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
