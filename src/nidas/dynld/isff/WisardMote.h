/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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
/*

 Sensor class supporting EOL "Wisard" motes.

 This code handles the serial messages from a base radio for a set of motes.

 The raw messages from each mote have a general format of

     ID N ':' version msgtype data CRC terminator

 ID is a non-numeric string of ASCII characters, typically "ID".

 N is one or more ascii decimal digits, specifying the number of the
 originating mote. Each mote's number should be unique for the set of motes
 being acquired by a data system. As explained below the mote
 number should be in the range 1-127.

 version is a 1 byte integer indicating the message format version
 number.

 msgtype is another 1 byte integer, where 1 indicates a data message.

 The data portion is a concatenation of one or more sensor data
 fields. Each sensor data field consists of a 8 bit sensor type
 followed by the data for that sensor type, where the format and
 length of the data is defined for each sensor type.

 The full id of the raw samples will contain the DSM id in the top 16
 bits, with the DSM sensor id in the low order 16 bits.
 By convention we use a DSM sensor id of 0x8000 for all base motes
 attached to a data system.

 The process() method of this class unpacks the raw samples, generating
 a floating point sample for each sensor data field found in the raw
 message.

 The NIDAS sample id provides 16 bits to uniquely identify a sample
 on a DSM.  In order to manage these ids we have established a convention
 where the processed samples will have a DSM sensor id of 0x8000,
 leaving 7 bits for the mote number (1-127), and 8 bits for the sensor type.

 Mote number of 0 is used in the configuration to specify variable names
 for a sensor type which should be used for all motes. Therefore
 a physical mote should not have an id of 0.

 */

#ifndef NIDAS_DYNLD_ISFF_WISARDMOTE_H
#define NIDAS_DYNLD_ISFF_WISARDMOTE_H

#include <nidas/core/SerialSensor.h>
#include <nidas/core/Sample.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <exception>

#include <limits>
#include <list>

#include <regex>

using namespace std;

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/*
 * AutoConfig enums, etc
 */

enum MOTE_CMDS
{
	NULL_CMD = -1, // don't include in any command tables

    // Sampling Rate Cmds
    DATA_RATE_CMD,
    PWR_SAMP_RATE_CMD,
    SERNUM_RATE_CMD,

    // Operating Mode Cmds
//    SAMP_MODE_CMD,  // Doesn't work????
    NODE_ID_CMD,
    MSG_FMT_CMD,
    OUT_PORT_CMD,
    SENSORS_ON_CMD,

    // Local file Cmds
    MSG_STORE_CMD,
    MSG_STORE_FLUSHRATE_CMD,

    // Battery Monitor Cmds
    VMON_ENABLE_CMD,
    VMON_LOW_CMD,
    VMON_RESTART_CMD,
    VMON_SLEEP_CMD,

    // Calibration
    ADCALS_CMD,
    VBG_CAL_CMD,
    IIG_CAL_CMD,
    I3G_CAL_CMD,

    // EEPROM
    EE_CFG_CMD,
    EE_UPDATE_CMD,
    EE_INIT_CMD,
    EE_LOAD_CMD,

//    // Xbee Radio Cmds
//    XB_AT_CMD,
//    XB_RESET_TIMEOUT_CMD,
//    XB_STATUS_NOW_CMD,
//    XB_REBOOT_CMD,
//    XB_HEARTBEAT_MSG_CMD,
//    XB_RADIO_CMD,

    // Bluetooth Radio
    BT_INTERACTIVE_CMD,
    BT_CMD_MODE,
    BT_GET_MACADDR,
    BT_GET_NAME,
    BT_SET_NAME,
    BT_GET_RFPWR,
    BT_SET_RFPWR,
    BT_SET_DATAMODE,
    BT_EXIT_BTRADIO,

    // GPS/Timing Cmds
    GPS_ENABLE_CMD,
    GPS_SYNC_RATE_CMD,
    GPS_LCKTMOUT_CMD,
    GPS_LCKFAIL_RETRY_CMD,
    GPS_NLOCKS_CNFRM_CMD,
    GPS_SENDALL_MSGS_CMD,

    // List commands, reset
    LIST_CMD,
    RESET_CMD,
    REBOOT_CMD,
    SENSOR_SRCH_CMD,
    NUM_SUPPORTED_CMDS
};

