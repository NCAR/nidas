// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
