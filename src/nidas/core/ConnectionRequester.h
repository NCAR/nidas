// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef NIDAS_CORE_CONNECTIONREQUESTER_H
#define NIDAS_CORE_CONNECTIONREQUESTER_H

#include <nidas/util/IOException.h>
#include <nidas/util/Inet4PacketInfo.h>

namespace nidas { namespace core {

class SampleInput;
class SampleOutput;

/**
 * Interface for an object that requests connections SampleOutputs.
 */
class SampleConnectionRequester
{
public:
    virtual ~SampleConnectionRequester() {}

    /**
     * How SampleOutputs notify their SampleConnectionRequester
     * that they are connected.
     */
    virtual void connect(SampleOutput* output) throw() = 0;

    /**
     * How SampleOutputs notify their SampleConnectionRequester
     * that they wish to be closed, likely do to an IOException.
     */
    virtual void disconnect(SampleOutput* output) throw() = 0;
};

}}	// namespace nidas namespace core

#endif
