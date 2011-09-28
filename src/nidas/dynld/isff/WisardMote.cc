// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2009 UCAR, NCAR, All Rights Reserved

   $LastChangedDate:  $

   $LastChangedRevision:  $

   $LastChangedBy: dongl $

   $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/WisardMote.h $

 */

#include "WisardMote.h"
#include <nidas/util/Logger.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/DSMTime.h>
#include <nidas/util/UTime.h>
#include <nidas/util/InvalidParameterException.h>

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <memory>               // auto_ptr<>
using namespace nidas::dynld;
using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#define MSECS_PER_HALF_DAY 43200000

/* static */
bool WisardMote::_functionsMapped = false;

/* static */
std::map<unsigned char, WisardMote::readFunc> WisardMote::_nnMap;

/* static */
std::map<unsigned char, string> WisardMote::_typeNames;

/* static */
const n_u::EndianConverter * WisardMote::_fromLittle =
    n_u::EndianConverter::getConverter(
            n_u::EndianConverter::EC_LITTLE_ENDIAN);

/* static */
map<dsm_sample_id_t,WisardMote*> WisardMote::_processorSensors;

NIDAS_CREATOR_FUNCTION_NS(isff, WisardMote)

WisardMote::WisardMote() :
    _processorSensor(0),_moteId(-1), _version(-1)
{
    setDuplicateIdOK(true);
    initFuncMap();
}
WisardMote::~WisardMote()
{
}

void WisardMote::validate()
    throw (n_u::InvalidParameterException)
{
    // Since WisardMote data has internal identifiers (mote ids and
    // sensor types), multiple WisardMote sensors can be instantiated on
    // a DSM with the same sensor id (typically 0x8000), but on different
    // serial ports. When the internal indentifiers are parsed from
    // the raw sample, the processed sample ids are generated from the
    // mote ids and sensor type values in the raw sample.

    // When samples are processed, the process methods of all DSMSensor
    // objects matching a given raw sample id are called. For WisardMotes
    // we want to do the processing in only one sensor instance for the
    // sensor id on each DSM, primarily so that multiple sensor objects
    // are not complaining about unrecognized mote ids or sample types.
    // So WisardMotes share a static map of sampleTagsById so that one
    // WisardMote object can do the processing for a DSM.
    //

    _processorSensor = _processorSensors[getId()];

    if (!_processorSensor) _processorSensors[getId()] = _processorSensor = this;
    else if (_processorSensor == this) return;  // already validated

    vector<int> motev;

    const Parameter* motes = getParameter("motes");
    if (motes) {
        /*
         * If a "motes" parameter exists for the sensor, then for
         * all sample tags with a zero mote field (bits 15-8), generate
         * tags for the list of motes in the parameter.
         */
        if (motes->getType() != Parameter::INT_PARAM)
            throw n_u::InvalidParameterException(getName(),"motes","should be integer type");
        for (int i = 0; i < motes->getLength(); i++) {
            int mote = (unsigned int) motes->getNumericValue(i);
            motev.push_back(mote);
        }
    }

    list<SampleTag*> configTags = _sampleTags;
    _sampleTags.clear();
    list<SampleTag*>::iterator ti = configTags.begin();

    // loop over the configured sample tags, creating the ones we want
    // using the "motes" and "stypes" parameters. Delete the original
    // configured tags.
    for ( ; ti != configTags.end(); ) {
        SampleTag* stag = *ti;
        removeSampleTag(stag);
        addSampleTags(stag,motev);
        ti = configTags.erase(ti);
        delete stag;
    }

    _processorSensor->addImpliedSampleTags(motev);

    // for all possible sensor types, create sample ids for mote 0, the
    // unconfigured, unexpected mote.
    if (_processorSensor == this) addMote0SampleTags();

#ifdef DEBUG
    cerr << "final getSampleTags().size()=" << getSampleTags().size() << endl;
    cerr << "final _sampleTags.size()=" << _sampleTags.size() << endl;
    cerr << "final _sampleTagsByIdTags.size()=" << _sampleTagsById.size() << endl;
#endif
    assert(_sampleTags.size() == getSampleTags().size());

    DSMSerialSensor::validate();
}

void WisardMote::addSampleTags(SampleTag* stag,const vector<int>& sensorMotes)
    throw (n_u::InvalidParameterException)
{

    // The stag must contain a Parameter, called "stypes", specifing one or
    // more sensor types that the stag is applied to.

    // stag can also contain a Parameter, "motes" specifying the motes that the
    // sample is for, overriding sensorMotes.  Otherwise the motes must be
    // passed in the sensorMotes vector which came from the "motes" parameter of
    // the sensor.

    // Sample tags are created for every mote and sensor type.
    // The resultant sample ids for processed samples will be the sum of
    //      sensor_id + (mote id << 8) + stype
    // where sensor_id is typically 0x8000

    string idstr;
    {
        ostringstream ost;
        ost << stag->getDSMId() << ",0x" << hex << stag->getSensorId() << "+" <<
            dec << stag->getSampleId();
        idstr = ost.str();
    }

    vector<int> motes;

    const Parameter* motep = stag->getParameter("motes");
    if (motep) {
        if (motep->getType() != Parameter::INT_PARAM)
            throw n_u::InvalidParameterException(getName() + ": id=" + idstr,
                    "parameter \"motes\"","should be integer type");
        for (int i = 0; i < motep->getLength(); i++)
            motes.push_back((int) motep->getNumericValue(i));
    }
    else motes = sensorMotes;

    if (motes.empty())
        throw n_u::InvalidParameterException(getName(),string("id=") + idstr,"no motes specified");

#ifdef DEBUG_DSM
    if (stag->getDSMId() == DEBUG_DSM) cerr << "mote=" << mote << endl;
#endif

    // This sample applies to all sensor type ids in the "stypes" parameter.
    // inid is a full sample id (dsm,sensor,mote,sensor type), except
    // that mote may be 0 indicating it applies to all motes.
    // If there is no "stypes" parameter
    const Parameter* stypep = stag->getParameter("stypes");
    if (!stypep)
            throw n_u::InvalidParameterException(getName(), string("id=") + idstr,
                    "no \"stypes\" parameter");
    if (stypep->getType() != Parameter::INT_PARAM || stypep->getLength() < 1)
        throw n_u::InvalidParameterException(getName()+": id=" + idstr,
                "stypes","should be hex or integer type, of length > 0");

    vector<int> stypes;
    for (int i = 0; i < stypep->getLength(); i++) {
        stypes.push_back((int) stypep->getNumericValue(i));
    }

    for (unsigned im = 0; im < motes.size(); im++) {
        int mote = motes[im];

        ostringstream moteost;
        moteost << mote;
        string motestr = moteost.str();

        for (unsigned int is = 0; is < stypes.size(); is++) {
            int stype = stypes[is];

            SampleTag *newtag = new SampleTag(*stag);
            newtag->setDSMId(stag->getDSMId());
            newtag->setSensorId(stag->getSensorId());
            newtag->setSampleId((mote << 8) + stype);
            for (unsigned int iv = 0; iv < newtag->getVariables().size(); iv++) {
                Variable& var = newtag->getVariable(iv);
                var.setPrefix(n_u::replaceChars(var.getPrefix(),"%m",motestr));
                var.setPrefix(n_u::replaceChars(var.getPrefix(),"%c",string(1,(char)('a' + is))));
                var.setSuffix(n_u::replaceChars(var.getSuffix(),"%m",motestr));
                var.setSuffix(n_u::replaceChars(var.getSuffix(),"%c",string(1,(char)('a' + is))));
            }
            _processorSensor->addMoteSampleTag(newtag);
        }
    }
    return;
}

