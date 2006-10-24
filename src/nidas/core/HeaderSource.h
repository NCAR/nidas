/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-06-01 09:00:03 -0600 (Thu, 01 Jun 2006) $

    $LastChangedRevision: 3379 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/ConnectionRequester.h $
 ********************************************************************

*/


#ifndef NIDAS_CORE_HEADERSOURCE_H
#define NIDAS_CORE_HEADERSOURCE_H

#include <nidas/core/DSMTime.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

class SampleOutput;

/**
 * Interface for an object that requests connections to Inputs
 * or Outputs.
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
