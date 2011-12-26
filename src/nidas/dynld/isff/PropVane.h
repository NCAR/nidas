// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/core/VariableConverter.h>

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

    const std::string& getSpeedName() const { return _speedName; }

    void setSpeedName(const std::string& val) { _speedName = val; }

    const std::string& getDirName() const { return _dirName; }

    void setDirName(const std::string& val) { _dirName = val; }

    const std::string& getUName() const { return _uName; }

    void setUName(const std::string& val) { _uName = val; }

    const std::string& getVName() const { return _vName; }

    void setVName(const std::string& val) { _vName = val; }

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    std::string _speedName;

    std::string _dirName;

    std::string _uName;

    std::string _vName;

    /**
     * Index of wind speed in output sample.
     */
    int _speedIndex;

    /**
     * Index of wind direction in output sample.
     */
    int _dirIndex;

    /**
     * Index of wind u component in output sample.
     */
    int _uIndex;

    /**
     * Index of wind v component in output sample.
     */
    int _vIndex;

    /**
     * Length of output sample.
     */
    unsigned int _outlen;

    /**
     * If direction is derived from U,V, user may want a
     * correction applied to it.
     */
    nidas::core::VariableConverter* _dirConverter;

    /**
     * If speed is derived from U,V, user may want a
     * correction applied to it.
     */
    nidas::core::VariableConverter* _speedConverter;

    // no copying
    PropVane(const PropVane& x);

    // no assignment
    PropVane& operator=(const PropVane& x);


};

}}}	// namespace nidas namespace dynld namespace isff

#endif