void WisardMote::addMoteSampleTag(SampleTag* tag)
{
#ifdef DEBUG
    cerr << "addSampleTag, id=" << tag->getDSMId() << ',' << hex << tag->getSpSId() << dec <<
        ", ntags=" << getSampleTags().size() << endl;
#endif
    
    if (_sampleTagsById[tag->getId()]) {
        WLOG(("%s: duplicate processed sample tag for id %d,%#x",
                    getName().c_str(), tag->getDSMId(),tag->getSpSId()));
        delete tag;
    }
    else {
        _sampleTagsById[tag->getId()] = tag;
        addSampleTag(tag);
    }

}

void WisardMote::addImpliedSampleTags(const vector<int>& sensorMotes)
{
    // add samples for WST_IMPLIED types. These don't have
    // to be configured in the XML for a mote, but
    // the sensor should have a "motes" parameter.
    for (unsigned int im = 0; im < sensorMotes.size(); im++) {
        int mote = sensorMotes[im];
        for (unsigned int itype = 0;; itype++) {
            int stype1 = _samps[itype].firstst;
            if (stype1 == 0) break;
            if (_samps[itype].type == WST_IMPLIED) {
                int stype2 = _samps[itype].lastst;
                for (int stype = stype1; stype <= stype2; stype++) {
                    SampleTag* newtag = createSampleTag(_samps[itype],mote,stype);
                    _processorSensor->addMoteSampleTag(newtag);
                }
            }
        }
    }
}

void WisardMote::addMote0SampleTags()
{
    // add sample tags for all possible sensor types, setting
    // the mote value to 0, the "match-all" mote
    for (unsigned int itype = 0;; itype++) {
        int stype1 = _samps[itype].firstst;
        if (stype1 == 0) break;
        if (_samps[itype].type != WST_IGNORED) {
            int stype2 = _samps[itype].lastst;
            for (int stype = stype1; stype <= stype2; stype++) {
                int mote = 0;
                SampleTag* newtag = createSampleTag(_samps[itype],mote,stype);
                _processorSensor->addMoteSampleTag(newtag);
            }
        }
    }
    // create a list of sensor types to be ignored.
    for (unsigned int itype = 0;; itype++) {
        int stype1 = _samps[itype].firstst;
        if (stype1 == 0) break;
        if (_samps[itype].type == WST_IGNORED) {
            int stype2 = _samps[itype].lastst;
            for (int stype = stype1; stype <= stype2; stype++) _ignoredSensorTypes.insert(stype);
        }
    }
}

// create a SampleTag from contents of a SampInfo object
SampleTag* WisardMote::createSampleTag(SampInfo& sinfo,int mote, int stype)
{

    ostringstream moteost;
    moteost << mote;
    string motestr = moteost.str();

    mote <<= 8;

    SampleTag* newtag = new SampleTag();
    newtag->setDSMId(getDSMId());
    newtag->setSensorId(getId());
    newtag->setSampleId(mote + stype);

    int nv = sizeof(sinfo.variables) / sizeof(sinfo.variables[0]);

    for (int iv = 0; iv < nv; iv++) {
        VarInfo vinf = sinfo.variables[iv];
        if (vinf.name == NULL)
            break;

        Variable *var = new Variable();
        var->setName(n_u::replaceChars(vinf.name,"%m",motestr));
        var->setName(n_u::replaceChars(var->getName(),"%c",string(1,(char)('a' + stype - sinfo.firstst))));

        var->setUnits(vinf.units);
        var->setLongName(vinf.longname);
        var->setDynamic(true);
        var->setLength(1);

        string aval = Project::getInstance()->expandString(vinf.plotrange);
        std::istringstream ist(aval);
        float prange[2] = { -10.0, 10.0 };
        // if plotrange value starts with '$' ignore error.
        if (aval.length() < 1 || aval[0] != '$') {
            int k;
            for (k = 0; k < 2; k++) {
                if (ist.eof())
                    break;
                ist >> prange[k];
                if (ist.fail())
                    break;
            }
            // Don't throw exception on poorly formatted plotranges
            if (k < 2) {
                n_u::InvalidParameterException e(string("variable ")
                        + vinf.name, "plot range", aval);
                WLOG(("%s", e.what()));
            }
        }
        var->setPlotRange(prange[0], prange[1]);

        newtag->addVariable(var);
    }
    return newtag;
}

