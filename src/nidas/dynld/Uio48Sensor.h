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

#ifndef NIDAS_DYNLD_UIO48SENSOR_H
#define NIDAS_DYNLD_UIO48SENSOR_H

#include <nidas/Config.h> 
#include <nidas/core/DSMSensor.h>
#include <nidas/core/UnixIODevice.h>
#include <nidas/core/LooperClient.h>

#include <memory>

#ifdef HAVE_UIO48_H
#include <uio48.h>
#include <nidas/util/BitArray.h>
#endif


namespace nidas { namespace dynld {

using namespace nidas::core;

#ifdef HAVE_UIO48_H

/**
 * Class supporting UIO48 digital I/O device.
 */
class Uio48 {
public:

    Uio48(int npins = 48);
    ~Uio48();

    int getFd() const { return _fd; }

   /**
     * Open the DIO device.
     *
     * @throws nidas::util::IOException
     **/
    void open(const std::string& device);

   /**
     * Close the DIO device.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * Return number of pins on this devicec.
     * Value will be 0 if the device has not been opened,
     * or -1 if the open failed.
     */
    int getNumPins() const
    {
        return _npins;
    }

    /**
     * Clear, to low state, pins as selected by bits in which.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void clearPins(const nidas::util::BitArray& which);

    /**
     * Set, to high state, pins
     * as selected by set bits in which.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void setPins(const nidas::util::BitArray& which);

    /**
     * Set pins, selected by bits in which,
     * to 0(low) or high(1) based on corresponding bits of val
     *
     * @throws nidas::util::IOException,
     * @throws nidas::util::InvalidParameterException
     **/
    void setPins(const nidas::util::BitArray& which,
                    const nidas::util::BitArray& val);

    /**
     * Get current state of pins.
     *
     * @throws nidas::util::IOException
     **/
    nidas::util::BitArray getPins();

    const std::string& getName() const { return _devName; }

private:
    std::string _devName;
    int _fd;
    int _npins;
};
#endif

/**
 * Nidas sensor support for UIO48 digital I/O chip on a PCM-C418 Vortex CPU.
 */
class Uio48Sensor : public DSMSensor {
public:

    Uio48Sensor();
    ~Uio48Sensor();

    /**
     * Open the UIO48 device and the pipe which sends data to Nidas.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    IODevice* buildIODevice();

    SampleScanner* buildSampleScanner();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void init();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    bool process(const Sample* insamp,std::list<const Sample*>& results) throw();

#ifdef HAVE_UIO48_H

    /**
     * Close the UIO48 device and the pipe which sends data to Nidas.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    class MyIODevice: public UnixIODevice {
    public:
        MyIODevice(): UnixIODevice() {}
        // pipe is opened by sensor open method.
        void open(int) {}

        void setFd(int val) { _fd = val; }
    };

    class MyLooperClient : public LooperClient {
    public:
        MyLooperClient(const DSMSensor& sensor, Uio48& uio, int pipefd);

        void looperNotify();

        void setFd(int val) { _pipefd = val; }
    private:
        const DSMSensor& _sensor;
        Uio48& _uio;
        int _pipefd;
        std::unique_ptr<unsigned char> _buffer;
    };

#endif

private:

    int _nvars;

    SampleTag* _stag;

#ifdef HAVE_UIO48_H

    Uio48 _uio48;

    int _pipefds[2];

    MyIODevice *_iodevice;

    MyLooperClient _looperClient;
#endif

    /** No copying */
    Uio48Sensor(const Uio48Sensor&);

    /** No assignment */
    Uio48Sensor& operator=(const Uio48Sensor&);
};

}}	// namespace nidas namespace dynld

#endif // NIDAS_DYNLD_UIO48SENSOR_H
