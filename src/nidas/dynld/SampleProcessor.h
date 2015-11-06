// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