bool WisardMote::process(const Sample * samp, list<const Sample *>&results)
throw ()
{
    if (_processorSensor != this) return false;

    /* unpack a WisardMote packet, consisting of binary integer data from a variety
     * of sensor types. */
    const unsigned char *sos =
        (const unsigned char *) samp->getConstVoidDataPtr();
    const unsigned char *eos = sos + samp->getDataByteLength();
    const unsigned char *cp = sos;

    dsm_time_t ttag = samp->getTimeTag();

    /*  check for good EOM  */
    if (!(eos = checkEOM(cp, eos,ttag)))
        return false;

    /*  verify crc for data  */
    if (!(eos = checkCRC(cp, eos, ttag)))
        return false;

    /*  read header */
    int mtype = readHead(cp, eos, ttag);
    if (_moteId < 0)
        return false; // invalid
    if (mtype == -1)
        return false; // invalid

    if (mtype != 1)
        return false; // other than a data message


    while (cp < eos) {

        /* get Wisard sensor type */
        unsigned char sensorType = *cp++;

#ifdef DEBUG
        DLOG(("%s: %s, moteId=%d, sensorid=%x, sensorType=%#x",
                getName().c_str(),
                n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                _moteId, getSensorId(),sensorType));
#endif

        /* find the appropriate member function to unpack the data for this sensorType */
        readFunc func = _nnMap[sensorType];

        if (func == NULL) {
            if (!( _numBadSensorTypes[_moteId][sensorType]++ % 100))
                WLOG(("%s: %s, moteId=%d: unknown sensorType=%#x, at byte %u, #times=%u",
                        getName().c_str(),
                        n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                        _moteId, sensorType,
                        (unsigned int)(cp-sos-1),_numBadSensorTypes[_moteId][sensorType]));
            continue;
        }

        /* unpack the data for this sensorType */
        vector<float> data;
        cp = (this->*func)(cp, eos, ttag,data);

        /* create an output floating point sample */
        if (data.size() == 0)
            continue;

        // sample id of processed sample
        dsm_sample_id_t sid = getId() + (_moteId << 8) + sensorType;
        SampleTag* stag = _sampleTagsById[sid];

        if (!stag) {
            if (_ignoredSensorTypes.find(sensorType) != _ignoredSensorTypes.end()) continue;
            if (!(_unconfiguredMotes[_moteId]++ % 100))
                WLOG(("%s: %s, unconfigured mote id %d, sensorType=%#x, #times=%u, sid=%d,%#x",
                    getName().c_str(),
                    n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                    _moteId,sensorType,_unconfiguredMotes[_moteId],
                    GET_DSM_ID(sid),GET_SPS_ID(sid)));
            // no match for this mote, try mote id 0
            stag = _sampleTagsById[getId() + sensorType];
        }

        SampleT<float>* osamp;

        if (stag) {
            const vector<const Variable*>& vars = stag->getVariables();
            unsigned int slen = vars.size();
            osamp = getSample<float> (slen);
            osamp->setId(stag->getId());
            float *fp = osamp->getDataPtr();
            std::copy(data.begin(),
                    data.begin()+std::min(data.size(),vars.size()),fp);
            unsigned int nv;
            for (nv = 0; nv < slen; nv++,fp++) {
                // DLOG(("f[%d]= %f", nv, *fp));
                const Variable* var = vars[nv];
                if (nv >= data.size() || *fp == var->getMissingValue()) *fp = floatNAN;
                else if (*fp < var->getMinValue() || *fp > var->getMaxValue())
                    *fp = floatNAN;
                else if (getApplyVariableConversions()) {
                    VariableConverter* conv = var->getConverter();
                    if (conv) *fp = conv->convert(ttag,*fp);
                }
            }
        }
        else {
            WLOG(("%s: %s, no sample tag for %d,%#x",
                    getName().c_str(),
                    n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                    GET_DSM_ID(sid),GET_SPS_ID(sid)));
            osamp = getSample<float> (data.size());
            osamp->setId(sid);
            std::copy(data.begin(), data.end(),osamp->getDataPtr());
#ifdef DEBUG
            for (unsigned int i = 0; i < data.size(); i++) {
                DLOG(("data[%d]=%f", i, data[i]));
            }
#endif
        }
        osamp->setTimeTag(ttag);

        /* push out */
        results.push_back(osamp);
    }
    return true;
}

/**
 * read mote id, version.
 * return msgType: -1=invalid header, 0 = sensortype+SN, 1=seq+time+data,  2=err msg
 */
int WisardMote::readHead(const unsigned char *&cp, const unsigned char *eos,
        dsm_time_t ttag) {
    _moteId = -1;

    /* look for mote id. First skip non-digits. */
    for (; cp < eos; cp++)
        if (::isdigit(*cp))
            break;
    if (cp == eos)
        return -1;

    const unsigned char *colon = (const unsigned char *) ::memchr(cp, ':', eos
            - cp);
    if (!colon)
        return -1;

    // read the moteId
    string idstr((const char *) cp, colon - cp);
    {
        stringstream ssid(idstr);
        ssid >> std::dec >> _moteId;
        if (ssid.fail())
            return -1;
    }

    // DLOG(("idstr=%s moteId=$i", idstr.c_str(), _moteId));

    cp = colon + 1;

    // version number
    if (cp == eos)
        return -1;
    _version = *cp++;

    // message type
    if (cp == eos)
        return -1;
    int mtype = *cp++;

    switch (mtype) {
    case 0:
        /* unpack 1 bytesId + 2 byte s/n */
        while (cp + 3 <= eos) {
            int sensorType = *cp++;
            int serialNumber = _fromLittle->uint16Value(cp);
            cp += sizeof(short);
            // log serial number if it changes.
            if (_sensorSerialNumbersByMoteIdAndType[_moteId][sensorType]
                                                             != serialNumber) {
                _sensorSerialNumbersByMoteIdAndType[_moteId][sensorType]
                                                             = serialNumber;
                ILOG(("%s: %s, mote=%s, sensorType=%#x SN=%d, typeName=%s",
                        getName().c_str(),
                        n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S").c_str(),
                        idstr.c_str(), sensorType,
                        serialNumber,_typeNames[sensorType].c_str()));
            }
        }
        break;
    case 1:
        /* unpack 1byte sequence */
        if (cp == eos)
            return false;
        _sequenceNumbersByMoteId[_moteId] = *cp++;
#ifdef DEBUG
        DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d seq=%d",
                idstr.c_str(), _moteId, _version, mtype,
                _sequenceNumbersByMoteId[_moteId]));
