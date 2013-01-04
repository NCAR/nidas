/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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
    struct VarInfo variables[9];
    enum WISARD_SAMPLE_TYPE type;
};

class WisardMote:public DSMSerialSensor
{
public:
    WisardMote();

    virtual ~ WisardMote();

    bool process(const Sample * insamp,
            list < const Sample * >&results) throw();

    void validate() throw (nidas::util::InvalidParameterException);

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
                    const SampleTag* stag, SampleT< float >* osamp);

    static const nidas::util::EndianConverter * fromLittle;

private:

    /**
     * Add a SampleTag from the configuration. This SampleTag may have
     * "stypes" and "motes" Parameters, indicating that
     * actual SampleTags should be created for each mote and sample type.
     * @param motes: vector of mote numbers from the "motes" sensor parameter.
     */
    void addSampleTags(SampleTag* stag,const std::vector<int>& motes)
        throw (nidas::util::InvalidParameterException);

    /**
     * Add processed sample tags for all sensor types indicated
     * as WST_IMPLIED in _samps.
     */
    void addImpliedSampleTags(const std::vector<int>& motes);

    /**
     * Samples received from a mote id that is not expected will
     * be assigned an id with a mote field of 0. This method
     * adds sample tags for all sensor types in the big
     * _samps array, with a mote id of 0.
     */
    void addMote0SampleTags();

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
     * the usual Variable conversions and limit checks.
     */
    void convert(const SampleTag* stag, SampleT<float>* osamp);

    const char* unpackPicTime(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackUint16(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackInt16(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackUint32(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackInt32(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackAccumSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpack100thSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpack10thSec(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackPicTimeFields(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackTRH(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackTsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackGsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackQsoil(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackTP01(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackStatus(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackXbee(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackPower(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackRnet(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackRsw(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackRlw(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackRlwKZ(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackCNR2(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

    const char* unpackRsw2(const char *, const char *,
            unsigned int, const struct MessageHeader*,
            const SampleTag*, SampleT<float>*);

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
    static std::map<int, pair<unpack_t,unsigned int> > _unpackMap;

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

    std::set<int> _ignoredSensorTypes;

    /**
     */
    class TsoilData
    {
    public:
        float tempLast[4];
        dsm_time_t timeLast;
        // TsoilData(): tempLast[0](floatNAN),tempLast[1](floatNAN),tempLast[2](floatNAN),tempLast[3](floatNAN),timeLast(0) {}
        // TsoilData(): tempLast({floatNAN,floatNAN,floatNAN,floatNAN}),timeLast(0) {}
        TsoilData(): tempLast(),timeLast(0)
        {
            for (unsigned int i = 0; i < sizeof(tempLast)/sizeof(tempLast[0]); i++) tempLast[i] = floatNAN;
        }
    };
    std::map<dsm_sample_id_t, TsoilData> _tsoilData;

    /** No copying. */
    WisardMote(const WisardMote&);

    /** No assignment. */
    WisardMote& operator=(const WisardMote&);

};
}}}                             // nidas::dynld::isff
#endif                          /* WISARDMOTE_H_ */