/*
 *  Used to keep track of what config data has been captured already,
 *  and whether a command needs to be re-sent. If the metadata is same
 *  as what is to be sent, then the command is canceled.
 */
struct MoteSensorConfigMetaData : public SensorConfigMetaData
{
    MoteSensorConfigMetaData()
    : _eeCfg("Not Available"), _dataRateCfg("Not Available"), _pwrSampCfg("Not Available"),
      _serNumSampCfg("Not Available"), _idCfg("Not Available"), _msgFmtCfg("Not Available"),
      _portCfg("Not Available"), _sensorsOnCfg("Not Available"), _fileEnableCfg("Not Available"),
      _fileFlushCfg("Not Available"), _vmonEnableCfg("Not Available"), _vmonLowCfg("Not Available"),
      _vmonRestartCfg("Not Available"), _vmonSleepCfg("Not Available"), _vbCalCfg("Not Available"),
      _i3CalCfg("Not Available"), _iiCalCfg("Not Available"), _gpsEnableCfg("Not Available"),
      _gpsResyncCfg("Not Available"), _gpsFailRetryCfg("Not Available"),
      _gpsNumMsgsToLockCfg("Not Available"), _gpsTimeOutCfg("Not Available"),
      _gpsSendAllMsgsCfg("Not Available")
    {/*Intentionally Left Blank*/}
    virtual ~MoteSensorConfigMetaData() {}

    std::string _eeCfg;
    std::string _dataRateCfg;
    std::string _pwrSampCfg;
    std::string _serNumSampCfg;
    std::string _idCfg;
    std::string _msgFmtCfg;
    std::string _portCfg;
    std::string _sensorsOnCfg;
    std::string _fileEnableCfg;
    std::string _fileFlushCfg;
    std::string _vmonEnableCfg;
    std::string _vmonLowCfg;
    std::string _vmonRestartCfg;
    std::string _vmonSleepCfg;
    std::string _vbCalCfg;
    std::string _i3CalCfg;
    std::string _iiCalCfg;
    std::string _gpsEnableCfg;
    std::string _gpsResyncCfg;
    std::string _gpsFailRetryCfg;
    std::string _gpsNumMsgsToLockCfg;
    std::string _gpsTimeOutCfg;
    std::string _gpsSendAllMsgsCfg;
};


/*
 *  Mote operational details
 */

struct VarInfo
{
    const char *name;
    const char *units;
    const char *longname;
    const char *plotrange;
};

/**
 * WST_IMPLIED: create sample tags for these sensor types, even if they
 *          aren't found in the XML.
 * WST_IGNORED: if a raw sample of this sensor type is found, don't 
 *      generate a processed sample.
 * WST_NOWARN: if a raw sample of this sensor type is found, generate
 *      a processed sample, but don't log a warning.
 * WST_NORMAL: otherwise.
 */
enum WISARD_SAMPLE_TYPE { WST_NORMAL, WST_IMPLIED, WST_IGNORED, WST_NOWARN };

struct SampInfo
{
    /**
     * firstst and lastst are the range of sensor types
     * containing the list of variables.
     */
    int firstst;
    int lastst;
    struct VarInfo variables[9];
    enum WISARD_SAMPLE_TYPE type;
};

class WisardMote : public SerialSensor
{
public:
    WisardMote();

    virtual ~ WisardMote();

    bool process(const Sample* insamp,
                 std::list<const Sample*>& results) throw();

    void validate() throw (nidas::util::InvalidParameterException);

    virtual const SensorConfigMetaData& getSensorConfigMetaData() const
    {
        return _configMetaData;
    }

    /**
     * Extracted fields from the initial portion of a Wisard message.
     */
    struct MessageHeader {
        int moteId;
        int version;
        int messageType;
    };

    /**
     * typedef for the functions that parse the message data
     * for each sensor type.
     */
    typedef const char *(WisardMote::*unpack_t)
                (const char *cp, const char *eos,
                    unsigned int nfields,
                    const struct MessageHeader* hdr,
                    SampleTag* stag, SampleT< float >* osamp);