#endif
        break;
    case 2:
#ifdef DEBUG
        DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d ErrMsg=\"",
                idstr.c_str(), _moteId, _version,
                mtype) << string((const char *) cp, eos - cp) << "\"");
#endif
        break;
    default:
        WLOG(("%s: %s, unknown msgType, mote=%s, id=%d, Ver=%d MsgType=%d, msglen=",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S").c_str(),
                idstr.c_str(), _moteId, _version, mtype, eos - cp));
        break;
    }
    return mtype;
}

/*
 * Check EOM (0x03 0x04 0xd). Return pointer to start of EOM.
 */
const unsigned char *WisardMote::checkEOM(const unsigned char *sos,
        const unsigned char *eos, dsm_time_t ttag) {

    if (eos - 4 < sos) {
        WLOG(("%s: %s, message length is too short, len= %d",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S").c_str(),
                eos - sos));
        return 0;
    }
    // NIDAS will likely add a NULL to the end of the message. Check for that.
    if (*(eos - 1) == 0)
        eos--;
    eos -= 3;

    if (memcmp(eos, "\x03\x04\r", 3) != 0) {
        WLOG(("%s: %s, bad EOM, last 3 chars= %x %x %x",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S").c_str(),
                    *(eos), *(eos + 1),*(eos + 2)));
        return 0;
    }
    return eos;
}

/*
 * Check CRC. Return pointer to CRC, which is one past the end of the data portion.
 */
const unsigned char *WisardMote::checkCRC(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag) {
    // Initial value of eos points to one past the CRC.
    if (eos-- <= cp) {
        WLOG(("%s: %s, message length is too short, len= %d",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S").c_str(),
                eos - cp));
        return 0;
    }

    // retrieve CRC at end of message.
    unsigned char crc = *eos;

    // Calculate Cksum. Start with length of message, not including checksum.
    unsigned char cksum = eos - cp;
    for (const unsigned char *cp2 = cp; cp2 < eos;)
        cksum ^= *cp2++;

    if (cksum != crc) {
        //skip the non-printable characters
        int bogusCnt = 0;
        while (!isprint(*cp)) {
            cp++;
            bogusCnt++;
        }
        //try once more time
        if (bogusCnt > 0) {
            return checkCRC(cp, eos, ttag);
        }
        // Try to print out some header information.
        int mtype = readHead(cp, eos,ttag);
        if (!(_badCRCsByMoteId[_moteId]++ % 10)) {
            if (_moteId >= 0) {
                WLOG(("%s: %s, %d bad CKSUMs for mote id %d, messsage type=%d, length=%d, tx crc=%x, calc crc=%x",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                            _badCRCsByMoteId[_moteId], _moteId, mtype, (eos - cp),
                            crc,cksum));
            } else {
                WLOG(("%s: %s, %d bad CKSUMs for unknown mote, length=%d, tx crc=%x, calc crc=%x",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S").c_str(),
                            _badCRCsByMoteId[_moteId], (eos - cp), crc, cksum));
            }
        }
        return 0;
    }
    return eos;
}

const unsigned char *WisardMote::readUint8(const unsigned char *cp,
        const unsigned char *eos, int nval,float scale, vector<float>& data) {
    /* convert unsigned chars to float */
    int i;
    for (i = 0; i < nval; i++) {
        if (cp + sizeof(uint8_t) > eos) break;
        unsigned char val = *cp++;
        cp += sizeof(uint8_t);
        if (val != _missValueUint8)
            data.push_back(val * scale);
        else
            data.push_back(floatNAN);
    }
    for ( ; i < nval; i++) data.push_back(floatNAN);
    return cp;
}

const unsigned char *WisardMote::readUint16(const unsigned char *cp,
        const unsigned char *eos, int nval,float scale, vector<float>& data) {
    /* unpack 16 bit unsigned integers */
    int i;
    for (i = 0; i < nval; i++) {
        if (cp + sizeof(uint16_t) > eos) break;
        unsigned short val = _fromLittle->uint16Value(cp);
        cp += sizeof(uint16_t);
        if (val != _missValueUint16)
            data.push_back(val * scale);
        else
            data.push_back(floatNAN);
    }
    for ( ; i < nval; i++) data.push_back(floatNAN);
    return cp;
}

const unsigned char *WisardMote::readInt16(const unsigned char *cp,
        const unsigned char *eos, int nval,float scale, vector<float>& data) {
    /* unpack 16 bit signed integers */
    int i;
    for (i = 0; i < nval; i++) {
        if (cp + sizeof(int16_t) > eos) break;
        signed short val = _fromLittle->int16Value(cp);
        cp += sizeof(int16_t);
        if (val != _missValueInt16)
            data.push_back(val * scale);
        else
            data.push_back(floatNAN);
    }
    for ( ; i < nval; i++) data.push_back(floatNAN);
    return cp;
}

const unsigned char *WisardMote::readUint32(const unsigned char *cp,
        const unsigned char *eos, int nval,float scale, vector<float>& data) {
    /* unpack 32 bit unsigned ints */
    int i;
    for (i = 0; i < nval; i++) {
        if (cp + sizeof(uint32_t) > eos) break;
        unsigned int val = _fromLittle->uint32Value(cp);
        cp += sizeof(uint32_t);
        if (val != _missValueUint32)
            data.push_back(val * scale);
        else
            data.push_back(floatNAN);
    }
    for ( ; i < nval; i++) data.push_back(floatNAN);
    return cp;
}

