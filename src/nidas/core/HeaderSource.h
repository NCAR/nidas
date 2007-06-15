/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_HEADERSOURCE_H
#define NIDAS_CORE_HEADERSOURCE_H

#include <nidas/core/DSMTime.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

class SampleOutput;

/**
 * An interface for sending a SampleHeader to a SampleOutput.
 */
class HeaderSource
{
public:
    virtual ~HeaderSource() {}

    static void sendDefaultHeader(SampleOutput* output)
    	throw(nidas::util::IOException);

    /**
     * Method called to write a header to an SampleOutput.
     * Derived classes implement as they see fit.
     */
    virtual void sendHeader(dsm_time_t,SampleOutput* output)
    	throw(nidas::util::IOException) = 0;
};

}}	// namespace nidas namespace core

#endif
