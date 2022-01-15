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
/*
 * device name, baud, parity, data, stop, 485
 * parameters:
 *  slave id, default 1
 *  register address, default 0
 *
 *  read as many uint16's as there are variables in the sample
 *      conversion:  treat data as signed, unsigned?
 */
 
#include "ModbusRTU.h"
#include <nidas/core/Variable.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>
#include <nidas/util/EndianConverter.h>

#include <sstream>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::Logger;

NIDAS_CREATOR_FUNCTION(ModbusRTU)

/*
 * modbus_read_registers apparently converts from modbus endianness,
 * which is big endian, to the endianness of the host. We want the
 * archived data to have a defined endianness, not depending on
 * the endianness of the host it was acquired on.  It is quite unlikely
 * that we'll use big-endian hosts, but just to be paranoid we'll
 * convert all data to little-endian, which is a no-op for little-endian hosts.
 */
static const n_u::EndianConverter *toLittle = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::getHostEndianness(),
        n_u::EndianConverter::EC_LITTLE_ENDIAN);

static const n_u::EndianConverter *toHost = n_u::EndianConverter::getConverter(
        n_u::EndianConverter::EC_LITTLE_ENDIAN,
        n_u::EndianConverter::getHostEndianness());

ModbusRTU::ModbusRTU():SerialSensor(),
    _modbusrtu(0), _slaveID(1), _regaddr(0),
    _thread(0), _pipefds(),
    _nvars(0), _stag(0), _converters()
{
}

void ModbusRTU::validate()
{
    SerialSensor::validate();

    if (getSampleTags().size() != 1)
        throw n_u::InvalidParameterException(getName(), "sample",
            "must have exactly one sample");
    const SampleTag* stag = *getSampleTags().begin();

    _nvars = (uint16_t) stag->getVariables().size();

}

void ModbusRTU::init()
{
    _stag = *getSampleTags().begin();

    // get the converters
    const vector<Variable*>& vars = _stag->getVariables();
    vector<Variable*>::const_iterator iv = vars.begin();

    _converters.resize(vars.size());

    for (int i = 0; iv != vars.end(); ++iv, i++) {
        Variable* var = *iv;
        _converters[i] = var->getConverter();
    }

    SerialSensor::init();
}

class MyIODevice: public UnixIODevice {
public:
    MyIODevice(int fd): UnixIODevice() { _fd = fd; }
    // pipe is already open
    void open(int) {}
};

IODevice* ModbusRTU::buildIODevice()
{
    if (::pipe(_pipefds) < 0)
        throw n_u::IOException(getDeviceName(), "pipe", errno);

    return new MyIODevice(_pipefds[0]);
}

SampleScanner* ModbusRTU::buildSampleScanner()
{
    MessageStreamScanner* scanner = new MessageStreamScanner();

    // Format of data written by writer thread is a simple buffer,
    // consisting of all little-endian, words of uint16_t. 
    // The first word is the number of values, nvars, followed by nvars of data.
    //
    // Use the little endian uint16_t nvars at the beginning of the
    // data buffer as the message separator for nidas to use when parsing messages.
    // The message length (which doesn't include the separator) is then 2 * nvars.
    uint16_t nvars_le = toLittle->uint16Value(_nvars);
    string sepstr((const char*) &nvars_le, 2);
    scanner->setMessageParameters(_nvars * sizeof(uint16_t), sepstr, false);
    return scanner;
}