/* type id 0x01 */
const unsigned char *WisardMote::readPicTm(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    return readUint16(cp,eos,1,0.1,data);
}

/* type id 0x04 */
const unsigned char *WisardMote::readGenShort(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    return readUint16(cp,eos,1,1.0,data);
}

/* type id 0x05 */
const unsigned char *WisardMote::readGenLong(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    return readUint32(cp,eos,1,1.0,data);
}

/* type id 0x0b */
const unsigned char *WisardMote::readSecOfYear(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    /* unpack 32 bit unsigned int, seconds since Jan 01 00:00 UTC */
    if (cp + sizeof(uint32_t) <= eos) {
        unsigned int val = _fromLittle->uint32Value(cp);    // seconds of year
        if (val != _missValueUint32) {
            struct tm tm;
            n_u::UTime ut(ttag);
            ut.toTm(true,&tm);

            // compute time on Jan 1, 00:00 UTC of year from sample time tag
            tm.tm_sec = tm.tm_min = tm.tm_hour = tm.tm_mon = 0;
            tm.tm_mday = 1;
            tm.tm_yday = -1;
            ut = n_u::UTime::fromTm(true,&tm);

#ifdef DEBUG
            cerr << "ttag=" << n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%6f") <<
                ",ut=" << ut.format(true,"%Y %m %d %H:%M:%S.%6f") << endl;
            cerr << "ttag=" << ttag << ", ut=" << ut.toUsecs() << ", val=" << val << endl;
#endif
            // will have a rollover issue on Dec 31 23:59:59, but we'll ignore it
            long long diff = (ttag - (ut.toUsecs() + (long long)val * USECS_PER_SEC));

            // bug in the mote timekeeping: the 0x0b values are 1 day too large
            if (::llabs(diff+USECS_PER_DAY) < 60 * USECS_PER_SEC) diff += USECS_PER_DAY;
            data.push_back((float)diff / USECS_PER_SEC);
        }
        else data.push_back(floatNAN);
        cp += sizeof(uint32_t);
    }
    return cp;
}

/* type id 0x0c */
const unsigned char *WisardMote::readTmCnt(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    return readUint32(cp,eos,1,1.0,data);
}

/* type id 0x0d */
const unsigned char *WisardMote::readTm100thSec(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    return readUint32(cp,eos,1,0.01,data);
}

/* type id 0x0e */
const unsigned char *WisardMote::readTm10thSec(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) //ttag=microSec
{
    /* unpack  32 bit  t-tm-ticks in 10th sec */
    unsigned int val = 0;
    if (cp + sizeof(uint32_t) > eos) return cp;
    val = _fromLittle->uint32Value(cp);
    // convert mote time to 1/10th secs since 00:00 UTC
    val %= (SECS_PER_DAY * 10);
    // convert to milliseconds
    val *= 100;

    cp += sizeof(uint32_t);

    //convert sample time tag to milliseconds since 00:00 UTC
    int mSOfDay = (ttag / USECS_PER_MSEC) % MSECS_PER_DAY;

    int diff = mSOfDay - val; //mSec

    if (abs(diff) > MSECS_PER_HALF_DAY) {
        if (diff < -MSECS_PER_HALF_DAY)
            diff += MSECS_PER_DAY;
        else if (diff > MSECS_PER_HALF_DAY)
            diff -= MSECS_PER_DAY;
    }
    float fval = (float) diff / MSECS_PER_SEC; // seconds

    // keep track of the first time difference.
    if (_tdiffByMoteId[_moteId] == 0)
        _tdiffByMoteId[_moteId] = diff;

    // subtract the first difference from each succeeding difference.
    // This way we can check the mote clock drift relative to the adam
    // when the mote is not initialized with an absolute time.
    diff -= _tdiffByMoteId[_moteId];
    if (abs(diff) > MSECS_PER_HALF_DAY) {
        if (diff < -MSECS_PER_HALF_DAY)
            diff += MSECS_PER_DAY;
        else if (diff > MSECS_PER_HALF_DAY)
            diff -= MSECS_PER_DAY;
    }

    float fval2 = (float) diff / MSECS_PER_SEC;

    data.push_back(fval);
    data.push_back(fval2);
    return cp;
}

/* type id 0x0f */
const unsigned char *WisardMote::readPicDT(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
    /*  16 bit jday */
    if (cp + sizeof(uint16_t) > eos) return cp;
    unsigned short jday = _fromLittle->uint16Value(cp);
    cp += sizeof(uint16_t);
    if (jday != _missValueUint16)
        data.push_back(jday);
    else
        data.push_back(floatNAN);

    /*  8 bit hour+ 8 bit min+ 8 bit sec  */
    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char hh = *cp;
    cp += sizeof(uint8_t);
    if (hh != _missValueUint8)
        data.push_back(hh);
    else
        data.push_back(floatNAN);

    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char mm = *cp;
    cp += sizeof(uint8_t);
    if (mm != _missValueUint8)
        data.push_back(mm);
    else
        data.push_back(floatNAN);

    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char ss = *cp;
    cp += sizeof(uint8_t);
    if (ss != _missValueUint8)
        data.push_back(ss);
    else
        data.push_back(floatNAN);

    return cp;
}

/* type id 0x20-0x23 */
const unsigned char *WisardMote::readTsoilData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,4,0.01,data);
}

/* type id 0x24-0x27 */
const unsigned char *WisardMote::readGsoilData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,1,0.1,data);
}

/* type id 0x28-0x2b */
const unsigned char *WisardMote::readQsoilData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readUint16(cp,eos,1,0.01,data);
}

