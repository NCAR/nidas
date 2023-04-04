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

#include "PortType.h"
#include <vector>

namespace nidas {
namespace core {

const PortType PortType::LOOPBACK(ELOOPBACK);
const PortType PortType::RS232(ERS232);
const PortType PortType::RS422(ERS422);
const PortType PortType::RS485_FULL(ERS485_FULL);
const PortType PortType::RS485_HALF(ERS485_HALF);


const PortType LOOPBACK(PortType::LOOPBACK);
const PortType RS232(PortType::RS232);
const PortType RS422(PortType::RS422);
const PortType RS485_FULL(PortType::RS485_FULL);
const PortType RS485_HALF(PortType::RS485_HALF);


struct port_type_aliases_t
{
    PortType ptype;
    std::vector<std::string> aliases;
};


// Associate port types with names.  The first alias is the short form, the
// last is the long form.
std::vector<port_type_aliases_t> port_type_aliases{
    { PortType::LOOPBACK,    { "loop", "loopback", "LOOP", "LOOPBACK" } },
    { PortType::RS232,       { "232", "rs232", "RS232" } },
    { PortType::RS422,       { "422", "rs422", "RS422" } },
    { PortType::RS485_FULL,  { "485f", "rs485f", "485F", "rs485_full", "RS485_FULL" } },
    { PortType::RS485_HALF,  { "485h", "rs485h", "485H", "rs485_half", "RS485_HALF" } }
};


PortType::
PortType(): ptype(ELOOPBACK)
{}


std::string
PortType::
toString(pt_format_flags_t bit1,
         pt_format_flags_t bit2,
         pt_format_flags_t bit3) const
{
    unsigned int flags = bit1 | bit2 | bit3;
    bool rs485 = (flags & ptf_485);
    bool longform = (flags & ptf_long);

    for (auto& pa: port_type_aliases)
    {
        if (*this == RS422 && rs485 && pa.aliases.front() == "422")
            continue;
        if (pa.ptype == *this)
            return longform ? pa.aliases.back() : pa.aliases.front();
    }
    // something is really wrong...
    return "undefined";
}


std::string
PortType::
toShortString(bool rs485) const
{
    return toString(rs485 ? ptf_485 : ptf_none);
}


std::string
PortType::
toLongString(bool rs485) const
{
    return toString(ptf_long, rs485 ? ptf_485 : ptf_none);
}


bool
PortType::
parse(const std::string& text)
{
    for (auto& pa: port_type_aliases)
    {
        for (auto& alias: pa.aliases)
        {
            if (text == alias)
            {
                ptype = pa.ptype.ptype;
                return true;
            }
        }
    }
    return false;
}


std::ostream&
operator<<(std::ostream& out, const PortType& ptype)
{
    out << ptype.toShortString();
    return out;
}


const PortTermination PortTermination::NO_TERM(ENO_TERM);
const PortTermination PortTermination::TERM_ON(ETERM_ON);

const PortTermination NO_TERM(PortTermination::NO_TERM);
const PortTermination TERM_ON(PortTermination::TERM_ON);
const PortTermination TERM_120_OHM(PortTermination::TERM_ON);



PortTermination::
PortTermination(): term(ENO_TERM)
{}


std::string
PortTermination::
toShortString() const
{
    if (term == ETERM_ON)
        return "term";
    return "noterm";
}


std::string
PortTermination::
toLongString() const
{
    if (term == ETERM_ON)
        return "TERM_ON";
    return "NO_TERM";
}


bool
PortTermination::
parse(const std::string& text)
{
    if (text == "term" || text == "term_on" ||
        text == "TERM_ON" || text == "TERM" ||
        text == "TERM_120_OHM" || text == "term_120_ohm")
    {
        term = ETERM_ON;
    }
    else if (text == "noterm" || text == "no_term" || text == "NO_TERM")
    {
        term = ENO_TERM;
    }
    else
    {
        return false;
    }
    return true;
}


std::ostream&
operator<<(std::ostream& out, const PortTermination& pterm)
{
    out << pterm.toShortString();
    return out;
}


} // namespace core
} // namespace nidas
