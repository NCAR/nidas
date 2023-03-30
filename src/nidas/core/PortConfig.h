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
    PortConfig(const int baudRate, const int dataBits, const nidas::util::Termios::parity parity, const int stopBits, 
               const PortType ptype, const PortTermination term, const int initRts485);

    PortConfig(const PortConfig& rInitPortConfig);

    PortConfig& operator=(const PortConfig& rInitPortConfig);

    PortConfig();

    PortConfig(const std::string& rDeviceName, const int fd);

    bool operator!=(const PortConfig& rRight) const;
    bool operator==(const PortConfig& rRight) const;

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
