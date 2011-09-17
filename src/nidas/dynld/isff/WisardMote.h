/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision:  $

    $LastChangedBy: dongl $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/WisardMote.h $

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

#include <nidas/core/DSMTime.h>
#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/util/InvalidParameterException.h>

#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <exception>

//#include <sstream>
#include <list>

namespace nidas { namespace dynld { namespace isff {

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
 * WST_IGNORED: if a sample of this sensor type is received, don't process it.
 * WST_NORMAL: otherwise.
 */
enum WISARD_SAMPLE_TYPE { WST_NORMAL, WST_IMPLIED, WST_IGNORED };

struct SampInfo
{
    /**
     * firstst and lastst are the range of sensor types
     * containing the list of variables.
     */
    int firstst;
    int lastst;
    struct VarInfo variables[6];
    enum WISARD_SAMPLE_TYPE type;
};

class WisardMote:public DSMSerialSensor
{
public:
    static const unsigned char _missValueUint8 = 0x80;
    static const short _missValueInt16 = (signed) 0x8000;
    static const unsigned short _missValueUint16 = (unsigned) 0x8000;
    static const unsigned int _missValueUint32 = 0x80000000;

    WisardMote();

    virtual ~ WisardMote();

    bool process(const Sample * insamp,
            list < const Sample * >&results) throw();

    void validate() throw (nidas::util::InvalidParameterException);

    /**
     * typedef for the functions that parse the message data
     * for each sensor type.
     */
    typedef const unsigned char *(WisardMote::
            *readFunc) (const unsigned
                    char *cp,
                    const unsigned
                    char *eos,
                    nidas::core::dsm_time_t
                    ttag_msec,std::vector < float >& data);

private:
    static const nidas::util::EndianConverter * _fromLittle;

    /*
     * Add a SampleTag from the configuration. This SampleTag may have
     * "stypes" and "motes" Parameters, indicating that
     * actual SampleTags should be created for each mote and sample type.
     * @param motes: vector of mote numbers from the "motes" sensor parameter.
     */
    void addSampleTags(SampleTag* stag,const std::vector<int>& motes)
        throw (nidas::util::InvalidParameterException);

    /**
     * Samples received from a mote id that is not expected will
     * be assigned an id with a mote field of 0. This method
     * adds sample tags for all sensor types in the big
     * _samps array, with a mote id of 0.
     */
    void addMote0SampleTags();

    /**
     * create a SampleTag from contents of a SampInfo object
     */
    SampleTag* createSampleTag(SampInfo& sinfo,int mote, int stype);

    /**
     * Mote id, read from initial digits in message, up to colon.
     * For example the number XX from "IDXX:"
     */
    int _moteId;

    /**
     * Sensor serial numbers, from message.
     */
    std::map < int, std::map < unsigned char,int > > _sensorSerialNumbersByMoteIdAndType;

    /**
     * Version number of current message.
     */
    int _version;

    std::map < int, int > _sequenceNumbersByMoteId;

    std::map < int, int > _badCRCsByMoteId;

    std::map < int, int > _tdiffByMoteId;

    /**
     * For each mote id, the number of unrecognized sensor types.
     */
    std::map < int, std::map< unsigned char, unsigned int> > _numBadSensorTypes;

    /**
     * Check for correct EOM. Return pointer to the beginning of the eom,
     * (which is one past the CRC) or NULL if a correct EOM is not found.
     */
    const unsigned char *checkEOM(const unsigned char *cp,
            const unsigned char *eom);

    /**
     * Verify crc. Return pointer to the CRC (which is one past the end of the data),
     * or NULL if a correct CRC is not found.
     */
    const unsigned char *checkCRC(const unsigned char *cp,
            const unsigned char *eom, nidas::core::dsm_time_t ttag);

    /**
     * Read mote id, find ID#, :, seq#, and msgType. Return msgType.
     */
    int readHead(const unsigned char *&cp,
            const unsigned char *eom);

    /**
     * Function to unpack unsigned 16 bit values, scale and store in a vector of floats
     */
    const unsigned char *readUint8(const unsigned char *cp,
            const unsigned char *eos, int nval,float scale, vector<float>& data);

    /**
     * Function to unpack unsigned 16 bit values, scale and store in a vector of floats
     */
    const unsigned char *readUint16(const unsigned char *cp,
            const unsigned char *eos, int nval,float scale, vector<float>& data);

    /**
     * Function to unpack signed 16 bit values, scale and store in a vector of floats
     */
    const unsigned char *readInt16(const unsigned char *cp,
            const unsigned char *eos, int nval,float scale, vector<float>& data);

    /**
     * Function to unpack unsigned 32 bit values, scale and store in a vector of floats
     */
    const unsigned char *readUint32(const unsigned char *cp,
            const unsigned char *eos, int nval,float scale, vector<float>& data);

    /* methods to retrieve sensorType data    */
    const unsigned char *readPicTm(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readGenShort(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readGenLong(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readTmCnt(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readTmSec(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readTm100thSec(const unsigned char
            *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readTm10thSec(const unsigned char *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readPicDT(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readTsoilData(const unsigned char *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readGsoilData(const unsigned char *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readQsoilData(const unsigned char *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readTP01Data(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readG5ChData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readG4ChData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readG1ChData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readRnetData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readRswData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readRswData2(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag, std::vector<float>& data);

    const unsigned char *readRlwData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readRlwKZData(const unsigned char *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readCNR2Data(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    const unsigned char *readStatusData(const unsigned char
            *cp,
            const unsigned char
            *eos, nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readPwrData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);
    const unsigned char *readXbeeData(const unsigned char *cp,
            const unsigned char *eos,
            nidas::core::dsm_time_t ttag,std::vector<float>& data);

    static SampInfo _samps[];

    static bool _functionsMapped;

    /**
     * Mapping between sensor type and function which decodes the data.
     */
    static std::map < unsigned char,WisardMote::readFunc > _nnMap;

    static std::map < unsigned char,std::string> _typeNames;

    static void initFuncMap();

    /**
     * These are the sample tags for the output samples, containing
     * all the information that the user specified for each mote.
     */
    static std::map<dsm_sample_id_t, std::map<dsm_sample_id_t,SampleTag*> > _sampleTagsById;
    std::map<dsm_sample_id_t,SampleTag*> _mySampleTagsById;

    bool _isProcessSensor;

    std::map<int,unsigned int> _unconfiguredMotes;

    std::set<int> _ignoredSensorTypes;

};
}}}                             // nidas::dynld::isff
#endif                          /* WISARDMOTE_H_ */
