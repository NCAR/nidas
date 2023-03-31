/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2023, Copyright University Corporation for Atmospheric Research
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
#ifndef _PORTTYPE_H_
#define _PORTTYPE_H_

#include <string>

namespace nidas {
namespace core {

/**
 * PortType is an enumerated class.  It wraps a private enumeration type and
 * adds methods for converting to and from text.  The RS485_FULL instance is
 * set deliberately to RS422, so they can be used interchangeably.
 */
struct PortType
{
private:
    enum PORT_TYPES {
        ELOOPBACK=0,
        ERS232=232,
        ERS422=422,
        ERS485_FULL=422,
        ERS485_HALF=485
    }
    ptype;

    explicit
    PortType(PORT_TYPES pt):
        ptype(pt)
    {}

public:

    /**
     * Default to LOOPBACK.
     */
    PortType();

    PortType(const PortType&) = default;
    PortType& operator=(const PortType&) = default;

    static const PortType LOOPBACK;
    static const PortType RS232;
    static const PortType RS422;
    static const PortType RS485_FULL;
    static const PortType RS485_HALF;

    /**
     * Return the short version of the port type.
     * 
     * loop, 232, 422, 485h.  422 is returned instead of 485f.
     * 
     * @return std::string 
     */
    std::string toShortString() const;

    /**
     * Return the long version of the port type as a string, like RS485_HALF.
     */
    std::string toLongString() const;

    /**
     * Parse @p text and set this port type to the parsed value.
     * 
     * loopback:   "loop", "loopback", "LOOP", "LOOPBACK"
     * rs232:      "232", "rs232", "RS232"
     * rs422:      "422", "rs422", "RS422"
     * rs485_full: "485f", "rs485f", "485F", "rs485_full", "RS485_FULL"
     * rs485_half: "485h", "rs485h", "485H", "rs485_half", "RS485_HALF"
     * 
     * @return true if the text parses and the value is set, otherwise
     * return false.
     */
    bool
    parse(const std::string& text);

    bool operator==(const PortType& right) const
    {
        return this->ptype == right.ptype;
    }

    bool operator!=(const PortType& right) const
    {
        return this->ptype != right.ptype;
    }

};

// for compatibility with code that uses the enumerations in namespace scope.
extern const PortType LOOPBACK;
extern const PortType RS232;
extern const PortType RS422;
extern const PortType RS485_FULL;
extern const PortType RS485_HALF;

std::ostream&
operator<<(std::ostream& out, const PortType& ptype);


struct PortTermination
{
private:
    enum TERM {
        ENO_TERM=0,
        ETERM_ON
    }
    term;

    explicit
    PortTermination(TERM eterm):
        term(eterm)
    {}

public:

    /**
     * Default to NO_TERM.
     */
    PortTermination();

    PortTermination(const PortTermination&) = default;
    PortTermination& operator=(const PortTermination&) = default;

    static const PortTermination NO_TERM;
    static const PortTermination TERM_ON;

    /**
     * Return "term" or "noterm".
     */
    std::string toShortString() const;

    /**
     * Return "TERM_ON" or "NO_TERM".
     */
    std::string toLongString() const;

    /**
     * Parse @p text and assign the value to this PortTermination.
     *
     * Accepts upper or lower case forms of term, term_on, noterm, no_term.
     *
     * @return true if the text parsed and the setting was applied, otherwise
     * false.
     */
    bool
    parse(const std::string& text);

    bool operator==(const PortTermination& right) const
    {
        return this->term == right.term;
    }

    bool operator!=(const PortTermination& right) const
    {
        return this->term != right.term;
    }
};

// for compatibility with code which put the enumeration in the namespace, and
// for the more verbose TERM_120_OHM which is equivalent to TERM_ON.
extern const PortTermination NO_TERM;
extern const PortTermination TERM_ON;
extern const PortTermination TERM_120_OHM;

std::ostream&
operator<<(std::ostream& out, const PortTermination& pterm);

} // namespace core
} // namespace nidas

#endif // _PORTTYPE_H_
