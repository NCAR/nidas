// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2014, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_NCAR_TRH_H
#define NIDAS_DYNLD_ISFF_NCAR_TRH_H

#include <vector>
#include <string>
#include <boost/regex.hpp>

#include <nidas/core/SerialSensor.h>
#include <nidas/core/VariableConverter.h>

namespace nidas { namespace dynld { namespace isff {

class HandleRawT;
class HandleRawRH;

enum TRH_SENSOR_COMMANDS : unsigned short;

/**
 * Sensor class for the NCAR hygrothermometer, built at EOL.
 * This hygrothermometer has an electrical aspiration fan, and reports
 * the current used by the fan in its output message, along with the values
 * for temperature and relativef humidity. If the fan current is outside
 * an acceptable range, as specified by the getMinValue(),getMaxValue()
 * settings of the Ifan variable, then any value other than the fan current
 * is set to the missing value. The idea is to discard temperature and relative
 * humidity when aspiration is not sufficient.
 */
class NCAR_TRH: public nidas::core::SerialSensor
{
public:

    NCAR_TRH();

    ~NCAR_TRH();

    void validate() throw(nidas::util::InvalidParameterException);

    bool process(const nidas::core::Sample* samp,
                 std::list<const nidas::core::Sample*>& results) throw();

    void
    ifanFilter(std::list<const nidas::core::Sample*>& results);

    double
    tempFromRaw(double traw);

    double
    rhFromRaw(double rhraw, double temp_cal);
    
    /**
     * Set the three polynomial coefficients which enable converting the
     * raw temperature counts variable Traw into the calibrated temperature
     * variable T.  @p begin points to an array of three coefficients, in
     * order a0, a1, and a2, up to but not including @p end.  Pass @p
     * end to set less than all three coefficients, otherwise @p begin must
     * point to an array with at least 3 values.  Pass @begin as null to
     * disable raw temperature conversions.
     **/
    void
    setRawTempCoefficients(float* begin = 0, float* end = 0);

    /**
     * Set the five polynomial coefficients which enable converting the raw
     * temperature RH variable RHraw into the calibrated humidity variable
     * RH.  @p begin points to an array of three coefficients, in order
     * ha0, ha1, ha2, ha3, and ha4, up to but not including @p end.  Pass
     * @p end to set less than all five coefficients, otherwise @p begin
     * must point to an array with at least 5 values.  Pass @begin as null
     * to disable raw humidity conversions.
     **/
    void
    setRawRHCoefficients(float* begin = 0, float* end = 0);

    std::vector<float>
    getRawTempCoefficients();

    std::vector<float>
    getRawRHCoefficients();

    typedef nidas::core::SensorCmdArg SensorCmdArg;

    /*
     *  AutoConfig - TRH-specific methods.
     */
    void sendSensorCmd(int cmd, nidas::core::SensorCmdArg arg=SensorCmdArg(), bool resetNow=false);
    bool sendAndCheckSensorCmd(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg=SensorCmdArg());
    bool checkCmdResponse(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg);
    void initCustomMetadata();
    bool captureEepromMetaData(const char* buf);
    void updateMetaData();

    /**
     * @brief Tell the TRH sensor to enter eeprom menu mode.
     * 
     * @return true if successful, false otherwise.
     */
    bool enterMenuMode();

protected:
    /*
     *  AutoConfig - Virtual overrides of SerialSensor methods.
     */
    virtual void fromDOMElement(const xercesc::DOMElement* node)
                    throw(nidas::util::InvalidParameterException);
    virtual bool checkResponse();
    virtual nidas::core::CFG_MODE_STATUS enterConfigMode();

    // No need to override sendScienceParameters() and checkScienceParameters()
    // because there are none, so the default implementations suffice.

private:

    void convertVariable(nidas::core::SampleT<float>* outs,
                         nidas::core::Variable* var, float* fp);

    bool handleRawRH(nidas::core::CalFile* cf);
    bool handleRawT(nidas::core::CalFile* cf);
    
    typedef nidas::core::VariableIndex VariableIndex;
    typedef nidas::core::CalFileHandler CalFileHandler;
    
    /**
     * In the validate() method the variables generated by this sensor
     * class are scanned, and if one matches "Ifan" then its index, min and
     * max values are copied to these class members. The min, max limit
     * checks for the Ifan variable are then removed (actually expanded to
     * the limits for a float value), so that the Ifan variable is itself
     * not overwritten with floatNAN if it exceeds the limit checks, but
     * the T and RH values are.
     */
    VariableIndex _ifan;

    float _minIfan;
    float _maxIfan;

    /**
     * The indices of the raw T and RH variables are saved in case a
     * calibration record specifies raw conversions, where the processed T
     * and RH variables are replaced with a conversion from the raw values;
     **/
    VariableIndex _traw;
    VariableIndex _rhraw;

    /**
     * Indices of the processed T and RH variables, in case they need to be
     * replaced with a direct conversion from the raw counts.
     **/
    VariableIndex _t;
    VariableIndex _rh;

    /**
     * Coefficients for calculating T and RH from the raw counts.
     **/
    std::vector<float> _Ta;
    std::vector<float> _Ha;

    CalFileHandler* _raw_t_handler;
    CalFileHandler* _raw_rh_handler;

    std::vector<VariableIndex> _compute_order;
    
    void convertNext(const VariableIndex& vi);

    /*
     * Autoconfig 
     */
    bool _checkSensorCmdResponse(TRH_SENSOR_COMMANDS cmd, SensorCmdArg arg, 
                                 const boost::regex& matchStr, int matchGroup, 
                                 const char* buf);
    bool handleEepromExit(const char* buf, const int bufSize);

    // no copying
    NCAR_TRH(const NCAR_TRH& x);

    // no assignment
    NCAR_TRH& operator=(const NCAR_TRH& x);

};

}}} // namespace nidas namespace dynld namespace isff

#endif
