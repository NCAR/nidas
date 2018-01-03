// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#include "PIP_Image.h"
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>

#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf,PIP_Image)


const n_u::EndianConverter * PIP_Image::bigEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);

const n_u::EndianConverter * PIP_Image::littleEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_LITTLE_ENDIAN);


PIP_Image::PIP_Image()
{
}

PIP_Image::~PIP_Image()
{
}


bool PIP_Image::process(const Sample* samp,list<const Sample*>& results)
        throw()
{
//cerr<<"PIP_Image process\n";

return false;
}

