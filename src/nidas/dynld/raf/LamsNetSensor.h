// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
/* LamsNetSensor.h
 *
 */


#ifndef NIDAS_DYNLD_RAF_LAMSNETSENSOR_H
#define NIDAS_DYNLD_RAF_LAMSNETSENSOR_H

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/util/EndianConverter.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensor class supporting the NCAR/EOL Laser Air Motion Sensor (LAMS 3-beam)
 * via Ethernet UDP connection.
 */
class LamsNetSensor : public CharacterSensor
{
public:
    LamsNetSensor();
    ~LamsNetSensor();

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

private:

    static const int nBeams = 4;

    static const int LAMS_SPECTRA_SIZE = 512;

    const Sample *_saveSamps[nBeams];

    size_t _unmatchedSamples;

    size_t _outOfSequenceSamples;

    /// beams come in two packets, so need to re-enter process().
    size_t _beam;

    uint32_t _prevSeqNum[nBeams];

    static const nidas::util::EndianConverter * _fromLittle;

    /** No copying. */
    LamsNetSensor(const LamsNetSensor&);

    /** No assignment. */
    LamsNetSensor& operator=(const LamsNetSensor&);
};

}}}

#endif
