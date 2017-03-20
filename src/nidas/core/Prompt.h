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
#ifndef NIDAS_CORE_PROMPT_H 
#define NIDAS_CORE_PROMPT_H 

#include <iostream>
#include <string>

namespace nidas { namespace core {

/**
 *  Class to contain prompt information - string and rate
 */

class Prompt {

public: 
    Prompt(): _promptString(),_promptRate(0.0), _promptOffset(0.0) {}

    void setString(const std::string& val) { 
        _promptString = val; }

    const std::string& getString() const { 
        return _promptString; }

    /**
     * Set rate of desired prompting, in Hz (sec^-1).
     */
    void setRate(const double val) {
         _promptRate = val; }

    double getRate() const {
         return _promptRate; }

    /**
     * Set prompt offset in seconds.  For example, for a rate of 10Hz, an offset
     * of 0 would result in prompts at 0.0, 0.1, 0.2 seconds after each second.
     * An offset of 0.01 would result in prompts at 0.01, 0.11, 0.21 seconds after the second.
     */
    void setOffset(const double val) {
         _promptOffset = val; }

    double getOffset() const {
         return _promptOffset; }

private:

    std::string  _promptString;

    double        _promptRate;

    double        _promptOffset;

};

}} // namespace nidas namespace core

#endif
