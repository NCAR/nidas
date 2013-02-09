// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $LastChangedDate$

 $LastChangedRevision$

 $LastChangedBy$

 $HeadURL$

 ******************************************************************
 */
#ifndef NIDAS_DYNLD_RAF_DSMARINCSENSOR_H
#define NIDAS_DYNLD_RAF_DSMARINCSENSOR_H

#include <nidas/linux/arinc/arinc.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/VariableConverter.h>
#include <nidas/util/InvalidParameterException.h>

// Significant bits masks
//
// 32|31 30|29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11|10  9| 8  7  6  5  4  3  2  1
// --+-----+--------------------------------------------------------+-----+-----------------------
// P | SSM |                                                        | SDI |      8-bit label      

// bitmask for the Sign Status Matrix
#define SSM 0x60000000
#define NCD 0x20000000
#define TST 0x40000000

#define NLABELS 256

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

// inHg to mBar
const float INHG_MBAR  = 33.8639;

// NM to meter.
const float NM_MTR  = 1.0 / 1852.0;

// ft to meter.
const float FT_MTR  = 0.3048;

// G to m/s2 (ACINS).
const float G_MPS2   = 9.7959;

// knot to m/s
const float KTS_MS = 0.514791;

// ft/min to m/s (VSPD)
const float FPM_MPS  = 0.00508;

// radian to degree.
const float RAD_DEG = 180.0 / 3.14159265358979; 


/**
 * This is sorts a list of Sample tags by rate (highest first)
 * then by label.
 */
class SortByRateThenLabel {
public:
    bool operator() (const SampleTag* x, const SampleTag* y) const {
        if ( x->getRate() > y->getRate() ) return true;
        if ( x->getRate() < y->getRate() ) return false;
        if ( x->getId()   < y->getId()   ) return true;
        return false;
    }
};

/**
 * A sensor connected to an ARINC port.
 */
class DSMArincSensor : public DSMSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMArincSensor();
    ~DSMArincSensor();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Validate is called before open() or init().
     */
    void validate() throw(nidas::util::InvalidParameterException);

    /** This opens the associated device. */
    void open(int flags) throw(nidas::util::IOException,
            nidas::util::InvalidParameterException);

    /** This closes the associated device. */
    void close() throw(nidas::util::IOException);

    /**
     * Perform any initialization necessary for process method.
     */
    void init() throw(nidas::util::InvalidParameterException);

    /** Process a raw sample, which in this case means create
     * a list of samples with each sample containing a timetag. */
    bool process(const Sample*, std::list<const Sample*>& result)
        throw();

    /** Display some status information gathered by the driver. */
    void printStatus(std::ostream& ostr) throw();

    /** This contains a switch case for processing all labels. */
    virtual double processLabel(const int data,sampleType* stype) = 0;

    /** Extract the ARINC configuration elements from the XML header. */
    /// example XML:
    ///  <arincSensor ID="GPS-GV" class="GPS_HW_HG2021GB02" speed="low" parity="odd">
    void fromDOMElement(const xercesc::DOMElement*)
        throw(nidas::util::InvalidParameterException);

    int getInt32TimeTagUsecs() const 
    {
        return USECS_PER_MSEC;
    }


protected:
    /// A list of which samples are processed.
    int _processed[NLABELS];

private:

    /** channel configuration */
    unsigned int _speed;
    unsigned int _parity;

    std::map<dsm_sample_id_t,VariableConverter*> _converters;
};

// typedef SampleT<unsigned int> ArincSample;

}}}	// namespace nidas namespace dynld namespace raf

#endif
