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
#ifndef NIDAS_CORE_SYNCRECORDVARIABLE_H
#define NIDAS_CORE_SYNCRECORDVARIABLE_H

#include <nidas/core/Variable.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A Variable associated with a SyncRecord.
 */
class SyncRecordVariable : public Variable
{
public:
    SyncRecordVariable(): offset(0),lagOffset(0) {}
    /**
     * Get the index into the sync record of the 
     * first value for this variable.
     */
    int getSyncRecOffset() const { return offset; }

    void setSyncRecOffset(int val) { offset = val; }

    /**
     * Get the index into the sync record of the lag
     * value for this variable.
     * The lag value is the fractional number of micro-seconds
     * to add to the sync record time to get the time tag
     * of the first value of this variable in the sync record.
     * One would use it like so:
     * float lag = sync_rec[var->getLagOffset()];
     * if (!isnan(lag)) tt += lag;
     */
    int getLagOffset() const { return lagOffset; }

    void setLagOffset(int val) { lagOffset = val; }

private:

    int offset;

    int lagOffset;

    /**
     * Assignment not supported.
     */
    SyncRecordVariable& operator=(const SyncRecordVariable&);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
