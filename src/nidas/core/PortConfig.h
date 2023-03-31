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

#ifndef NIDAS_CORE_PORTCONFIG_H
#define NIDAS_CORE_PORTCONFIG_H

#include "nidas/util/Termios.h"
#include "PortType.h"

namespace nidas { namespace core {

struct PortConfig
{
    PortConfig(int baudRate, int dataBits,
               nidas::util::Parity parity,
               int stopBits,
               PortType ptype=RS232,
               PortTermination term=NO_TERM,
               int initRts485=0);

    PortConfig(const PortConfig& rInitPortConfig);

    PortConfig& operator=(const PortConfig& rInitPortConfig);

    PortConfig();

    bool operator!=(const PortConfig& rRight) const;
    bool operator==(const PortConfig& rRight) const;

    /**
     * If @p name matches a PortConfig attribute, set it to @p value.
     *
     * @p context is a string that will be included in exceptions to help
     * identify the source of the invalid text.
     * Attribute names and their possible values, upper or lower case:
     * 
     * @verbatim 
     * termination: NO_TERM, TERM_ON, TERM_120_OHM
     * porttype: RS232, RS422, RS485_HALF, RS458_FULL
     * rts485: TRUE, FALSE, -1, 0, 1
     * baud: rate as a number, like 9600 or 115200
     * parity: ODD, EVEN, NONE, or O, E, N
     * databits: 6, 7, 8
     * stopbits: 1, 2
     * @verbatim
     * 
     * Values can be upper or lower case.  Return true if attribute @p name
     * was recognized and parsed, false if not found.  Throws an exception
     * if the attribute value fails to parse.
     * 
     * @throws nidas::util::InvalidParameterException
     */
    bool setAttribute(const std::string& context, 
                      const std::string& name, const std::string& value);

    nidas::util::Termios termios;
    PortType port_type;
    PortTermination port_term;
    int rts485;

private:
    void update_termios();

};


std::ostream& operator <<(std::ostream& rOutStrm, const PortConfig& rObj);

} // namespace core
} // namespace nidas

#endif // NIDAS_CORE_PORTCONFIG_H