void ModbusRTU::open(int flags)
{
    const n_u::Termios& termios = getTermios();

    int baud = termios.getBaudRate();
    char parity = 'N';

    switch (termios.getParity()) {
        case termios.NONE: parity = 'N'; break;
        case termios.ODD: parity = 'O'; break;
        case termios.EVEN: parity = 'E'; break;
        default: break;
    }
    int data = termios.getDataBits();
    int stop = termios.getStopBits();

    _modbusrtu = modbus_new_rtu(getDeviceName().c_str(), baud, parity,
            data, stop);
    if (!_modbusrtu) throw n_u::IOException(getDeviceName(), "modbus_new_rtu", errno);

    const Parameter* param = getParameter("slaveID");
    if (param) {
        if (param->getLength() != 1)
            throw nidas::util::InvalidParameterException(
                getName(), "parameter",
                "bad length for slaveID");
        _slaveID = (int) param->getNumericValue(0);
    }
    param = getParameter("register");
    if (param) {
        if (param->getLength() != 1)
            throw nidas::util::InvalidParameterException(
                getName(), "parameter",
                "bad length for register");
        _regaddr = (int) param->getNumericValue(0);
    }

    if (modbus_set_slave(_modbusrtu, _slaveID) < 0) 
        throw n_u::IOException(getDeviceName(), "modbus_set_slave", errno);

    if (modbus_connect(_modbusrtu) < 0) 
        throw n_u::IOException(getDeviceName(), "modbus_connect", errno);

    _thread = new ModbusThread(getDeviceName() + " read thread", 
            _modbusrtu, _regaddr, _nvars, _pipefds[1]);

    _thread->setRealTimeFIFOPriority(40);

    try {
        _thread->start();
    }
    catch(n_u::Exception& e) {
    }

    SerialSensor::open(flags);
}

void ModbusRTU::close()
{
    _thread->interrupt();
    
    if (_pipefds[0] > 0) ::close(_pipefds[0]);
    if (_pipefds[1] > 0) ::close(_pipefds[1]);

    _thread->cancel();
    _thread->join();

    modbus_close(_modbusrtu);
    modbus_free(_modbusrtu);
}

int ModbusRTU::ModbusThread::run() throw()
{
    uint16_t data[_nvars + 1];
    data[0] = toLittle->uint16Value(_nvars);

    for (; !isInterrupted(); ) {
        int nreg = modbus_read_registers(_mb, _regaddr, _nvars, data + 1);
        if (nreg < 0) {
            n_u::IOException e(_devname, "modbus_read_registers", errno);

            PLOG(("Error: ") << e.what());
            return RUN_EXCEPTION;
         }

        // convert to little-endian
        for (int i = 0; i < _nvars; i++)
            data[i+1] = toLittle->uint16Value(data[i+1]);

        if (::write(_pipefd, data, (_nvars + 1) * sizeof(uint16_t)) < 0) {
            n_u::IOException e(_devname + " pipe", "write", errno);
            PLOG(("Error: ") << e.what());
        }
        ::sleep(1);
    }
    return RUN_OK;
}

bool ModbusRTU::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
    assert(samp->getType() == CHAR_ST);

    uint16_t nwords = samp->getDataLength() / sizeof(uint16_t);

    if (nwords == 0) return false;

    const uint16_t* inwords = (const uint16_t*) samp->getConstVoidDataPtr();

    SampleT<float>* outsamp = getSample<float>(_nvars);

    outsamp->setTimeTag(samp->getTimeTag());
    outsamp->setId(_stag->getId());

    adjustTimeTag(_stag, outsamp);

    uint16_t nvarsin = toHost->uint16Value(inwords[0]);
    if (nvarsin != _nvars)
        WLOG(("%s: ",getDeviceName()) << "nvarsin=" << nvarsin << ", _nvars=" << _nvars);

    if (nvarsin != nwords-1)
        WLOG(("%s: ",getDeviceName()) << "nvarsin=" << nvarsin << ", nwords=" << nwords);

    for (int i = 0; i < std::min(std::min((int)nvarsin, (int)_nvars), (int)nwords - 1); i++) {
        float val = (float) toHost->uint16Value(inwords[i+1]);
        VariableConverter* conv = _converters[i];

        if (!conv) outsamp->getDataPtr()[i] = val;
        else outsamp->getDataPtr()[i] =
            conv->convert(outsamp->getTimeTag(),val);
    }
    results.push_back(outsamp);
    return true;
}
#endif
