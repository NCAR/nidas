
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_SAMPLEARCHIVER_H
#define NIDAS_DYNLD_SAMPLEARCHIVER_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SampleSorter.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

class SampleArchiver: public SampleIOProcessor
{
public:
    
    SampleArchiver();

    /**
     * Copy constructor.
     */
    SampleArchiver(const SampleArchiver& x);

    virtual ~SampleArchiver();

    SampleArchiver* clone() const;

    bool singleDSM() const { return false; }

    void connect(SampleInput*) throw(nidas::util::IOException);

    void disconnect(SampleInput*) throw(nidas::util::IOException);

    void connected(SampleOutput* orig,SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

protected:

    SampleInput* input;

};

}}	// namespace nidas namespace core

#endif
