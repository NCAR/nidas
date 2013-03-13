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

#ifndef NIDAS_DYNLD_PROCESSEDSAMPLEUDPSERVICE_H
#define NIDAS_DYNLD_PROCESSEDSAMPLEUDPSERVICE_H

#include <nidas/core/SampleIOProcessor.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

class SampleProcessor: public SampleIOProcessor
{
public:
    SampleProcessor();

    ~SampleProcessor();

    void flush() throw();

    void connect(SampleSource*) throw();

    void disconnect(SampleSource*) throw();

    void connect(SampleOutput* output) throw();

    void disconnect(SampleOutput* output) throw();


private:

    nidas::util::Mutex _connectionMutex;

    std::set<SampleSource*> _connectedSources;

    std::set<SampleOutput*> _connectedOutputs;

    /**
     * Copy not supported.
     */
    SampleProcessor(const SampleProcessor&);

    /**
     * Assignment not supported.
     */
    SampleProcessor& operator=(const SampleProcessor&);

};


}}	// namespace nidas namespace core

#endif
