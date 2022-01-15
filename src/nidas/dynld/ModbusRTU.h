// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2021, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>

#ifdef HAVE_LIBMODBUS

#ifndef NIDIS_DYNLD_MODBUSRTU_H
#define NIDIS_DYNLD_MODBUSRTU_H

#include <modbus/modbus.h>
#include <unistd.h>     // pipe
                        //
#include <nidas/core/SerialSensor.h>
#include <nidas/core/TimetagAdjuster.h>
#include <nidas/core/VariableConverter.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace dynld {

    using namespace nidas::core;

/**
 * A class for data from a modbus RTU sensor, connected to a serial 
 * port.
 *
 * This class uses libmodbus, and so the package libmodbus-devel
 * should be installed on build systems, and package libmodbus
 * on run systems.
 *
 * An XML configuration for this sensor looks like the following, for
 * a temperature sensor at RS485 slaveID=1, reading from register 0.
 *
 * This example is for a DS18B20 temperature probe interfaced through
 * a Eletechsup R46CA01, as a RS485 device connected to /dev/ttyS2.
 * The temperature values a converted to degC by a 0.1 linear conversion.
 *
 *  <serialSensor class="ModbusRTU" devicename="/dev/ttyS2" id="100" 
 *      baud="9600" databits="8" parity="none" stopbits="1" id="100" >
 *      <parameter name="slaveID" type="int" value="1"/>
 *      <parameter name="regaddr" type="int" value="0"/>
 *      <sample id="1" rate="1">
 *          <variable longname="Temperature" name="temp" units="counts">
 *              <linear units="degC" slope="0.1" intercept="0.0"/>
 *          </variable>
 *      </sample>
 *  </serialSensor>
 *
 *  The file descriptor is hidden in an opaque structure in libmodbus.
 *  The Nidas read loop does a poll on filedescriptors.  In order to read
 *  a modbus sensor with Nidas, this class spawns a thread to do the
 *  modbus_read_registers, and the thread feeds the data to nidas via
 *  a pipe.
 */
class ModbusRTU: public SerialSensor
{
public:

    ModbusRTU();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    void init();

    void open(int flags);

    void close();

    IODevice* buildIODevice();

    SampleScanner* buildSampleScanner();
    /**
     * Virtual method that is called to convert a raw sample containing
     * an ASCII NMEA message to a processed floating point sample.
     * These processed samples contain double precision rather than
     * single precision values because the latitude and longitude
     * reported by GPS's may have more than 7 digits of precision.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

    class ModbusThread: public nidas::util::Thread
    {
    public:
        ModbusThread(const std::string& devname, modbus_t* mb, int regaddr,
                int nvars, int pipefd):
            Thread(devname, false), _devname(devname), _mb(mb), _regaddr(regaddr),
            _nvars(nvars), _pipefd(pipefd)
        {}
        int run() throw();
    private:
        std::string _devname;
        modbus_t* _mb;
        int _regaddr;
        uint16_t _nvars;
        int _pipefd;

        // no copying
        ModbusThread(const ModbusThread&);
        // no assignment
        ModbusThread& operator=(const ModbusThread&);
    };

protected:

    modbus_t* _modbusrtu;

    int _slaveID;

    int _regaddr;

    ModbusThread *_thread;

    int _pipefds[2];

    uint16_t _nvars;

    SampleTag* _stag;

    std::vector<nidas::core::VariableConverter*> _converters;

private:
    // no copying
    ModbusRTU(const ModbusRTU&);
    // no assignment
    ModbusRTU& operator=(const ModbusRTU&);
};

}}	// namespace nidas namespace dynld

#endif
#endif