/* type id 0x2c-0x2f */
const unsigned char *WisardMote::readTP01Data(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    // 5 signed
    int i;
    for (i = 0; i < 5; i++) {
        if (cp + sizeof(int16_t) > eos) break;
        short val = _fromLittle->int16Value(cp);
        cp += sizeof(int16_t);
        if (val != (signed) 0xFFFF8000) {
            switch (i) {
            case 0:
                data.push_back(val / 10000.0);
                break;
            case 1:
                data.push_back(val / 1.0);
                break;
            case 2:
                data.push_back(val / 1.0);
                break;
            case 3:
                data.push_back(val / 100.0);
                break;
            case 4:
                data.push_back(val / 1000.0);
                break;
            }
        } else
            data.push_back(floatNAN);
    }
    for ( ; i < 5; i++) data.push_back(floatNAN);
    return cp;
}

/* type id 0x30 -- ox33  */
const unsigned char *WisardMote::readG5ChData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,5,1.0,data);
}

/* type id 0x34 -- 0x37  */
const unsigned char *WisardMote::readG4ChData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,4,1.0,data);
}

/* type id 0x38 -- ox3b  */
const unsigned char *WisardMote::readG1ChData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,1,1.0,data);
}

/* type id 0x40 Sampling Mode */
const unsigned char *WisardMote::readStatusData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    if (cp + 1 > eos) return cp;
    unsigned char val = *cp++;
    if (val != _missValueUint8)
        data.push_back(val);
    else
        data.push_back(floatNAN);
    return cp;

}

/* type id 0x41 Xbee status */
const unsigned char *WisardMote::readXbeeData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readUint16(cp,eos,7,1.0,data);
}

/* type id 0x49 pwr */
const unsigned char *WisardMote::readPwrData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    cp = readUint16(cp,eos,6,1.0,data);
    data[0] /= 1000.0; //millivolt to volt
    return cp;
}

/* type id 0x50-0x53 */
const unsigned char *WisardMote::readRnetData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,1,0.1,data);
}

/* type id 0x54-0x5b */
const unsigned char *WisardMote::readRswData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,1,0.1,data);
}

/* type id 0x5c-0x63 */
const unsigned char *WisardMote::readRlwData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    cp = readInt16(cp,eos,5,1.0,data);
    data[0] /= 10.0; // Rpile
    for (int i = 1; i < 5; i++) {
        data[i] /= 100.0; // Tcase and Tdome1-3
    }
    return cp;
}

/* type id 0x64-0x6b */
const unsigned char *WisardMote::readRlwKZData(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    cp = readInt16(cp,eos,2,1.0,data);
    data[0] /= 10.0; // Rpile
    data[1] /= 100.0; // Tcase
    return cp;
}

/* type id 0x6c-0x6f */
const unsigned char *WisardMote::readCNR2Data(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,2,0.1,data);
}


/*  tyep id 0x70 -73*/
const unsigned char *WisardMote::readRswData2(const unsigned char *cp,
        const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
    return readInt16(cp,eos,2,0.1,data);
}