    static const nidas::util::EndianConverter * fromLittle;

protected:
    virtual void fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException);

    /*
     * AutoConfig helpers
     */
    virtual bool supportsAutoConfig() { return true; }
    virtual CFG_MODE_STATUS enterConfigMode();
    virtual void exitConfigMode();
    virtual bool checkResponse();
    // There is only one configuration, so this is always successful
    virtual bool installDesiredSensorConfig(const PortConfig& /*rDesiredConfig*/) { return true; }
    virtual void sendScienceParameters();
    virtual void sendEpilogScienceParameters();
    virtual bool checkScienceParameters();

    void initCmdTable();
    void initCustomMetaData();
    void initScienceParams();
    void initPortCfgParams();
    inline void initAutoCfg()
    {
    	initCmdTable();
    	initPortCfgParams();
    	initScienceParams();
    }
    void updateScienceParameter(const MOTE_CMDS cmd, const SensorCmdArg& arg = SensorCmdArg());
    void sendSensorCmd(MOTE_CMDS cmd, SensorCmdArg arg = SensorCmdArg());
    bool sendAndCheckSensorCmd(MOTE_CMDS cmd, SensorCmdArg arg = SensorCmdArg());
    // Need a special method for this parameter, as it is a toggle,
    // or there is no way to query it. Nor is it a part of the eecfg command response.
    bool _checkSensorCmdResponse(MOTE_CMDS cmd, SensorCmdArg arg, const regex& matchStr, int matchGroup, const char* buf);
    bool checkCmdResponse(MOTE_CMDS cmd, SensorCmdArg arg = SensorCmdArg());
    bool checkIfCmdNeeded(MOTE_CMDS cmd, SensorCmdArg arg = SensorCmdArg());
    bool captureResetMetaData(const char* buf);
    bool captureCfgData(const char* buf);
    void updateCfgParam(MOTE_CMDS cmd, std::string val);

