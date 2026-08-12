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

#include "PortConfig.h"
#include "nidas/util/Logger.h"
#include "nidas/util/InvalidParameterException.h"

#include <sstream>
#include <algorithm>

namespace nidas { namespace core {

using nidas::util::Termios;
using nidas::util::Parity;
using nidas::util::InvalidParameterException;


PortConfig::
PortConfig(const int baudRate, const int dataBits, const Parity parity,
           const int stopBits,
           const PortType ptype, const PortTermination term,
           const int initRts485):
    termios(),
    port_type(ptype),
    port_term(term),
    rts485(initRts485)
{
    update_termios();

    termios.setBaudRate(baudRate);
    termios.setParity(parity);
    termios.setDataBits(dataBits);
    termios.setStopBits(stopBits);
}


PortConfig::
PortConfig():
    termios(),
    port_type(nidas::core::RS232),
    port_term(nidas::core::NO_TERM),
    rts485(0)
{
    update_termios();
}


bool
PortConfig::
operator!=(const PortConfig& rRight) const
{
    return !((*this) == rRight);
}


bool
PortConfig::
operator==(const PortConfig& rRight) const
{
    return (termios == rRight.termios &&
            port_type == rRight.port_type &&
            port_term == rRight.port_term &&
            rts485 == rRight.rts485);
}


bool
PortConfig::
setAttribute(const std::string& context, const std::string& name,
             const std::string& value_in)
{
    // xform everything to uppercase - this shouldn't affect numbers
    std::string value = value_in;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    std::istringstream ist(value);
    int ivalue;
    ist >> ivalue;
    bool found = true;
    bool parsed = true;
    DLOG(("PortConfig checking attribute: ") << name << " : " << value_in);
    if (name == "porttype")
    {
        parsed = port_type.parse(value);
    }
    else if (name == "termination")
    {
        parsed = port_term.parse(value);
    }
    else if (name == "rts485")
    {
        if (value == "true" || value == "1") {
            rts485 = 1;
        }
        else if (value == "false" || value == "0")
        {
            rts485 = 0;
        }
        else if (value == "-1")
        {
            rts485 = -1;
        }
        else
        {
            parsed = false;
        }
    }
    else if (name == "baud")
    {
        parsed = (!ist.fail() && termios.setBaudRate(ivalue));
    }
    else if (name == "parity")
    {
        Parity parity;
        if ((parsed = parity.parse(value)))
            termios.setParity(parity);
    }
    else if (name == "databits") {
        parsed = !(ist.fail() || ivalue < 5 || ivalue > 8);
        if (parsed)
            termios.setDataBits(ivalue);
    }
    else if (name == "stopbits") {
        parsed = !(ist.fail() || ivalue < 1 || ivalue > 2);
        if (parsed)
            termios.setStopBits(ivalue);
    }
    else {
        found = false;
    }
    if (found && !parsed)
        throw InvalidParameterException(context, name, value_in);
    return found;
}


void
PortConfig::
update_termios()
{
    // ensure some important required settings for all port configs, to
    // override the Termios defaults which are not appropriate for reading
    // sensor data from serial ports.
    termios.setLocal(true);
    termios.setFlowControl(Termios::NOFLOWCONTROL);

    // There were not originally set here, but all known callers of PortConfig
    // expect these settings.  So set them here so the settings do not need to
    // be duplicated everywhere else a PortConfig is created. Callers can
    // still change the termios settings through direct access to the termios
    // member.  See the comment in SerialSensor::setPortConfig().

    termios.setRaw(true);
    termios.setRawLength(1);
    termios.setRawTimeout(0);
}


} // namespace core
} // namespace nidas