void WisardMote::initFuncMap() {
    if (!_functionsMapped) {
        _nnMap[0x01] = &WisardMote::readPicTm;
        _nnMap[0x04] = &WisardMote::readGenShort;
        _nnMap[0x05] = &WisardMote::readGenLong;

        _nnMap[0x0b] = &WisardMote::readSecOfYear;
        _nnMap[0x0c] = &WisardMote::readTmCnt;
        _nnMap[0x0d] = &WisardMote::readTm100thSec;
        _nnMap[0x0e] = &WisardMote::readTm10thSec;
        _nnMap[0x0f] = &WisardMote::readPicDT;

        _nnMap[0x20] = &WisardMote::readTsoilData;
        _nnMap[0x21] = &WisardMote::readTsoilData;
        _nnMap[0x22] = &WisardMote::readTsoilData;
        _nnMap[0x23] = &WisardMote::readTsoilData;

        _nnMap[0x24] = &WisardMote::readGsoilData;
        _nnMap[0x25] = &WisardMote::readGsoilData;
        _nnMap[0x26] = &WisardMote::readGsoilData;
        _nnMap[0x27] = &WisardMote::readGsoilData;

        _nnMap[0x28] = &WisardMote::readQsoilData;
        _nnMap[0x29] = &WisardMote::readQsoilData;
        _nnMap[0x2a] = &WisardMote::readQsoilData;
        _nnMap[0x2b] = &WisardMote::readQsoilData;

        _nnMap[0x2c] = &WisardMote::readTP01Data;
        _nnMap[0x2d] = &WisardMote::readTP01Data;
        _nnMap[0x2e] = &WisardMote::readTP01Data;
        _nnMap[0x2f] = &WisardMote::readTP01Data;

        _nnMap[0x30] = &WisardMote::readG5ChData;
        _nnMap[0x31] = &WisardMote::readG5ChData;
        _nnMap[0x32] = &WisardMote::readG5ChData;
        _nnMap[0x33] = &WisardMote::readG5ChData;

        _nnMap[0x34] = &WisardMote::readG4ChData;
        _nnMap[0x35] = &WisardMote::readG4ChData;
        _nnMap[0x36] = &WisardMote::readG4ChData;
        _nnMap[0x37] = &WisardMote::readG4ChData;

        _nnMap[0x38] = &WisardMote::readG1ChData;
        _nnMap[0x39] = &WisardMote::readG1ChData;
        _nnMap[0x3a] = &WisardMote::readG1ChData;
        _nnMap[0x3b] = &WisardMote::readG1ChData;

        _nnMap[0x40] = &WisardMote::readStatusData;
        _nnMap[0x41] = &WisardMote::readXbeeData;
        _nnMap[0x49] = &WisardMote::readPwrData;

        _nnMap[0x50] = &WisardMote::readRnetData;
        _nnMap[0x51] = &WisardMote::readRnetData;
        _nnMap[0x52] = &WisardMote::readRnetData;
        _nnMap[0x53] = &WisardMote::readRnetData;

        _nnMap[0x54] = &WisardMote::readRswData;
        _nnMap[0x55] = &WisardMote::readRswData;
        _nnMap[0x56] = &WisardMote::readRswData;
        _nnMap[0x57] = &WisardMote::readRswData;

        _nnMap[0x58] = &WisardMote::readRswData;
        _nnMap[0x59] = &WisardMote::readRswData;
        _nnMap[0x5a] = &WisardMote::readRswData;
        _nnMap[0x5b] = &WisardMote::readRswData;

        _nnMap[0x5c] = &WisardMote::readRlwData;
        _nnMap[0x5d] = &WisardMote::readRlwData;
        _nnMap[0x5e] = &WisardMote::readRlwData;
        _nnMap[0x5f] = &WisardMote::readRlwData;

        _nnMap[0x60] = &WisardMote::readRlwData;
        _nnMap[0x61] = &WisardMote::readRlwData;
        _nnMap[0x62] = &WisardMote::readRlwData;
        _nnMap[0x63] = &WisardMote::readRlwData;

        _nnMap[0x64] = &WisardMote::readRlwKZData;
        _nnMap[0x65] = &WisardMote::readRlwKZData;
        _nnMap[0x66] = &WisardMote::readRlwKZData;
        _nnMap[0x67] = &WisardMote::readRlwKZData;

        _nnMap[0x68] = &WisardMote::readRlwKZData;
        _nnMap[0x69] = &WisardMote::readRlwKZData;
        _nnMap[0x6a] = &WisardMote::readRlwKZData;
        _nnMap[0x6b] = &WisardMote::readRlwKZData;

        _nnMap[0x6c] = &WisardMote::readCNR2Data;
        _nnMap[0x6d] = &WisardMote::readCNR2Data;
        _nnMap[0x6e] = &WisardMote::readCNR2Data;
        _nnMap[0x6f] = &WisardMote::readCNR2Data;

        _nnMap[0x70] = &WisardMote::readRswData2;
        _nnMap[0x71] = &WisardMote::readRswData2;
        _nnMap[0x72] = &WisardMote::readRswData2;
        _nnMap[0x73] = &WisardMote::readRswData2;

        _typeNames[0x01] = "PicTm";
        _typeNames[0x04] = "GenShort";
        _typeNames[0x05] = "GenLong";

        _typeNames[0x0b] = "SecOfYear";
        _typeNames[0x0c] = "TmCnt";
        _typeNames[0x0d] = "Tm100thSec";
        _typeNames[0x0e] = "Tm10thSec";
        _typeNames[0x0f] = "PicDT";

        _typeNames[0x20] = "Tsoil";
        _typeNames[0x21] = "Tsoil";
        _typeNames[0x22] = "Tsoil";
        _typeNames[0x23] = "Tsoil";

        _typeNames[0x24] = "Gsoil";
        _typeNames[0x25] = "Gsoil";
        _typeNames[0x26] = "Gsoil";
        _typeNames[0x27] = "Gsoil";

        _typeNames[0x28] = "Qsoil";
        _typeNames[0x29] = "Qsoil";
        _typeNames[0x2a] = "Qsoil";
        _typeNames[0x2b] = "Qsoil";

        _typeNames[0x2c] = "TP01";
        _typeNames[0x2d] = "TP01";
        _typeNames[0x2e] = "TP01";
        _typeNames[0x2f] = "TP01";

        _typeNames[0x30] = "G5CH";
        _typeNames[0x31] = "G5CH";
        _typeNames[0x32] = "G5CH";
        _typeNames[0x33] = "G5CH";

        _typeNames[0x34] = "G4CH";
        _typeNames[0x35] = "G4CH";
        _typeNames[0x36] = "G4CH";
        _typeNames[0x37] = "G4CH";

        _typeNames[0x38] = "G1CH";
        _typeNames[0x39] = "G1CH";
        _typeNames[0x3a] = "G1CH";
        _typeNames[0x3b] = "G1CH";

        _typeNames[0x40] = "Sampling Mode";     // don't really know what this is
        _typeNames[0x41] = "Xbee Status";
        _typeNames[0x49] = "Power Monitor";

        _typeNames[0x50] = "Q7 Net Radiometer";
        _typeNames[0x51] = "Q7 Net Radiometer";
        _typeNames[0x52] = "Q7 Net Radiometer";
        _typeNames[0x53] = "Q7 Net Radiometer";

        _typeNames[0x54] = "Uplooking Pyranometer (Rsw.in)";
        _typeNames[0x55] = "Uplooking Pyranometer (Rsw.in)";
        _typeNames[0x56] = "Uplooking Pyranometer (Rsw.in)";
        _typeNames[0x57] = "Uplooking Pyranometer (Rsw.in)";

        _typeNames[0x58] = "Downlooking Pyranometer (Rsw.out)";
        _typeNames[0x59] = "Downlooking Pyranometer (Rsw.out)";
        _typeNames[0x5a] = "Downlooking Pyranometer (Rsw.out)";
        _typeNames[0x5b] = "Downlooking Pyranometer (Rsw.out)";

        _typeNames[0x5c] = "Uplooking Epply Pyrgeometer (Rlw.in)";
        _typeNames[0x5d] = "Uplooking Epply Pyrgeometer (Rlw.in)";
        _typeNames[0x5e] = "Uplooking Epply Pyrgeometer (Rlw.in)";
        _typeNames[0x5f] = "Uplooking Epply Pyrgeometer (Rlw.in)";

        _typeNames[0x60] = "Downlooking Epply Pyrgeometer (Rlw.out)";
        _typeNames[0x61] = "Downlooking Epply Pyrgeometer (Rlw.out)";
        _typeNames[0x62] = "Downlooking Epply Pyrgeometer (Rlw.out)";
        _typeNames[0x63] = "Downlooking Epply Pyrgeometer (Rlw.out)";

        _typeNames[0x64] = "Uplooking K&Z Pyrgeometer (Rlw.in)";
        _typeNames[0x65] = "Uplooking K&Z Pyrgeometer (Rlw.in)";
        _typeNames[0x66] = "Uplooking K&Z Pyrgeometer (Rlw.in)";
        _typeNames[0x67] = "Uplooking K&Z Pyrgeometer (Rlw.in)";

        _typeNames[0x68] = "Downlooking K&Z Pyrgeometer (Rlw.out)";
        _typeNames[0x69] = "Downlooking K&Z Pyrgeometer (Rlw.out)";
        _typeNames[0x6a] = "Downlooking K&Z Pyrgeometer (Rlw.out)";
        _typeNames[0x6b] = "Downlooking K&Z Pyrgeometer (Rlw.out)";

        _typeNames[0x6c] = "CNR2 Net Radiometer";
        _typeNames[0x6d] = "CNR2 Net Radiometer";
        _typeNames[0x6e] = "CNR2 Net Radiometer";
        _typeNames[0x6f] = "CNR2 Net Radiometer";

        _typeNames[0x70] = "Diffuse shortwave";
        _typeNames[0x71] = "Diffuse shortwave";
        _typeNames[0x72] = "Diffuse shortwave";
        _typeNames[0x73] = "Diffuse shortwave";
        _functionsMapped = true;
    }
}

