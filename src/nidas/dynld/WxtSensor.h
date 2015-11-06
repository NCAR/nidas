// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_WXTSENSOR_H
#define NIDAS_DYNLD_WXTSENSOR_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

using nidas::dynld::DSMSerialSensor;
using nidas::util::IOException;
using nidas::util::InvalidParameterException;

/**
 * Vaisala WXT weather sensor.
 *
 * This is a serial sensor, but the messages contain multiple fields, and
 * one or more of the fields may be missing values.  The missing value is
 * indicated by a '#' in place of the Units character specifier.  This
 * makes it infeasible to parse the message with the normal serial sensor
 * scanf string.  Instead, the scanf format string is comma-separated along
 * with the raw data message, and variables are scanned individually.
 *
 * Support is also provided for derivation of wind U,V components
 * from wind speed and direction. To produce a derived U and V, add
 * a <sample> element to the WxtSensor without a scanFormat attribute,
 * containing the U and V variables:
 /code
    <serialSensor ID="WXT510" class="WxtSensor" ... >
        <parameter name="u" type="string" value="wu"/>
        <parameter name="v" type="string" value="wv"/>
        <!-- sample for values parsed from WxT -->
        <sample id="1" scanfFormat="0R0,Dn=%fD,Dm=%fD,Dx=%fD,Sn=%fM,Sm=%fM,...">
           <variable name="wdir_min" longname="Wind direction minimum" units="m/s"/>
           ...
        </sample>
        <!-- sample with derived U,V -->
        <sample id="2">
           <variable name="wu" longname="Wind U component" units="m/s"/>
           <variable name="wv" longname="Wind V component" units="m/s"/>
        </sample>
    </serialSensor>
 /endcode
 * The WxtSensor code makes no assumption about which sample contains the
 * U and V variables, and their names. As above, you must tell NIDAS
 * the names of the U and V variables via the "u" and "v" parameters.
 * The u and v are calculated from the wind speed and direction as:
/code
       float u = -spd * ::sin(dir * M_PI / 180.0);
       float v = -spd * ::cos(dir * M_PI / 180.0);
/endcode
 * These calculations are done after the speed and direction have been
 * corrected by any optional linear or polynomical conversion with
 * a possible cal file.
 */
class WxtSensor: public DSMSerialSensor
{

public:

    WxtSensor();

    ~WxtSensor();

    int
    scanSample(AsciiSscanf* sscanf, const char* inputstr, float* data_ptr);

    void init() throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp, std::list<const Sample*>& results) throw();

    const std::string& getUName() const { return _uName; }

    void setUName(const std::string& val) { _uName = val; }

    const std::string& getVName() const { return _vName; }

    void setVName(const std::string& val) { _vName = val; }

private:

    /**
     * For each <sample> with an scanfFormat, the parse format
     * tokenized by commas.
     */
    std::map<dsm_sample_id_t,std::vector<std::string> > _field_formats;

    /**
     * Initial characters in wind U component variable name.
     */
    std::string _uName;

    /**
     * Initial characters in wind V component variable name.
     */
    std::string _vName;

    /**
     * Index of wind speed in output sample.
     */
    int _speedIndex;

    /**
     * Index of wind direction in output sample.
     */
    int _dirIndex;

    /**
     * Index of wind u component in output sample.
     */
    int _uIndex;

    /**
     * Index of wind v component in output sample.
     */
    int _vIndex;

    /**
     * ID of sample containing wind speed, direction.
     */
    dsm_sample_id_t _speedDirId;

    /**
     * ID of sample containing wind U,V components.
     */
    dsm_sample_id_t _uvId;

    /**
     * Length of output U,V sample.
     */
    int _uvlen;

};

}}	// namespace nidas namespace dynld

#endif // NIDAS_DYNLD_WXTSENSOR_H
