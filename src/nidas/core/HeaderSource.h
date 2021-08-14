// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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


#ifndef NIDAS_CORE_HEADERSOURCE_H
#define NIDAS_CORE_HEADERSOURCE_H

#include "Sample.h"
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

class SampleOutput;
class SampleInputHeader;

/**
 * An interface for sending a SampleHeader to a SampleOutput.
 */
class HeaderSource
{
public:
    virtual ~HeaderSource() {}

    static void setDefaults(SampleInputHeader& header);

    /**
     * @throws nidas::util::IOException
     **/
    static void sendDefaultHeader(SampleOutput* output);

    /**
     * Method called to write a header to an SampleOutput.
     * Derived classes implement as they see fit.
     *
     * @throws nidas::util::IOException
     **/
    virtual void sendHeader(dsm_time_t,SampleOutput* output) = 0;
};

}}	// namespace nidas namespace core

#endif
