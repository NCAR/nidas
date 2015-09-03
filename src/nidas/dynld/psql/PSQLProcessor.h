/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */

/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H
#define NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SampleAverager.h>

namespace nidas { namespace dynld { namespace psql {

using nidas::core::SampleTag;
using nidas::core::SampleInput;
using nidas::core::SampleOutput;
using nidas::core::SampleAverager;

class PSQLProcessor: public nidas::core::SampleIOProcessor
{
public:
    
    PSQLProcessor();

    /**
     * Copy constructor.
     */
    PSQLProcessor(const PSQLProcessor& x);

    virtual ~PSQLProcessor();

    PSQLProcessor* clone() const;

    bool cloneOnConncetion() const { return false; }

    void connect(SampleInput*) throw(nidas::util::IOException);

    void disconnect(SampleInput*) throw(nidas::util::IOException);

    void connected(SampleOutput* orig, SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    /**
     * Set average period, in milliseconds.
     */
    void setAveragePeriod(int val) { averager.setAveragePeriod(val); }

    /**
     * Get average period, in milliseconds.
     */
    int getAveragePeriod() const { return averager.getAveragePeriod(); }

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return averager.getSampleTags();
    }

protected:

    SampleInput* input;

    SampleAverager averager;

    const nidas::core::Site* site;
};

}}}

#endif // NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H
