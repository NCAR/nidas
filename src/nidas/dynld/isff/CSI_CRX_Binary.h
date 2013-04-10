// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_ISFF_CSI_CRX_BINARY_H
#define NIDAS_DYNLD_ISFF_CSI_CRX_BINARY_H

#include <nidas/core/SerialSensor.h>
#include <nidas/core/Sample.h>

namespace nidas { namespace dynld { namespace isff {

using nidas::core::Sample;
using nidas::core::dsm_sample_id_t;

/**
 * A class for parsing the binary output of Campbell Scientific CR10X and CR23X
 * data loggers. See section C.2 "Final Storage Format", in appendix C of
 * the CR10X or CR23X Operator's Manuals.
 */
class CSI_CRX_Binary: public nidas::core::SerialSensor
{
public:

    CSI_CRX_Binary();

    ~CSI_CRX_Binary();

    void validate()
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const nidas::core::Sample*>& results)
    	throw();

    /**
     * Calculate the CRC signature of a data record. From CR10X, CR23X manual.
     */
    static unsigned short signature(const unsigned char* buf, const unsigned char* eob);

private:

    bool reportBadCRC();

    /**
     * Requested number of output variables.
     */
    int _numOut;

    /**
     * Output sample id
     */
    dsm_sample_id_t _sampleId;

    /**
     * Counter of the number of records with incorrect CRC signatures.
     */
    unsigned int _badCRCs;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
