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


#ifndef NIDIS_DYNLD_MODBUSRTU_H
#define NIDIS_DYNLD_MODBUSRTU_H

#ifdef HAVE_LIBMODBUS
#include <modbus/modbus.h>
#endif

#include <unistd.h>     // pipe

#include <nidas/core/SerialSensor.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace dynld {

    using namespace nidas::core;

/**
 * A class for data from a modbus RTU sensor, connected to a serial 
 * port.
 *
 * To read modbus data from the port, this class uses libmodbus,
 * and so the package libmodbus-devel should be installed on build systems,
 * and package libmodbus on run systems.
 *
 * libmodbus is not needed on systems which only process the archived data
 * from a libmodbus sensor.
 *
 * An XML configuration for this sensor looks like the following, for
 * a temperature sensor at RS485 slaveID=1, reading one value
 * from register 0.
 *
 * This example works for a DS18B20 temperature probe interfaced through
 * a Eletechsup R46CA01, as a RS485 device connected to /dev/ttyS2.
 * 2 byte word values are treated as signed integers and converted to
 * temperature values by a 0.1 degC/count linear conversion.
 *
 *  <serialSensor class="ModbusRTU" devicename="/dev/ttyS2" id="100" 
 *      baud="9600" databits="8" parity="none" stopbits="1">
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
 *  modbus_read_registers, and the thread feeds the data to Nidas via
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

#ifdef HAVE_LIBMODBUS
    void close();

    IODevice* buildIODevice();

    SampleScanner* buildSampleScanner();
#endif

    /**
     * Virtual method that is called to convert a raw sample containing
     * an ASCII NMEA message to a processed floating point sample.
     * These processed samples contain double precision rather than
     * single precision values because the latitude and longitude
     * reported by GPS's may have more than 7 digits of precision.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();

#ifdef HAVE_LIBMODBUS
    class ModbusThread: public nidas::util::Thread
    {
    public:
        ModbusThread(const std::string& devname, modbus_t* mb, int regaddr,
                int nvars, int pipefd, float rate):
            Thread(devname+ "_modbus_thread", false), _devname(devname), _mb(mb), _regaddr(regaddr),
            _nvars(nvars), _pipefd(pipefd), _rate(rate)
        {}
        int run() throw();
    private:
        std::string _devname;
        modbus_t* _mb;
        int _regaddr;
        uint16_t _nvars;
        int _pipefd;
        float _rate;

        // no copying
        ModbusThread(const ModbusThread&);
        // no assignment
        ModbusThread& operator=(const ModbusThread&);
    };
#endif

protected:

#ifdef HAVE_LIBMODBUS
    modbus_t* _modbusrtu;

    int _slaveID;

    int _regaddr;

    ModbusThread *_thread;

    int _pipefds[2];
#endif

    uint16_t _nvars;

    SampleTag* _stag;

private:
    // no copying
    ModbusRTU(const ModbusRTU&);
    // no assignment
    ModbusRTU& operator=(const ModbusRTU&);
};

}}	// namespace nidas namespace dynld

#endif
