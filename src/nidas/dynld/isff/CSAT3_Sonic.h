// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_CSAT3_SONIC_H
#define NIDAS_DYNLD_ISFF_CSAT3_SONIC_H

#include "Wind3D.h"
#include "CS_Krypton.h"

class TimetagAdjuster;

using namespace nidas::core;


namespace nidas { namespace dynld { namespace isff {

/**
 * AutoConig stuff....
 */
enum CSAT_COMMANDS {
	TERMINAL_MODE_CMD,
	DATA_MODE_CMD,
	SYSCFG_QRY_CMD,
	BAUD_RATE_CMD,
	NUM_SENSOR_CMDS
};

/**
 * A class for making sense of data from a Campbell Scientific Inc
 * CSAT3 3D sonic anemometer.
 * This also supports records which have been altered by an NCAR/EOL
 * "serializer" A2D, which digitizes voltages at the reporting rate
 * of the sonic and inserts additional 2-byte A2D counts in the
 * CSAT3 sample.
 */
class CSAT3_Sonic: public Wind3D
{
public:

    CSAT3_Sonic();

    ~CSAT3_Sonic();

    /**
     * Open the serial port connected to this sonic. open() 
     * also queries the sonic, with "??", for its status message,
     * which contains, amongst other things, the serial number and
     * the current sampling rate configuration. This information is
     * gathered as samples, and will be archived.  It is also optionally
     * logged, if a "soniclog" string parameter is specified, which
     * contains the path name of the log file.  If the user has specified
     * a rate parameter, which differs from the sonic's current configuration,
     * then a command is sent to change the rate.
     * Note that if the commands or the format of the CSAT3 sonic responses
     * change, then this method, or the stopSonic(),querySonic() or startSonic() methods
     * will likely need to be changed.
     */
    void open(int flags) throw(nidas::util::IOException,
    			       nidas::util::InvalidParameterException);

    /**
     * No correction for path curvature is needed on the CSAT,
     * so this method just returns an unchanged tc.
     */
    float correctTcForPathCurvature(float tc,
            float u, float v, float w);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void parseParameters() throw(nidas::util::InvalidParameterException);

    /**
     * Conversion factor from speed of sound squared to Kelvin.
     * See Appendix C of the "CSAT3 Three Dimensional Sonic Anemometer 
     * Instruction Manual".
     * The sonic virtual temperature Ts, can be approximated from the
     * measured speed of sound:
     *      Ts = c^2 / (Gamma_d * Rd) - 273.15
     * Where Gamma_d is the ratio of specific heat of dry air at
     * constant pressure to that at constant volume.
     * The usual approximation is:
     *      Gamma_d = 1.4
     * Rd is the gas constant for dry air, 287.04 J/K/kg:
     *      GAMMA_R = Gamma_d * Rd = 401.856
     *
     * static const float GAMMA_R = 401.856;
     * 
     * However, up to early 2012 (NIDAS revision 6420) we used 20.067^2=402.684
     * for GAMMA_R, so until we clear this matter up, we'll use that value.
    */
    static const float GAMMA_R;

    /**
     * Get serial number field and its index in "??" query output.
     */
    std::string parseSerialNumber(const std::string& str,
            std::string::size_type & index );

protected:

    void checkSampleTags() throw(nidas::util::InvalidParameterException);

    /*
     * Autoconfig overrides
     */
    virtual bool supportsAutoConfig() { return true; }
    /**
     * @return: ENTERED or ENTERED_RESP_CHECK=successful, '>' prompt received, and then no data.
     */
    virtual nidas::core::CFG_MODE_STATUS enterConfigMode() throw(nidas::util::IOException);

    /**
     * @return: none.
     */
    virtual void exitConfigMode() throw(nidas::util::IOException);
    virtual bool checkResponse();
    virtual bool installDesiredSensorConfig(const PortConfig& rDesiredConfig);
    virtual void sendScienceParameters();
    virtual bool checkScienceParameters();


private:

    /**
     * Send a "??CR" string, and read the response, parsing out the
     * acquisition rate, osc parameter, serial number and the software revision.
     *
     * rtsIndep is the setting of the "ri" parameter. For our free-running
     * 3-wire mode, we want it to be ri=1, so that the RS232 drivers
     * on the sonic are always on, not dependent on RTS.
     *
     * recSep is the setting of the "rs" parameter. NIDAS expects a record
     * separator (0x55AA), so it is set to rs=1.
     *
     * Both of these can be set in EEPROM, following the procedure in
     * section 12 of the CSAT3 manual.
     */
    std::string querySonic(int& acqrate, char& osc, std::string& serialNumber,
    			   std::string& revsion, int& rtsIndep, int& recSep, int& baudRate) throw(nidas::util::IOException);

