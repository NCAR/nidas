// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_CSI_IRGA_SONIC_H
#define NIDAS_DYNLD_ISFF_CSI_IRGA_SONIC_H

#include "CSAT3_Sonic.h"

#include <nidas/Config.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/core/Sample.h>
#include <nidas/core/VariableIndex.h>

class TimetagAdjuster;

namespace nidas { namespace dynld { namespace isff {

/**
 * CSI_IRGA_Fields holds the fields reported by the CSI IRGA Sonic in their
 * native types.
 */
struct CSI_IRGA_Fields
{
    const float floatNAN = nidas::core::floatNAN;

    float u { floatNAN };
    float v { floatNAN };
    float w { floatNAN };
    float tc { floatNAN };
    u_int32_t diagbits { 0 };
    float h2o { floatNAN };
    float co2 { floatNAN };
    u_int32_t irgadiag { 0 };
    float Tirga { floatNAN };
    float Pirga { floatNAN };
    float SSco2 { floatNAN };
    float SSh2o { floatNAN };
    float dPirga { floatNAN };
    // these are not actually in the record any more, as of ages ago
    // float Tsource { floatNAN };
    // float Tdetector { floatNAN };
    u_int32_t counter { 0 };

    /// Number of valid fields (when unpacked from a sonic message)
    int nvals { 0 };

    template <typename F>
    void visit(F& f);
};


struct SonicStatistics
{
    /**
     * Initialize all counts to zero, and set restart_counter to true.
     * @param name A name or other identifier for these statistics, such as
     * the sensor device.
     */
    SonicStatistics(const std::string& name = "");

    /**
     * Increase message count and set the message time.
     */
    void addMessage(dsm_time_t message_time);

    /**
     * Return a string representation of the time of the most recent message.
     */
    std::string timeString() const;

    /**
     * Check the sequence counter for continuity, update statistics, and
     * return the masked counter suitable for assigning to a float variable.
     */
    uint32_t
    checkCounter(uint32_t sonic_counter);

    /**
     * Add to the bad CRC count, logging the current count periodically.
     * Return false.
     * @param reason A reason for the bad CRC to include in the log message.
     */
    bool reportBadCRC(const std::string& reason = "checksum failure");

    /**
     * Return a string with the current statistics.
     */
    std::string toString() const;

    /**
     * Log the current statistics.  If there are bad CRCs or missed messages,
     * then log as a warning, otherwise log as info.  This can be called
     * periodically and upon sensor destruction to log the statistics.
     */
    void logStatistics() const;

    std::string name {};

    /// @brief Number of messages processed by this sensor, including those
    /// with bad CRC signatures.
    unsigned int nmessages {0};

    /// @brief Time of the most recent message received.
    dsm_time_t message_time {0};

    /// @brief Number of messages which failed checksums.
    unsigned int badCRCs {0};

    /// @brief The most recent counter value seen from this sensor.
    unsigned int counter {0};

    /// @brief Number of missed messages based on missing counters.
    unsigned int missed_messages {0};

    /**
     * When true, take the next counter as the start of a new sequence rather
     * than checking for continuity with the previous counter.  This is set at
     * startup before the initial counter is valid, but it could also be set
     * after significant time delays or any indication that the sonic or data
     * stream were restarted.
     */
    bool restart_counter {true};
};


/**
 * A class for making sense of data from a Campbell Scientific
 * IRGASON integrated Gas Analyzer and 3D sonic anemometer.
 */
class CSI_IRGA_Sonic: public CSAT3_Sonic
{
public:

    CSI_IRGA_Sonic();

    ~CSI_IRGA_Sonic();

    void open(int flags);

    void parseParameters();

    void checkSampleTags();

    bool process(const Sample* samp,std::list<const Sample*>& results);

    virtual void updateAttributes() override;

    /**
     * Unpack the binary buffer @p buf into @p fields, up until the end of
     * buffer @p eob.  Return the number of fields successfully unpacked. If
     * the CRC signature check fails or the buffer is too short, the return
     * value is zero.
     */
    int
    unpackBinary(const char* buf, const char* eob, CSI_IRGA_Fields& fields);

    /**
     * Pack @p fields into the binary buffer @p buf.  Return the number of
     * fields packed, which should be equal to the number of valid fields in
     * @p fields (nvals). The CRC signature and termination bytes are added at
     * the end of the buffer.
     */
    int
    packBinary(const CSI_IRGA_Fields& fields, std::vector<char>& buf);

    /**
     * Return the current statistics for this sonic sensor.
     */
    SonicStatistics getStatistics();

    /**
     * Calculate the CRC signature of a data record. From EC150 manual.
     */
    static unsigned short signature(const unsigned char* buf, const unsigned char* eob);

    // max value of counter that can be preserved in a 32-bit float variable
    // with a 23-bit mantissa and an implied 1 leading bit.  this is the value
    // at which the counter will roll over to zero.
    static constexpr uint32_t MAX_COUNTER = ((1U << 24) - 1);

private:

    typedef nidas::core::VariableIndex VariableIndex;

    /**
     * Requested number of output variables.
     */
    unsigned int _numOut;

    /**
     * Filter time delay, depends on the selected bandwidth.
     * From the Campbell Scientific manual: "EC150 CO2 and H2O Open-Path
     * Gas Analyzer and EC100 Electronics with Optional CSAT3A 3D
     * Sonic Anemometer".
     * bandwidth(Hz)    delay(ms)
     *    5                800
     *    10               400
     *    12.5             320
     *    20               200
     *    25               160
     */
    int _timeDelay;

    SonicStatistics _stats;

    /// integer mask for IRGA diagnostic bits to ignore
    unsigned int _irgaDiagMask;

    VariableIndex _irgaDiag;
    VariableIndex _h2o;
    VariableIndex _co2;
    VariableIndex _Pirga;
    VariableIndex _Tirga;

    VariableIndex _Tirga_src;
    VariableIndex _Tirga_det;

    /**
     * Campbell has provided custom firmware on the EC100 logger box so that
     * it can generate binary values (IEEE floats and 4-byte integers)
     * instead of ASCII.
     */
    bool _binary;

    /**
     * Endian-ness of binary values.
     */
    nidas::util::EndianConverter::endianness _endian;

    /**
     * Converter for binary values.
     */
    const nidas::util::EndianConverter* _converter;

    nidas::core::TimetagAdjuster* _ttadjust;

    /// No copying
    CSI_IRGA_Sonic(const CSI_IRGA_Sonic &);

    /// No assignment.
    CSI_IRGA_Sonic& operator = (const CSI_IRGA_Sonic &);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
