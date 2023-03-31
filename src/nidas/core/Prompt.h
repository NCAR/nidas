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

#include "XDOM.h"

#include <string>
#include <iosfwd>

namespace nidas { namespace core {

/**
 * Prompt contains properties for prompting sensors.
 *
 * The prompt string gets sent to the sensor, at a certain rate, with an
 * offset into the rate interval.  The prompt can also specify a response
 * prefix to be inserted into any subsequent sensor responses.  A prompt is
 * not considered valid, or actionable, unless it has a rate, and it must also
 * have a non-empty prompt string or an activated prefix.  In other words, a
 * prompt can reset any previously set prefix without writing anything to the
 * sensor.
 */
class Prompt {

public: 
    Prompt(const std::string& promptString = "",
           double promptRate = 0.0,
           double promptOffset = 0.0);

    /**
     * Set the prompt string for this prompt.
     *
     * The prompt string may contain backslash escape sequences and null
     * characters, so be careful when copying to a char*.
     */
    void setString(const std::string& val);

    const std::string& getString() const;

    /**
     * Set the prefix.  The prefix will be marked valid even if empty.
     * 
     * Returns a reference to this Prompt, as convenience to add a prefix to a
     * constructor call:
     * 
     * @code {.C++.}
     * Prompt prompt = Prompt("ask why?", 10).setPrefix("answer:")
     * @endcode
     */
    Prompt& setPrefix(const std::string& prefix);

    const std::string& getPrefix() const;

    /**
     * Return true if the prefix has been set, even if set to empty.
     */
    bool hasPrefix() const;

    /**
     * Convenience check for a non-empty prompt string.
     */
    bool hasPrompt() const;

    /**
     * Set rate of desired prompting, in Hz (sec^-1).
     */
    void setRate(const double val);

    double getRate() const;

    /**
     * Set prompt offset in seconds.  For example, for a rate of 10Hz, an
     * offset of 0 would result in prompts at 0.0, 0.1, 0.2 seconds after each
     * second.  An offset of 0.01 would result in prompts at 0.01, 0.11, 0.21
     * seconds after the second.
     */
    void setOffset(const double val);

    double getOffset() const;

    /**
     * This is a valid prompt if it has a non-zero rate and a non-empty prompt
     * string, or else it has a non-zero rate and a prefix.
     */
    bool valid() const;

    /**
     * Load Prompt settings from the xml node.  Everything not specified in
     * the xml is reset to the default.
     */
    void fromDOMElement(const xercesc::DOMElement* node);

    /**
     * Prompt equality is member-wise equality, without any tolerance allowed
     * in the floating point comparisons.
     */
    bool operator==(const Prompt& right) const;

    /**
     * @brief Return the xml string for this Prompt.
     */
    std::string toXML() const;

    Prompt(const Prompt& right) = default;
    Prompt& operator=(const Prompt& right) = default;

private:

    std::string _promptString;
    double _promptRate;
    double _promptOffset;

    // _prefixValid indicates this is a prefix that should be implemented by
    // the sensor, even if the string is empty.
    std::string _prefix;
    bool _prefixValid;

};

std::ostream&
operator<<(std::ostream& out, const Prompt& prompt);

}} // namespace nidas namespace core

#endif