    const char* getRateCommand(int rate,bool overSample) const;

    void sendBaudCmd(int baud);
    void sendRTSIndepCmd(bool on=true);
    void sendRecSepCmd();
    std::string sendRateCommand(const char* cmd) throw(nidas::util::IOException);

    /**
     * expected input sample length of basic CSAT3 record.
     */
    size_t _windInLen;

    /**
     * expected input sample length of basic CSAT3 record,
     * with any additional fields added by NCAR/EOL "serializer" A2D.
     */
    size_t _totalInLen;

    /**
     * Requested number of output wind variables.
     */
    int _windNumOut;

    /**
     * If user requests despike variables, e.g. "uflag","vflag","wflag","tcflag",
     * the index of "uflag" in the output variables.
     */
    int _spikeIndex;

    /**
     * Output sample id of the wind sample.
     */
    dsm_sample_id_t _windSampleId;

    /**
     * Sample tags of extra "serializer" values.
     */
    std::vector<SampleTag*> _extraSampleTags;

    dsm_time_t _timetags[2];

    int _nttsave;

    int _counter;

#if __BYTE_ORDER == __BIG_ENDIAN
    std::vector<short> _swapBuf;
#endif

    int _rate;

    bool _oversample;

//    Move to DSMSensor so that all sensors can collect this metadata
//    std::string _serialNumber;

    std::string _sonicLogFile;

    /**
     * This is a limit for the inter-sample delta-T. If the delta-T is greater
     * than this value, then the saved values of the previous two sample
     * time tags are discarded, and the correction for the 2 sample
     * internal CSAT3 buffer is restarted, resulting in a discard of the
     * previous 2 samples.
     */
    int _gapDtUsecs;

    /**
     * Last time tag.
     */
    dsm_time_t _ttlast;

    /**
     * Set winds and virtual temperature to NaN if
     * diagnostic value is non-zero?
     */
    bool _nanIfDiag;

    /**
     * Counter of how many times a open fails because it could't query the sonic serial
     * number, and then get recognizable samples.
     */
    int _consecutiveOpenFailures;

    /**
     * Whether to log the sonic parameters and set the rate in the open method.
     * If false, simple open the port.
     */
    bool _checkConfiguration;

    /**
     *  Whether to check the counter in the data samples in order to
     *  detect missing samples. If so, 16 is added to the diagbits variable
     *  if the counter is other than the last counter + 1, mod 64.
     */
    bool _checkCounter;

    nidas::core::TimetagAdjuster* _ttadjuster;

    /**
     * Serial port autoconfig items
     */
    static const PortConfig DEFAULT_PORT_CONFIG;
    static const PORT_TYPES DEFAULT_PORT_TYPE = RS232;
    static const int DEFAULT_BAUD_RATE = 9600;
    static const int DEFAULT_DATA_BITS = 8;
    static const Termios::parity DEFAULT_PARITY = Termios::NONE;
    static const int DEFAULT_STOP_BITS = 1;
    static const TERM DEFAULT_LINE_TERMINATION = NO_TERM;
//    static const SENSOR_POWER_STATE DEFAULT_SENSOR_POWER = SENSOR_POWER_ON;
    static const int DEFAULT_RTS485 = -1; // De-assert, but don't mess w/this when writing to the port
    static const bool DEFAULT_CONFIG_APPLIED = false;

    MessageConfig defaultMessageConfig;
    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 10;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHARS;

    static const int NUM_SENSOR_BAUDS = 2;
    static const int SENSOR_BAUDS[NUM_SENSOR_BAUDS];
    static const int NUM_WORD_SPECS = 1;
    static const WordSpec SENSOR_WORD_SPECS[NUM_WORD_SPECS];
    static const int NUM_PORT_TYPES = 1;
    static const PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    /**
     *  Attributes for querySensor. Need to add them as attributes because
     *  there may well be more than one of these sensors per DSM. This
     *  means that these cannot be file scope variables.
     */
    char* rateCmd;
    int acqrate;
    std::string serialNumber;
    char osc;
    std::string revision;
    int rtsIndep;
    int recSep;
    int baudRate;

    /**
     * No copying.
     */
    CSAT3_Sonic(const CSAT3_Sonic&);

    /**
     * No assignment.
     */
    CSAT3_Sonic& operator=(const CSAT3_Sonic&);
};

}}}	// namespace nidas namespace dynld namespace isff

#endif