//  %c will be replaced by 'a','b','c', or 'd' for the range of sensor types
//  %m in the variable names below will be replaced by the decimal mote number
SampInfo WisardMote::_samps[] = {
    { 0x0b, 0x0b, {
                      { "Clockdiff.m%m", "secs","Time difference: sampleTimeTag - moteTime", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IMPLIED
    },
    { 0x0e, 0x0e, {
                      { "Tdiff.m%m", "secs","Time difference, adam-mote", "$ALL_DEFAULT" },
                      { "Tdiff2.m%m", "secs", "Time difference, adam-mote-first_diff", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x20, 0x23, {
                      {"Tsoil.0.6cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.1.9cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.3.1cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.4.4cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x24, 0x27, {
                      { "Gsoil.%c_m%m", "W/m^2", "Soil Heat Flux", "$GSOIL_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x28, 0x2b, {
                      { "Qsoil.%c_m%m", "vol%", "Soil Moisture", "$QSOIL_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x2c, 0x2f, {
                      { "Vheat.%c_m%m", "V", "TP01 heater voltage", "$VHEAT_RANGE" },
                      { "Vpile.on.%c_m%m", "microV", "TP01 thermopile after heating", "$VPILE_RANGE" },
                      { "Vpile.off.%c_m%m", "microV", "TP01 thermopile before heating", "$VPILE_RANGE" },
                      { "Tau63.%c_m%m", "secs", "TP01 time to decay to 37% of Vpile.on-Vpile.off", "$TAU63_RANGE" },
                      { "lambdasoil.%c_m%m", "W/mDegk", "TP01 derived thermal conductivity", "$LAMBDA_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x30, 0x33, {
                      { "G5_c1.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G5_c2.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G5_c3.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G5_c4.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G5_c5.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x34, 0x37, {
                      { "G4_c1.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G4_c2.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G4_c3.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { "G4_c4.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x38, 0x3b, {
                      { "G1_c1.%c_m%m", "",	"", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x41, 0x41, {
                      { "XbeeStatus.%m", "", "Xbee status", "-10 10" },
                      {0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x49, 0x49, {
                      { "Vin.m%m", "V", "Supply voltage", "$VIN_RANGE" },
                      { "Iin.m%m", "A", "Supply current", "$IIN_RANGE" },
                      { "I33.m%m", "A", "3.3 V current", "$IIN_RANGE" },
                      { "Isensors.m%m", "A", "Sensor current", "$IIN_RANGE" },
                      {0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x50, 0x53, {
                      { "Rnet.%c_m%m", "W/m^2", "Net Radiation", "$RNET_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x54, 0x57, {
                      { "Rsw.in.%c_m%m", "W/m^2", "Incoming Short Wave", "$RSWIN_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x58, 0x5b, {
                      { "Rsw.out.%c_m%m", "W/m^2", "Outgoing Short Wave", "$RSWOUT_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x5c, 0x5f, {
                      { "Rpile.in.%c_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE" },
                      { "Tcase.in.a_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE" },
                      { "Tdome1.in.a_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE" },
                      { "Tdome2.in.a_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE" },
                      { "Tdome3.in.a_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x60, 0x63, {
                      { "Rpile.out.%c_m%m", "W/m^2", "Epply pyrgeometer thermopile, outgoing", "$RPILE_RANGE" },
                      { "Tcase.out.%c_m%m", "degC", "Epply case temperature, outgoing", "$TCASE_RANGE" },
                      { "Tdome1.out.%c_m%m", "degC", "Epply dome temperature #1, outgoing", "$TDOME_RANGE" },
                      { "Tdome2.out.%c_m%m", "degC", "Epply dome temperature #2, outgoing", "$TDOME_RANGE" },
                      { "Tdome3.out.%c_m%m", "degC", "Epply dome temperature #3, outgoing", "$TDOME_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x64, 0x67, {
                      { "Rpile.in.%ckz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, incoming", "$RPILE_RANGE" },
                      { "Tcase.in.%ckz_m%m", "degC", "K&Z case temperature, incoming", "$TCASE_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x68, 0x6b, {
                      { "Rpile.out.%ckz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, outgoing", "$RPILE_RANGE" },
                      { "Tcase.out.%ckz_m%m", "degC", "K&Z case temperature, outgoing", "$TCASE_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x6c, 0x6f, {
                      { "Rsw.net.%c_m%m", "W/m^2", "CNR2 net short-wave radiation", "$RSWNET_RANGE" },
                      { "Rlw.net.%c_m%m", "W/m^2", "CNR2 net long-wave radiation", "$RLWNET_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x70, 0x73, {
                      { "Rsw.dfs.%c_m%m", "W/m^2", "Diffuse short wave", "$RSWIN_RANGE" },
                      { "Rsw.direct.%c_m%m", "W/m^2", "Direct short wave", "$RSWIN_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0, 0, { {} }, WST_NORMAL },
};