private:

    /**
     * Create SampleTags from the configuration SampleTag.
     * This SampleTag may have "stypes" and "motes" Parameters, indicating that
     * actual SampleTags should be created for each mote and sample type.
     * @param motes: vector of mote numbers from the "motes" sensor parameter.
     */
    void createSampleTags(const SampleTag* stag,const std::vector<int>& motes,std::list<SampleTag*>& newtags)
        throw (nidas::util::InvalidParameterException);

    /**
     * Add processed sample tags for all sensor types indicated
     * as WST_IMPLIED in _samps.
     */
    void addImpliedSampleTags(const std::vector<int>& motes);

    /**
     * Create sets of WST_IGNORED and WST_NOWARN sensors,
     * by looping over _samps, and checking the type field.
     * Samples for IGNORED sensors are not generated.
     * If a NOWARN sample is encountered it is generated, but
     * no warning is logged.
     */
    void checkLessUsedSensors(void);

    /**
     * Private method to add tags of processed samples to this WisardMote.
     * This is only called on the processing WisardMote.
     */
    void addMoteSampleTag(SampleTag* tag);

    /**
     * create a SampleTag from contents of a SampInfo object
     */
    SampleTag* createSampleTag(SampInfo& sinfo,int mote, int stype);

    /**
     * Check for correct EOM. Return pointer to the beginning of the eom,
     * (which is one past the CRC) or NULL if a correct EOM is not found.
     */
    const char *checkEOM(const char *cp,
            const char *eom, nidas::core::dsm_time_t ttag);

    /**
     * Verify crc. Return pointer to the CRC (which is one past the end of the data),
     * or NULL if a correct CRC is not found.
     */
    const char *checkCRC(const char *cp,
            const char *eom, nidas::core::dsm_time_t ttag);

    /**
     * Read initial portion of a Wisard message, filling in struct MessageHeader.
     * @ return true: header OK, false: header not OK.
     */
    bool readHead(const char *&cp,const char *eom, dsm_time_t ttag,
            struct MessageHeader*);

    /**
     * read mote id from a Wisard message.
     * @return: -1 invalid.  >0 valid mote id.
     */
    int readMoteId(const char* &cp, const char*eos);

    /**
     * After unpacking data from the Wisard block, pass it through
     * the usual Variable conversions and optional limit checks.
     */
    void convert(SampleTag* stag, SampleT<float>* osamp, float* results=0);

    const char* unpackPicTime(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackUint16(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackInt16(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackUint32(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackInt32(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackAccumSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpack100thSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpack10thSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackPicTimeFields(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackTRH(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackTsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackGsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackQsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackTP01(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackStatus(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackXbee(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackPower(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackRnet(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackRsw(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackRlw(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackRlwKZ(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackCNR2(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackNR01(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    const char* unpackRsw2(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            SampleTag*, SampleT<float>*);

    /**
     * Because the Wisard mote data has internal identifiers, more
     * than one WisardMote can be declared with the same sensor id.
     * This is done, for example, if there is more than one base
     * radio attached to a DSM. Both sensors would assign the same
     * raw sensor id to their data, which is 0x8000, by convention.
     * For the post-processing, it simplifies things to have just one
     * of the WisardMote instances do the processing for each 
     * raw sensor id.  So this map keeps track of the WisardMotes
     * that do the processing for each sensor id.
     */
    static std::map<dsm_sample_id_t, WisardMote*> _processorSensors;

    static SampInfo _samps[];

    static bool _functionsMapped;

    /**
     * Mapping between sensor type and function which parses the data.
     */
    static std::map<int, std::pair<unpack_t,unsigned int> > _unpackMap;

    static std::map<int, std::string> _typeNames;

    static void initFuncMap();

    /**
     * The processed sample tags for each id. This will
     * have non-zero size only for the processing WisardMote.
     */
    std::map<dsm_sample_id_t, SampleTag*> _sampleTagsById;

    /**
     * Pointer to the WisardMote that does the processing for
     * the samples with this sensor id. Since more than one WisardMote
     * instance may have the same sensor ids, this is the one
     * chosen to process the samples.
     */
    WisardMote* _processorSensor;

    /**
     * Sensor serial numbers, from message.
     */
    std::map<int, std::map<int, int > > _sensorSerialNumbersByMoteIdAndType;

    std::map<int, int> _sequenceNumbersByMoteId;

    std::map<int, unsigned int> _badCRCsByMoteId;

    std::map<int, int> _tdiffByMoteId;

    /**
     * For each mote id, the number of unrecognized sensor types.
     */
    std::map <int, std::map<int, unsigned int> > _numBadSensorTypes;

    std::map<int, unsigned int> _unconfiguredMotes;

    std::map <int, std::map<int, unsigned int> > _noSampleTags;

    std::set<int> _ignoredSensorTypes;

    std::set<int> _nowarnSensorTypes;

    static const unsigned int NTSOILS = 4;

    /**
     */
    class TsoilData
    {
    public:
        float tempLast[NTSOILS];
        dsm_time_t timeLast[NTSOILS];
        TsoilData(): tempLast(),timeLast()
        {
            for (unsigned int i = 0; i < sizeof(tempLast)/sizeof(tempLast[0]); i++) {
                tempLast[i] = floatNAN;
            }
        }
    };
    std::map<dsm_sample_id_t, TsoilData> _tsoilData;

    /*
     * AutoConfig attributes
     */
    static const int DEFAULT_BAUD_RATE = 38400;
    static const Termios::parity DEFAULT_PARITY = Termios::NONE;
    static const int DEFAULT_STOP_BITS = 1;
    static const int DEFAULT_DATA_BITS = 8;
    static const int DEFAULT_RTS485 = 0;
    static const PORT_TYPES DEFAULT_PORT_TYPE = RS232;
//    static const SENSOR_POWER_STATE DEFAULT_SENSOR_POWER = SENSOR_POWER_ON;
    static const TERM DEFAULT_SENSOR_TERMINATION = NO_TERM;
    static const bool DEFAULT_CONFIG_APPLIED = false;
    static const PortConfig DEFAULT_PORT_CONFIG;

    static const int NUM_SENSOR_BAUDS = 1;
    static const int SENSOR_BAUDS[NUM_SENSOR_BAUDS];
    static const int NUM_SENSOR_WORD_SPECS = 1;
    static const WordSpec SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS];
    static const int NUM_PORT_TYPES = 1;
    static const PORT_TYPES SENSOR_PORT_TYPES[NUM_PORT_TYPES];

    // default message parameters for the PB210
    static const int DEFAULT_MESSAGE_LENGTH = 0;
    static const bool DEFAULT_MSG_SEP_EOM = true;
    static const char* DEFAULT_MSG_SEP_CHARS;

    // Sample rate range/default
    static const int DATA_RATE_MIN = 0;
    static const int DATA_RATE_DEFAULT = 5;
    static const int DATA_RATE_MAX = INT16_MAX;

    static const int PWR_SAMP_RATE_MIN = 0;
    static const int PWR_SAMP_RATE_DEFAULT = 5;
    static const int PWR_SAMP_RATE_MAX = INT16_MAX;

    static const int SN_REPORT_RATE_MIN = 0;
    static const int SN_REPORT_RATE_DEFAULT = 360;
    static const int SN_REPORT_RATE_MAX = INT16_MAX;

    // Operating mode range/default
    static const int NODE_ID_MIN = 0;
    static const int NODE_ID_MAX = INT16_MAX;

    static const int SAMP_MODE_MIN = 0;
    static const int SAMP_MODE_DEFAULT = 0;
    static const int SAMP_MODE_MAX = 1;

    static const int MSG_FMT_MIN = 0;
    static const int MSG_FMT_DEFAULT = 0;
    static const int MSG_FMT_MAX = 2;

    static const int OUT_PORT_MIN = 0;
    static const int OUT_PORT_DEFAULT = 0;
    static const int OUT_PORT_MAX = 1;


    static const int MSG_STORE_FLUSHRATE_MIN = 0;
    static const int MSG_STORE_FLUSHRATE_DEFAULT = MSG_STORE_FLUSHRATE_MIN;
    static const int MSG_STORE_FLUSHRATE_MAX = INT16_MAX;

    // NOTE: The v2.7 Mote command reference shows these values in mVs.
    // 
    static const int VMON_ENABLE_DEFAULT=0;
    static const int VMON_LOW_MIN = 0;
    static const int VMON_LOW_DEFAULT = 1190;
    static const int VMON_LOW_MAX = INT16_MAX;

    static const int VMON_HIGH_MIN = 0;
    static const int VMON_HIGH_DEFAULT = 1230;
    static const int VMON_HIGH_MAX = INT16_MAX;

    static const int VMON_SLEEP_TIME_MIN = 0;
    static const int VMON_SLEEP_TIME_DEFAULT = 30;
    static const int VMON_SLEEP_TIME_MAX = INT16_MAX;

    static const float VBATT_GAIN_CAL_MIN;
    static const float VBATT_GAIN_CAL_MAX;
    static const float I3_GAIN_CAL_MIN;
    static const float I3_GAIN_CAL_MAX;
    static const float IIN_GAIN_CAL_MIN;
    static const float IIN_GAIN_CAL_MAX;

    static const int GPS_SYNC_RATE_MIN = 0;
    static const int GPS_SYNC_RATE_DEFAULT = 43200;
    static const int GPS_SYNC_RATE_MAX = INT32_MAX;

    static const int GPS_REQ_LOCKS_MIN = 0;
    static const int GPS_REQ_LOCKS_DEFAULT = 2;
    static const int GPS_REQ_LOCKS_MAX = 6;

    static const int GPS_LCKTMOUT_MIN = 0;
    static const int GPS_LCKTMOUT_DEFAULT = 360;
    static const int GPS_LCKTMOUT_MAX = 600;

    static const int GPS_MSGS_LOCKED = 0;
    static const int GPS_MSGS_DEFAULT = GPS_MSGS_LOCKED;
    static const int GPS_MSGS_ALL = 1;

    static const int GPS_LCKFAIL_RETRY_MIN = 0;
    static const int GPS_LCKFAIL_RETRY_DEFAULT = 1800;
    static const int GPS_LCKFAIL_RETRY_MAX = INT16_MAX;

    MessageConfig defaultMessageConfig;

    typedef std::vector<SensorCmdData> ScienceParamVector;
    ScienceParamVector _scienceParameters;
    ScienceParamVector _epilogScienceParameters;

    bool _scienceParametersOk;

    typedef std::map<MOTE_CMDS, std::string> CmdMap;
    CmdMap _commandTable;
    typedef std::map<MOTE_CMDS, std::string*> CfgMap;
    CfgMap _cfgParameters;

    MoteSensorConfigMetaData _configMetaData;

    /** No copying. */
    WisardMote(const WisardMote&);

    /** No assignment. */
    WisardMote& operator=(const WisardMote&);

};

}}}       // nidas::dynld::isff

#endif    /* WISARDMOTE_H_ */
