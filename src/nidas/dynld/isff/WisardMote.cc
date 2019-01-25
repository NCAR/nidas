// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include "WisardMote.h"
#include <nidas/util/Logger.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/util/util.h>
#include <nidas/util/UTime.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/IOException.h>

#include <boost/regex.hpp>

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

using namespace nidas::dynld;
using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace nidas::util;
using namespace std;
using namespace boost;

#define MSECS_PER_HALF_DAY 43200000

///*
// * Used for AutoConfig
// */
//static void compileRegex();
//static void freeRegex();

/* static */
bool WisardMote::_functionsMapped = false;

/* static */
map<int, pair<WisardMote::unpack_t,unsigned int> > WisardMote::_unpackMap;

/* static */
map<int, string> WisardMote::_typeNames;

/* static */
const n_u::EndianConverter * WisardMote::fromLittle =
    n_u::EndianConverter::getConverter(
            n_u::EndianConverter::EC_LITTLE_ENDIAN);

/* static */
map<dsm_sample_id_t,WisardMote*> WisardMote::_processorSensors;

NIDAS_CREATOR_FUNCTION_NS(isff, WisardMote)

WisardMote::WisardMote() :
	SerialSensor(DEFAULT_PORT_CONFIG),
	_sampleTagsById(),
    _processorSensor(0),
    _sensorSerialNumbersByMoteIdAndType(),
    _sequenceNumbersByMoteId(),
    _badCRCsByMoteId(),
    _tdiffByMoteId(),
    _numBadSensorTypes(),
    _unconfiguredMotes(),
    _noSampleTags(),
    _ignoredSensorTypes(),
    _nowarnSensorTypes(),
    _tsoilData(),
    defaultMessageConfig(DEFAULT_MESSAGE_LENGTH, DEFAULT_MSG_SEP_CHARS, DEFAULT_MSG_SEP_EOM),
    _scienceParameters(), _epilogScienceParameters(),
    _scienceParametersOk(false),
    _commandTable(), _cfgParameters(), _configMetaData()
{
    setDuplicateIdOK(true);
    initFuncMap();

    /*
     * AutoConfig setup
     *
     * We set the defaults at construction,
     * letting the base class modify according to fromDOMElement()
     */
    setMessageParameters(defaultMessageConfig);

//    compileRegex();
    initAutoCfg();
}
WisardMote::~WisardMote()
{
//	freeRegex();
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

    // make a copy, since removeSampleTag() will change the reference
    list<SampleTag*> configTags = getSampleTags();
    list<SampleTag*>::const_iterator ti = configTags.begin();

    // loop over the configured sample tags, creating the ones we want
    // using the "motes" and "stypes" parameters. Delete the original
    // configured tags.
    list<SampleTag*> newtags;
    for ( ; ti != configTags.end(); ++ti ) {
        SampleTag* stag = *ti;
        createSampleTags(stag,motev,newtags);
        removeSampleTag(stag);
    }

    ti = newtags.begin();
    for ( ; ti != newtags.end(); ++ti )
        _processorSensor->addMoteSampleTag(*ti);

    _processorSensor->addImpliedSampleTags(motev);

    if (_processorSensor == this) 
        checkLessUsedSensors();

    VLOG(("final getSampleTags().size()=") << getSampleTags().size());
    VLOG(("final _sampleTagsByIdTags.size()=") << _sampleTagsById.size());
    SerialSensor::validate();
}

void WisardMote::createSampleTags(const SampleTag* stag,const vector<int>& sensorMotes,list<SampleTag*>& newtags)
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
            newtag->setSampleId((mote << 8) + stype);
            for (unsigned int iv = 0; iv < newtag->getVariables().size(); iv++) {
                Variable& var = newtag->getVariable(iv);
                var.setPrefix(n_u::replaceChars(var.getPrefix(),"%m",motestr));
                var.setPrefix(n_u::replaceChars(var.getPrefix(),"%c",string(1,(char)('a' + is))));
                var.setSuffix(n_u::replaceChars(var.getSuffix(),"%m",motestr));
                var.setSuffix(n_u::replaceChars(var.getSuffix(),"%c",string(1,(char)('a' + is))));
            }
            newtags.push_back(newtag);
        }
    }
}

void WisardMote::addMoteSampleTag(SampleTag* tag)
{
    VLOG(("addSampleTag, id=") << tag->getDSMId() << ','
         << hex << tag->getSpSId() << dec
         << ", ntags=" << getSampleTags().size());
    
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
void WisardMote::checkLessUsedSensors()
{
    // accumulate sets of WST_IGNORED and WST_NOWARN sensor types.
    for (unsigned int itype = 0;; itype++) {
        int stype1 = _samps[itype].firstst;
        if (stype1 == 0) break;
        WISARD_SAMPLE_TYPE type = _samps[itype].type;
        int stype2 = _samps[itype].lastst;
        for (int stype = stype1; stype <= stype2; stype++) {
            switch (type) {
                case WST_IGNORED:
                    _ignoredSensorTypes.insert(stype);
                    break;
                case WST_NOWARN:
                    _nowarnSensorTypes.insert(stype);
                    break;
                default:
                    break;
            }
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

    SampleTag* newtag = new SampleTag(this);
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
    const char *sos =
        (const char *) samp->getConstVoidDataPtr();
    const char *eos = sos + samp->getDataByteLength();
    const char *cp = sos;

    dsm_time_t ttag = samp->getTimeTag();

    /*  check for good EOM  */
    if (!(eos = checkEOM(cp, eos,ttag)))
        return false;

    /*  verify crc for data  */
    if (!(eos = checkCRC(cp, eos, ttag)))
        return false;

    /*  read message header */
    struct MessageHeader header;

    if (!readHead(cp, eos, ttag, &header)) return false;

    if (header.messageType != 1)
        return false; // other than a data message

    while (cp < eos) {

        /* get Wisard sensor type */
        unsigned int sensorType = (unsigned char)*cp++;

        VLOG(("%s: %s, moteId=%d, sensorid=%#x, sensorType=%#x",
              getName().c_str(),
              n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
              header.moteId, getSensorId(),sensorType));

        /* find the appropriate member function to unpack the data for this sensorType */
        const pair<unpack_t,int>& upair = _unpackMap[sensorType];
        unpack_t unpack = upair.first;

        if (unpack == NULL) {
            if (!( _numBadSensorTypes[header.moteId][sensorType]++ % 1000))
                WLOG(("%s: %s, moteId=%d: unknown sensorType=%#.2x, at byte %u, #times=%u",
                        getName().c_str(),
                        n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                        header.moteId, sensorType,
                        (unsigned int)(cp-sos-1),_numBadSensorTypes[header.moteId][sensorType]));
            break;
        }

        unsigned int nfields = upair.second;

        // sample id of processed sample
        dsm_sample_id_t sid = getId() + (header.moteId << 8) + sensorType;
        SampleTag* stag = _sampleTagsById[sid];
        SampleT<float>* osamp = 0;

        /* create an output floating point sample */
        if (stag) {
            const vector<Variable*>& vars = stag->getVariables();
            unsigned int slen = vars.size();
            osamp = getSample<float> (std::max(slen,nfields));
            osamp->setId(sid);
            osamp->setTimeTag(ttag);
        }
        else {
            // if a sample is ignored, we don't create a sample, but still
            // need to keep parsing this raw sample.
            bool ignore = _ignoredSensorTypes.find(sensorType) != _ignoredSensorTypes.end();
            if (!ignore) {
                bool warn = _nowarnSensorTypes.find(sensorType) == _nowarnSensorTypes.end();
                if (warn) {
                    if (!( _noSampleTags[header.moteId][sensorType]++ % 1000))
                        WLOG(("%s: %s, no sample tag for %d,%#x, mote=%d, sensorType=%#.2x, #times=%u",
                                getName().c_str(),
                                n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                                GET_DSM_ID(sid),GET_SPS_ID(sid),header.moteId,sensorType,_noSampleTags[header.moteId][sensorType]));
                }
                osamp = getSample<float> (nfields);
                osamp->setId(sid);
                osamp->setTimeTag(ttag);
            }
        }

        /* unpack the sample for this sensorType.
         * If osamp is NULL, the character pointer will be
         * moved past the field for this sensorType, but (obviously) no
         * data stored in the sample. */
        cp = (this->*unpack)(cp, eos, nfields, &header, stag, osamp);

        /* push out */
        if (osamp) results.push_back(osamp);
    }
    return true;
}

void WisardMote::convert(SampleTag* stag, SampleT<float>* osamp, float* results)
{
    applyConversions(stag, osamp, results);
}

/*
 * read mote id: return -1 invalid.  >0 valid mote id.
 */
int WisardMote::readMoteId(const char* &cp, const char*eos)
{
    if (eos - cp < 4) return -1;    // IDn:

    if (memcmp(cp,"ID",2)) return -1;
    cp += 2;

    int l = strspn(cp,"0123456789");
    if (l == 0) return -1;
    l = std::min((int)(eos-cp)-1,l);

    if (*(cp + l) != ':') return -1;

    int moteId = atoi(cp);
    cp += l + 1;

    return moteId;
}

/*
 * Read initial portion of a Wisard message, filling in a struct MessageHeader.
 */
bool WisardMote::readHead(const char *&cp, const char *eos,
        dsm_time_t ttag, struct MessageHeader* hdr)
{
    hdr->moteId = readMoteId(cp,eos);
    if (hdr->moteId < 0) return false;

    // version number
    if (cp == eos) return false;
    hdr->version = *cp++;

    // message type
    if (cp == eos) return false;
    hdr->messageType = *cp++;

    switch (hdr->messageType) {
    case 0:
        /* unpack 1 bytesId + 2 byte s/n */
        while (cp + 3 <= eos) {
            int sensorType = *cp++;
            int serialNumber = WisardMote::fromLittle->uint16Value(cp);
            cp += sizeof(short);
            // log serial number if it changes.
            if (_sensorSerialNumbersByMoteIdAndType[hdr->moteId][sensorType]
                                                             != serialNumber) {
                _sensorSerialNumbersByMoteIdAndType[hdr->moteId][sensorType]
                                                             = serialNumber;
                ILOG(("%s: %s, mote=%d, sensorType=%#.2x SN=%d, typeName=%s",
                        getName().c_str(),
                        n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%3f").c_str(),
                        hdr->moteId, sensorType,
                        serialNumber,_typeNames[sensorType].c_str()));
            }
        }
        break;
    case 1:
        /* unpack 1byte sequence */
        if (cp == eos)
            return false;
        _sequenceNumbersByMoteId[hdr->moteId] = *cp++;
        VLOG(("mote=%d, Ver=%d MsgType=%d seq=%d",
              hdr->moteId, hdr->version, hdr->messageType,
              _sequenceNumbersByMoteId[hdr->moteId]));
        break;
    case 2:
        VLOG(("mote=%d, Ver=%d MsgType=%d ErrMsg=\"",
              hdr->moteId, hdr->version,
              hdr->messageType) << string((const char *) cp, eos - cp) << "\"");
        break;
    default:
        WLOG(("%s: %s, unknown msgType, mote=%d, Ver=%d MsgType=%d, len=%d",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%3f").c_str(),
                hdr->moteId, hdr->version, hdr->messageType, (int)(eos - cp)+3));
        return false;
    }
    return true;
}

/*
 * Check EOM (0x03 0x04 0xd). Return pointer to start of EOM.
 */
const char *WisardMote::checkEOM(const char *sos, const char *eos, dsm_time_t ttag)
{
    if (eos - 4 < sos) {
        WLOG(("%s: %s, message length is too short, len=%d",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%3f").c_str(),
                (int)(eos - sos)));
        return 0;
    }
    // NIDAS will likely add a NULL to the end of the message. Check for that.
    if (*(eos - 1) == 0)
        eos--;
    eos -= 3;

    if (memcmp(eos, "\x03\x04\r", 3) != 0) {
        NLOG(("%s: %s, bad EOM, last 3 chars= %x %x %x",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%3f").c_str(),
                    *(unsigned char*)(eos), *(unsigned char*)(eos + 1),*(unsigned char*)(eos + 2)));
        return 0;
    }
    return eos;
}

/*
 * Check CRC.
 * Return pointer to CRC, which is one past the end of the data portion,
 * or if the CRC is bad, return 0.
 */
const char *WisardMote::checkCRC(const char *cp, const char *eos, dsm_time_t ttag)
{
    // eos points to one past the CRC.
    const char* crcp = eos - 1;

    int origlen = (int)(eos - cp) + 3; // include the EOM (0x03 0x04 0xd)

    if (crcp <= cp) {
        WLOG(("%s: %s, message length is too short, len=%d",
                getName().c_str(),
                n_u::UTime(ttag).format(true,"%Y %m %d %H:%M:%S.%3f").c_str(),
                origlen));
        return 0;
    }

    // read value of CRC at end of message.
    unsigned char crc = *crcp;

    // Calculate checksum. Start with length of message, not including checksum.
    unsigned char cksum = (int)(crcp - cp);
    for (const char *cp2 = cp; cp2 < crcp;)
        cksum ^= *cp2++;


    if (cksum != crc) {

        int moteId = -1;

        const char* idstr = strstr(cp,"ID");
        if (!idstr) {
            if (!(_badCRCsByMoteId[moteId]++ % 100)) {
                NLOG(("%s: %s, bad checksum and no ID in message, len=%d, #bad=%u",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                            origlen,_badCRCsByMoteId[moteId]));
            }
            return 0;
        }
        else if (idstr > cp) {
            if (!(_badCRCsByMoteId[moteId]++ % 100)) {
                NLOG(("%s: %s, bad checksum and %d bad characters before ID, message len=%d, #bad=%u",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                            (int)(idstr-cp),origlen,_badCRCsByMoteId[moteId]));
            }
            cp = idstr;
            return checkCRC(cp, eos, ttag);
        }
        // Print out mote id
        moteId = readMoteId(cp, eos);
        if (moteId > 0) {
            if (!(_badCRCsByMoteId[moteId]++ % 10)) {
                NLOG(("%s: %s, bad checksum for mote id %d, length=%d, tx crc=%#.2x, calc crc=%#.2x, #bad=%u",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                            moteId, origlen,
                            crc,cksum,_badCRCsByMoteId[moteId]));
            }
        } else {
            if (!(_badCRCsByMoteId[moteId]++ % 100)) {
                NLOG(("%s: %s, bad checksum for unknown mote, length=%d, tx crc=%#.2x, calc crc=%#.2x, #bad=%u",
                            getName().c_str(),
                            n_u::UTime(ttag).format(true, "%Y %m %d %H:%M:%S.%3f").c_str(),
                             origlen, crc, cksum,_badCRCsByMoteId[moteId]));
            }
        }
        return 0;
    }
    return eos-1;
}

/* unnamed namespace */
namespace {

    const unsigned char _missValueUint8 = 0x80;

    const short _missValueInt16 = (signed) 0x8000;

    const unsigned short _missValueUint16 = (unsigned) 0x8000;

    const unsigned int _missValueUint32 = 0x80000000;

    const int _missValueInt32 = 0x80000000;

    /*
     * Currently not used
     */
//    const char *readUint8(const char *cp, const char *eos,
//            unsigned int nfields,float scale, float *fp)
//    {
//        /* convert unsigned chars to float */
//        unsigned int i;
//        for (i = 0; i < nfields; i++) {
//            if (cp + sizeof(uint8_t) > eos) break;
//            unsigned char val = *cp++;
//            cp += sizeof(uint8_t);
//            if (fp) {
//                if (val != _missValueUint8)
//                    *fp++ = val * scale;
//                else
//                    *fp++ = nidas::core::floatNAN;
//            }
//        }
//        if (fp) for ( ; i < nfields; i++) *fp++ = nidas::core::floatNAN;
//        return cp;
//    }

    const char *readUint16(const char *cp, const char *eos,
            unsigned int nfields,float scale, float *fp)
    {
        /* unpack 16 bit unsigned integers */
        unsigned int i;
        for (i = 0; i < nfields; i++) {
            if (cp + sizeof(uint16_t) > eos) break;
            unsigned short val = WisardMote::fromLittle->uint16Value(cp);
            cp += sizeof(uint16_t);
            if (fp) {
                if (val != _missValueUint16)
                    *fp++ = val * scale;
                else
                    *fp++ = nidas::core::floatNAN;
            }
        }
        if (fp) for ( ; i < nfields; i++) *fp++ = nidas::core::floatNAN;
        return cp;
    }

    const char *readInt16(const char *cp, const char *eos,
            unsigned int nfields,float scale, float *fp)
    {
        /* unpack 16 bit signed integers */
        unsigned int i;
        for (i = 0; i < nfields; i++) {
            if (cp + sizeof(int16_t) > eos) break;
            signed short val = WisardMote::fromLittle->int16Value(cp);
            cp += sizeof(int16_t);
            if (fp) {
                if (val != _missValueInt16)
                    *fp++ = val * scale;
                else
                    *fp++ = nidas::core::floatNAN;
            }
        }
        if (fp) for ( ; i < nfields; i++) *fp++ = nidas::core::floatNAN;
        return cp;
    }

    const char *readUint32(const char *cp, const char *eos,
            unsigned int nfields,float scale, float* fp)
    {
        /* unpack 32 bit unsigned ints */
        unsigned int i;
        for (i = 0; i < nfields; i++) {
            if (cp + sizeof(uint32_t) > eos) break;
            unsigned int val = WisardMote::fromLittle->uint32Value(cp);
            cp += sizeof(uint32_t);
            if (fp) {
                if (val != _missValueUint32)
                    *fp++ = val * scale;
                else
                    *fp++ = nidas::core::floatNAN;
            }
        }
        if (fp) for ( ; i < nfields; i++) *fp++ = nidas::core::floatNAN;
        return cp;
    }

    const char *readInt32(const char *cp, const char *eos,
            unsigned int nfields,float scale, float* fp)
    {
        /* unpack 32 bit unsigned ints */
        unsigned int i;
        for (i = 0; i < nfields; i++) {
            if (cp + sizeof(uint32_t) > eos) break;
            int val = WisardMote::fromLittle->int32Value(cp);
            cp += sizeof(int32_t);
            if (val != _missValueInt32)
                *fp++ = val * scale;
            else
                *fp++ = nidas::core::floatNAN;
        }
        for ( ; i < nfields; i++) *fp++ = nidas::core::floatNAN;
        return cp;
    }

}   // unnamed namespace

const char* WisardMote::unpackPicTime(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 1);

    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    // PIC time, convert from tenths of sec to sec
    cp =  readUint16(cp,eos,nfields,0.1,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackUint16(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{

    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }
    
    cp = readUint16(cp,eos,nfields,1.0,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackInt16(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{

    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,1.0,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackUint32(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readUint32(cp,eos,nfields,1.0,fp);

    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackInt32(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt32(cp,eos,nfields,1.0,fp);

    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackAccumSec(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 1);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
        *fp = floatNAN;
    }

    if (cp + sizeof(uint32_t) <= eos) {
        unsigned int val = WisardMote::fromLittle->uint32Value(cp);    // accumulated seconds
        if (fp && val != _missValueUint32 && val != 0) {
            struct tm tm;
            n_u::UTime ut(osamp->getTimeTag());
            ut.toTm(true,&tm);

            // compute time on Jan 1, 00:00 UTC of year from sample time tag
            tm.tm_sec = tm.tm_min = tm.tm_hour = tm.tm_mon = 0;
            tm.tm_mday = 1;
            tm.tm_yday = -1;
            ut = n_u::UTime::fromTm(true,&tm);

            VLOG(("") << "ttag="
                 << n_u::UTime(osamp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%6f")
                 << ",ut=" << ut.format(true,"%Y %m %d %H:%M:%S.%6f"));
            VLOG(("") << "ttag=" << osamp->getTimeTag() << ", ut=" << ut.toUsecs()
                 << ", val=" << val);
            // will have a rollover issue on Dec 31 23:59:59, but we'll ignore it
            long long diff = (osamp->getTimeTag() - (ut.toUsecs() + (long long)val * USECS_PER_SEC));

            // bug in the mote timekeeping: the 0x0b values are 1 day too large
            if (::llabs(diff+USECS_PER_DAY) < 60 * USECS_PER_SEC) diff += USECS_PER_DAY;
            *fp = (float)diff / USECS_PER_SEC;
        }
        cp += sizeof(uint32_t);
    }
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpack100thSec(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 1);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readUint32(cp,eos,nfields,0.01,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpack10thSec(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader* hdr,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 2);

    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    /* unpack  32 bit  t-tm-ticks in 10th sec */
    unsigned int val = 0;
    if (cp + sizeof(uint32_t) > eos) {
        if (fp) for (unsigned int n = 0; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        return cp;
    }
    val = WisardMote::fromLittle->uint32Value(cp);
    // convert mote time to 1/10th secs since 00:00 UTC
    val %= (SECS_PER_DAY * 10);
    // convert to milliseconds
    val *= 100;

    cp += sizeof(uint32_t);

    if (fp) {

        //convert sample time tag to milliseconds since 00:00 UTC
        int mSOfDay = (osamp->getTimeTag() / USECS_PER_MSEC) % MSECS_PER_DAY;

        int diff = mSOfDay - val; //mSec

        if (abs(diff) > MSECS_PER_HALF_DAY) {
            if (diff < -MSECS_PER_HALF_DAY)
                diff += MSECS_PER_DAY;
            else if (diff > MSECS_PER_HALF_DAY)
                diff -= MSECS_PER_DAY;
        }
        float fval = (float) diff / MSECS_PER_SEC; // seconds

        // keep track of the first time difference.
        if (_tdiffByMoteId[hdr->moteId] == 0)
            _tdiffByMoteId[hdr->moteId] = diff;

        // subtract the first difference from each succeeding difference.
        // This way we can check the mote clock drift relative to the adam
        // when the mote is not initialized with an absolute time.
        diff -= _tdiffByMoteId[hdr->moteId];
        if (abs(diff) > MSECS_PER_HALF_DAY) {
            if (diff < -MSECS_PER_HALF_DAY)
                diff += MSECS_PER_DAY;
            else if (diff > MSECS_PER_HALF_DAY)
                diff -= MSECS_PER_DAY;
        }

        float fval2 = (float) diff / MSECS_PER_SEC;

        fp[0] = fval;
        fp[1] = fval2;
        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackPicTimeFields(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 4);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    if (fp) for (unsigned int n = 0; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;

    /*  16 bit jday */
    if (cp + sizeof(uint16_t) > eos) return cp;
    unsigned short jday = WisardMote::fromLittle->uint16Value(cp);
    cp += sizeof(uint16_t);
    if (fp) {
        if (jday != _missValueUint16)
            fp[0] = jday;
        else
            fp[0] = floatNAN;
    }

    /*  8 bit hour+ 8 bit min+ 8 bit sec  */
    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char hh = *cp;
    cp += sizeof(uint8_t);
    if (fp) {
        if (hh != _missValueUint8)
            fp[1] = hh;
        else
            fp[1] = floatNAN;
    }

    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char mm = *cp;
    cp += sizeof(uint8_t);
    if (fp) {
        if (mm != _missValueUint8)
            fp[2] = mm;
        else
            fp[2] = floatNAN;
    }

    if (cp + sizeof(uint8_t) > eos) return cp;
    unsigned char ss = *cp;
    cp += sizeof(uint8_t);
    if (fp) {
        if (ss != _missValueUint8)
            fp[3] = ss;
        else
            fp[3] = floatNAN;
        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackTRH(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 3);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    // T * 100, RH * 100, fan current.
    cp = readInt16(cp,eos,nfields,0.01,fp);

    if (fp) {
        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        {
            fp[n] = floatNAN;
        }
        if (stag) {
            float results[3];
            convert(stag, osamp, results);
            // If ifan is bad or got filtered, then don't overwrite it's
            // value but filter T and RH.  Otherwise take all the results
            // as converted.
            if (::isnan(results[2]))
            {
                fp[0] = floatNAN;
                fp[1] = floatNAN;
            }
            else
            {
                memcpy(fp, results, sizeof(results));
            }
        }
    }
    return cp;
}

const char* WisardMote::unpackTsoil(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        assert(nfields >= 4);    // at least 4 temperatures
        fp = osamp->getDataPtr();
    }

    // This is not very efficient, but as a hack, we need to check if
    // TsoilUnsigned16 exists, and if it does, we want to treat it as an
    // uint16.
    bool tsoilIsUint16 = false;
    if (stag)
    {
        const Parameter* uint16samps = stag->getParameter("TsoilUnsigned16");
        if (uint16samps) {
            if (uint16samps->getType() != Parameter::BOOL_PARAM) throw n_u::InvalidParameterException(getName(),"TsoilUnsigned16","should be boolean type");
            tsoilIsUint16 = (bool) uint16samps->getNumericValue(0);
        }
    }
    cp = tsoilIsUint16 ? readUint16(cp,eos,NTSOILS,0.01,fp): readInt16(cp,eos,NTSOILS,0.01,fp);


    if (fp) {

        for (unsigned int it = NTSOILS; it < osamp->getDataLength(); it++)
        {
            fp[it] = floatNAN;
        }

        if (stag) {
            const vector<Variable*>& vars = stag->getVariables();
            unsigned int slen = vars.size();
            TsoilData& td = _tsoilData[stag->getId()];

            // sample should have variables for soil temps and their derivatives
            unsigned int ntsoils = slen / 2;
            unsigned int id = ntsoils;   // index of derivative
            for (unsigned int it = 0; it < ntsoils; it++,id++) {
                // DLOG(("f[%d]= %f", it, *fp));
                if (it < slen) {
                    vars[it]->convert(osamp->getTimeTag(), &fp[it], 1);
                }

                if (id < osamp->getDataLength()) {
                    float f = fp[it];
                    // time derivative
                    float fd = floatNAN;
                    if (!::isnan(f)) {
                        fd = (f - td.tempLast[it]) / double((osamp->getTimeTag() - td.timeLast[it])) * USECS_PER_SEC;
                        td.tempLast[it] = f;
                        td.timeLast[it] = osamp->getTimeTag();

                        // pass time derivative through limit checks and converters
                        if (id < slen) {
                            vars[id]->convert(osamp->getTimeTag(), &fd, 1);
                        }
                    }
                    fp[id] = fd;
                }
            }

        }
    }
    return cp;
}

const char* WisardMote::unpackGsoil(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.1,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackQsoil(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.01,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackTP01(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 5);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    unsigned int i;

    cp = readUint16(cp,eos,2,1.0,fp);   // Vheat, Vpile.on
    cp = readInt16(cp,eos,1,1.0,(fp ? fp+2 : fp));  // Vpile.off, signed
    cp = readUint16(cp,eos,2,1.0,(fp ? fp+3 : fp)); // Tau63, lambdasoil

    if (fp) {
        /* fields:
         * 0    Vheat, volts
         * 1    Vpile.on, microvolts
         * 2    Vpile.off, microvolts
         * 3    Tau63, seconds
         * 4    lambdasoil, derived on the mote, from Vheat, Vpile.on, Vpile.off
         */
        fp[0] /= 10000.0;   // Vheat, volts
        fp[3] /= 100.0;     // Tau63, seconds
        fp[4] /= 1000.0;    // lambdasoil, heat conductivity in W/(m * K)

        for (i = nfields ; i < osamp->getDataLength(); i++) fp[i] = floatNAN;

        convert(stag, osamp);

        // set derived lambdasoil to NAN if any of Vheat, Vpile.on,
        // Vpile.off are NAN
        for (i = 0; i < 3; i++) if (::isnan(fp[i])) fp[4] = floatNAN;
    }

    return cp;
}

const char* WisardMote::unpackStatus(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 1);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    if (cp + 1 > eos) return cp;
    unsigned char val = *cp++;
    if (fp) {
        if (val != _missValueUint8)
            *fp = val;
        else
            *fp = floatNAN;

        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackXbee(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readUint16(cp,eos,nfields,1.0,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackPower(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields == 6);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    unsigned int n = 3;
    // voltage, currents are unsigned according to the documentation.
    // However, in CABL, counts for Icharge would flip to
    // ~65400K after passing through 0 at sundown. Treated
    // as signed this would be -0.136 Amps, which is more believable
    // than 65.4 Amps. We'll treat them as signed which gives
    // enough range (+-32.7) for battery voltags and currents.
    cp = readInt16(cp,eos,n,0.001,fp);

    // signed temperature
    cp = readInt16(cp,eos,1,0.01,(fp ? fp+n : 0));
    n++;

    // remaining fields
    cp = readInt16(cp,eos,nfields-n,1.0,(fp ? fp+n : 0));
    n = nfields;

    if (fp) {
        for (  ; n < osamp->getDataLength(); n++) fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackRnet(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.1,fp);
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackRsw(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.1,fp);  // multiplies by 0.1
    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackRlw(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields > 0);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,1.0,fp);
    if (fp) {
        fp[0] /= 10.0; // Rpile
        for (unsigned int n = 1; n < nfields; n++) {
            fp[n] /= 100.0; // Tcase and Tdome1-3
        }
        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackRlwKZ(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    assert(nfields > 1);
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,1.0,fp);
    if (fp) {
        fp[0] /= 10.0; // Rpile
        fp[1] /= 100.0; // Tcase

        for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

const char* WisardMote::unpackCNR2(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.1,fp);

    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackRsw2(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,0.1,fp);  // multiplies by 0.1

    if (fp) for (unsigned int n = nfields; n < osamp->getDataLength(); n++)
        fp[n] = floatNAN;
    convert(stag, osamp);
    return cp;
}

const char* WisardMote::unpackNR01(const char *cp, const char *eos,
        unsigned int nfields, const struct MessageHeader*,
        SampleTag* stag, SampleT<float>* osamp)
{
    float *fp = 0;
    if (osamp) {
        assert(osamp->getDataLength() >= nfields);
        fp = osamp->getDataPtr();
    }

    cp = readInt16(cp,eos,nfields,1.0,fp);
    if (fp) {
        unsigned int n;
        for (n = 0; n < 4 && n < nfields; n++) {
            fp[n] *= 0.1;   // 2xRsw, 2xRpile
        }
        if (n < nfields) fp[n++] *= 0.01;   // Tcase
        if (n < nfields) fp[n++] *= 0.001;  // Wetness
        for (int i = 0; i < 2 && n < nfields; n++)
            fp[n] *= 0.01;                  // possible extra 2xTcase
        for ( ; n < osamp->getDataLength(); n++)
            fp[n] = floatNAN;
        convert(stag, osamp);
    }
    return cp;
}

void WisardMote::initFuncMap()
{
    if (_functionsMapped) return;
    _unpackMap[0x01] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackPicTime,1);
    _typeNames[0x01] = "PicTime";

    _unpackMap[0x04] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt16,1);
    _typeNames[0x04] = "Int16";

    _unpackMap[0x05] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt32,1);
    _typeNames[0x05] = "Int32";

    _unpackMap[0x0b] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackAccumSec,1);
    _typeNames[0x0b] = "SecOfYear";

    _unpackMap[0x0c] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackUint32,1);
    _typeNames[0x0c] = "TimerCounter";

    _unpackMap[0x0d] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpack100thSec,1);
    _typeNames[0x0d] = "Time100thSec";

    _unpackMap[0x0e] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpack10thSec,2);
    _typeNames[0x0e] = "Time10thSec";

    _unpackMap[0x0f] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackPicTimeFields,4);
    _typeNames[0x0f] = "PicTimeFields";

    for (int i = 0x10; i < 0x14; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackTRH,3);
        _typeNames[i] = "TRH";
    }

    for (int i = 0x1c; i < 0x20; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackUint16,2);
        _typeNames[i] = "Rain";
    }

    for (int i = 0x20; i < 0x24; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackTsoil,NTSOILS*2);
        _typeNames[i] = "Tsoil";
    }

    for (int i = 0x24; i < 0x28; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackGsoil,1);
        _typeNames[i] = "Gsoil";
    }

    for (int i = 0x28; i < 0x2c; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackQsoil,1);
        _typeNames[i] = "Qsoil";
    }

    for (int i = 0x2c; i < 0x30; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackTP01,5);
        _typeNames[i] = "TP01";
    }

    for (int i = 0x30; i < 0x34; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt16,5);
        _typeNames[i] = "5 fields of Int16";
    }

    for (int i = 0x34; i < 0x38; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt16,4);
        _typeNames[i] = "4 fields of Int16";
    }

    for (int i = 0x38; i < 0x3c; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt16,1);
        _typeNames[i] = "1 field of Int16, often Wetness";
    }

    for (int i = 0x3c; i < 0x40; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackInt16,1);
        _typeNames[i] = "Infra-red surface temperature";
    }

    _unpackMap[0x40] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackStatus,1);
    _typeNames[0x40] = "Status";

    _unpackMap[0x41] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackXbee,7);
    _typeNames[0x41] = "Xbee Status";

    _unpackMap[0x49] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackPower,6);
    _typeNames[0x49] = "Power Monitor";

    for (int i = 0x4c; i < 0x50; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackNR01,8);
        _typeNames[i] = "Hukseflux NR01";
    }

    for (int i = 0x50; i < 0x54; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRnet,1);
        _typeNames[i] = "Q7 Net Radiometer";
    }

    for (int i = 0x54; i < 0x58; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRsw,1);
        _typeNames[i] = "Uplooking Pyranometer (Rsw.in)";
    }

    for (int i = 0x58; i < 0x5c; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRsw,1);
        _typeNames[i] = "Downlooking Pyranometer (Rsw.out)";
    }

    for (int i = 0x5c; i < 0x60; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRlw,5);
        _typeNames[i] = "Uplooking Epply Pyrgeometer (Rlw.in)";
    }

    for (int i = 0x60; i < 0x64; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRlw,5);
        _typeNames[i] = "Downlooking Epply Pyrgeometer (Rlw.out)";
    }

    for (int i = 0x64; i < 0x68; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRlwKZ,2);
        _typeNames[i] = "Uplooking K&Z Pyrgeometer (Rlw.in)";
    }

    for (int i = 0x68; i < 0x6c; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRlwKZ,2);
        _typeNames[i] = "Downlooking K&Z Pyrgeometer (Rlw.out)";
    }

    for (int i = 0x6c; i < 0x70; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackCNR2,2);
        _typeNames[i] = "CNR2 Net Radiometer";
    }

    for (int i = 0x70; i < 0x74; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRsw2,2);
        _typeNames[i] = "Diffuse shortwave";
    }

    for (int i = 0x74; i < 0x78; i++) {
        _unpackMap[i] = pair<WisardMote::unpack_t,unsigned int>(&WisardMote::unpackRsw,1);
        _typeNames[i] = "Photosynthetically active radiation";
    }

    _functionsMapped = true;
}

//  %c will be replaced by 'a','b','c', or 'd' for the range of sensor types
//  %m in the variable names below will be replaced by the decimal mote number
SampInfo WisardMote::_samps[] = {
    { 0x01, 0x01, {
                      { "PicTime.m%m", "secs","PIC Time", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x0b, 0x0b, {
                      { "Clockdiff.m%m", "secs","Time difference: sampleTimeTag - moteTime", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x0c, 0x0c, {
                      { "Timer.m%m", "","Some sort of timer counter", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x0d, 0x0d, {
                      { "Clock100.m%m", "","Wizard 100th sec", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x0e, 0x0e, {
                      { "Tdiff.m%m", "secs","Time difference, adam-mote", "$ALL_DEFAULT" },
                      { "Tdiff2.m%m", "secs", "Time difference, adam-mote-first_diff", "$ALL_DEFAULT" },
                      { 0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x1c, 0x1f, {
                      {"Raintip", "#", "Rain tip", "$RAIN_RANGE" },
                      {"Rainaccum", "#", "Accumulated rain tips", "$RAIN_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x20, 0x23, {
                      {"Tsoil.0.6cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.1.9cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.3.1cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"Tsoil.4.4cm.%c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE" },
                      {"dTsoil_dt.0.6cm.%c_m%m", "degC/s", "Time derivative of soil temp", "$DTSOIL_RANGE" },
                      {"dTsoil_dt.1.9cm.%c_m%m", "degC/s", "Time derivative of soil temp", "$DTSOIL_RANGE" },
                      {"dTsoil_dt.3.1cm.%c_m%m", "degC/s", "Time derivative of soil temp", "$DTSOIL_RANGE" },
                      {"dTsoil_dt.4.4cm.%c_m%m", "degC/s", "Time derivative of soil temp", "$DTSOIL_RANGE" },
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
    { 0x3c, 0x3f, {
                      { "Tsfc.%c_m%m", "W/m^2", "Infra-red surface temperature", "$T_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x41, 0x41, {
                      { "XbeeStatus.%m", "", "Xbee status", "-10 10" },
                      {0, 0, 0, 0 }
                  }, WST_IGNORED
    },
    { 0x49, 0x49, {
                      { "Vdsm.m%m", "V", "System voltage", "$VIN_RANGE" },
                      { "Idsm.m%m", "A", "Load current", "$IIN_RANGE" },
                      { "Icharge.m%m", "A", "Charging current", "$IIN_RANGE" },
                      { "Tcharge.m%m", "degC", "Charging system temperature", "$T_RANGE" },
                      {0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0x4c, 0x4f, {
                      { "Rsw.in.%c_%m", "W/m^2", "Incoming Short Wave, Hukseflux NR01", "$RSWIN_RANGE" },
                      { "Rsw.out.%c_%m", "W/m^2", "Outgoing Short Wave, Hukseflux NR01", "$RSWOUT_RANGE" },
                      { "Rpile.in.%c_%m", "W/m^2", "Incoming Thermopile, Hukseflux NR01", "$RPILE_RANGE" },
                      { "Rpile.out.%c_%m", "W/m^2", "Outgoing Thermopile, Hukseflux NR01", "$RPILE_RANGE" },
                      { "Tcase.%c_%m", "degC", "Average case temperature, Hukseflux NR01", "$T_RANGE" },
                      { "Wetness.%c_%m", "V", "Leaf wetness", "$WETNESS_RANGE" },
                      { "Tcase.in.%c_%m", "degC", "Incoming case temperature, Hukseflux NR01", "$T_RANGE" },
                      { "Tcase.out.%c_%m", "degC", "Outgoing case temperature, Hukseflux NR01", "$T_RANGE" },
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
    { 0x74, 0x77, {
                      { "Rpar.%c_m%m", "W/m^2", "Photosynthetically active radiation", "$RSWIN_RANGE" },
                      { 0, 0, 0, 0 }
                  }, WST_NORMAL
    },
    { 0, 0, { {0,0,0,0} }, WST_NORMAL },
};

/*
 * AutoConfig Details
 */
const PortConfig WisardMote::DEFAULT_PORT_CONFIG(DEFAULT_BAUD_RATE, DEFAULT_DATA_BITS, DEFAULT_PARITY, DEFAULT_STOP_BITS,
                                             	 DEFAULT_PORT_TYPE, DEFAULT_SENSOR_TERMINATION,
												 DEFAULT_RTS485, DEFAULT_CONFIG_APPLIED);

const int WisardMote::SENSOR_BAUDS[NUM_SENSOR_BAUDS] = {38400};

const WordSpec WisardMote::SENSOR_WORD_SPECS[NUM_SENSOR_WORD_SPECS] =
{
	{8,Termios::NONE,1},
};
const PORT_TYPES WisardMote::SENSOR_PORT_TYPES[NUM_PORT_TYPES] = {RS232};

//Default message parameters for the WisardMote
const char* WisardMote::DEFAULT_MSG_SEP_CHARS = "\x03\x04\r";

// Data output after reset
static const regex MODEL_ID_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} (Wisard(?:_[[:alnum:]]+)+) ResetSource = Software Reset");
static const regex RESET_SRC_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} Wisard.* ResetSource = ((?:[[:alpha:]]+[[:blank:]]*)+)");
static const regex VERSION_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} (V[[:digit:]]+.[[:digit:]]+)");
static const regex CPU_CLK_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} V[[:digit:]]+.[[:digit:]]+ ([[:alnum:]]+)");
static const regex TIMING_SRC_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} V[[:digit:]]+.[[:digit:]]+ [[:alnum:]]+ '(.*)'");
static const regex BUILD_DATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} BUILD: ([[:alnum:]]+)");
static const regex RTCC_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} Use ([[:alnum:]=]+)");

static const regex TEMP_SENSOR_INIT_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2}[[:blank:]]+(Initialize MCP9800, ResolutionBitMask =[[:digit:]]MCP9800 CfgReg=0x[[:digit:]]{2})");
static const regex SENSOR_SERNUMS_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2}[[:blank:]]+Serial-Numbers:[[:blank:]]+(.*)(?:\x01\x02|[[:space:]]+)");

// Data Rates
static const regex DATA_RATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'dr(?:=[[:digit:]])*'=([[:digit:]]+)");
static const regex PWR_SMPRATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'sp(?:=[[:digit:]])*'=([[:digit:]]+)");
static const regex SERNUM_RATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'sn(?:=[[:digit:]])*'=([[:digit:]]+)");

// operating modes
static const regex NODEID_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'id(?:=[[:digit:]])*'=([[:digit:]]+)");
static const regex MSG_FMT_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'mp'*=([[:digit:]])'*");
static const regex OUT_PORT_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'pp'*=([[:digit:]])'*");
static const regex SENSORS_ON_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'sensors(ON|OFF)'");

// local file
static const regex MSG_STORE_ENABLE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'[[:digit:]]+ID\\.[[:digit:]]{3}' (opened)");
static const regex MSG_STORE_DISABLE_REGEX_STR("(ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'fsOFF')|(ID[[:digit:]]+: Closing msdFile '[[:digit:]]+ID\\.[[:digit:]]{3}')");
static const regex MSG_FLUSH_RATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'fsr(?:=[[:digit:]]+)*'=([[:digit:]]+)");

// battery monitor
static const regex VMON_ENABLE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'vm=[[:digit:]]'Toggling (?:0|1) to (1|0)");
static const regex VMON_LOW_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'vl(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex VMON_RESTART_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'vh(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex VMON_SLEEP_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'vs(?:=[[:digit:]]+)*'=([[:digit:]]+)");


// calibrations
static const regex ADC_CALS_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} ADChannels:[[:space:]]+"
                                      "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2}[[:blank:]]+(Vin i2c:[[:digit:]]{2}, ChMask=0x[[:digit:]]{4}, FSmask=0x[[:digit:]]{3},mV=[[:digit:]]{4}, Gain/Offset=[[:digit:]]+.[[:digit:]]+/[[:digit:]]+.[[:digit:]]+)[[:space:]]+"
                                      "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2}[[:blank:]]+(I3 i2c:[[:digit:]]{2}, ChMask=0x[[:digit:]]{4}, FSmask=0x[[:digit:]]{3},mV=[[:digit:]]{4}, Gain/Offset=[[:digit:]]+.[[:digit:]]+/[[:digit:]]+.[[:digit:]]+)[[:space:]]+"
                                      "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2}[[:blank:]]+(Iin i2c:[[:digit:]]{2}, ChMask=0x[[:digit:]]{4}, FSmask=0x[[:digit:]]{3},mV=[[:digit:]]{4}, Gain/Offset=[[:digit:]]+.[[:digit:]]+/[[:digit:]]+.[[:digit:]]+)[[:space:]]+");
static const regex VBG_CAL_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'vbg(?:=[[:digit:]]+\\.[[:digit:]]+)*'=([[:digit:]]+\\.[[:digit:]]+)");
static const regex IIG_CAL_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'iig(?:=[[:digit:]]*\\.[[:digit:]]+)*'=([[:digit:]]+\\.[[:digit:]]+)");
static const regex I3G_CAL_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'i3g(?:=[[:digit:]]*\\.[[:digit:]]+)*'=([[:digit:]]+\\.[[:digit:]]+)");

// eeprom
static const regex EECFG_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} EE=(Set) id([[:digit:]]+),pp=(sio|xb),md0,mp([0-2]),dr([[:digit:]]{1,5})s skips:sp([[:digit:]]{1,5}),sn([[:digit:]]{1,5}) fsr([[:digit:]]{1,5})s(?:\x03\x04)*[[:space:]]+"
                                   "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} batt: vm=(0|1),vl=([[:digit:]]{1,5}),vh=([[:digit:]]{1,5}),vs=([[:digit:]]{1,5})s(?:\x03\x04)*[[:space:]]+"
                                   "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} cals: vb=([[:digit:]]+\\.[[:digit:]]+) i3=([[:digit:]]+\\.[[:digit:]]+) iIn=([[:digit:]]+\\.[[:digit:]]+)(?:\x03\x04)*[[:space:]]+"
                                   "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} gps: gr=([[:digit:]]{1,5})s,gfr=([[:digit:]]{1,5})s,gnl=([[:digit:]]{1,5}),gto=([[:digit:]]{1,5})s,gmf=(0|1)(?:\x03\x04)*");
static const regex EEUPDATE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)EE Cfg Update: ([[:alpha:]]+)");
static const regex EEINIT_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)EE Cfg Initialize: ([[:alpha:]]+)");
static const regex EELOAD_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)[[:digit:]]{1,3}-[[:digit:]]{1,2}:[[:digit:]]{1,2}:[[:digit:]]{1,2} EE Cfg Load: ([[:alpha:]]+), nc=([[:digit:]]+)");

// GPS
static const regex GPS_ENABLE_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)GPS Power(ON|OFF) (?=Timeout=|CloseTimer).*(?:Open|Close)INT[[:digit:]]");
static const regex GPS_SYNC_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'gr(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex GPS_LCKTMOUT_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'gto(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex GPS_LCKFAIL_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'gfr(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex GPS_NLOCKS_CNFRM_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'gnl(?:=[[:digit:]]+)*'=([[:digit:]]+)");
static const regex GPS_SENDALL_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)'gmf'Toggling [[:digit:]] to ([[:digit:]])");

// Misc
static const regex LIST_COMMANDS_REGEX_STR("ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)Cmds: 'btradio' 'pp' 'mp' 'id' 'dr' 'sp' 'sn' 'fsON' 'fsOFF' 'fsDIR' 'fsr'[[:space:]]+"
                                           "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)Cmds: 'adcals' 'eecfg' 'eeinit' 'eeupdate' 'vm' 'vh' 'vl' 'vs' 'vbg' 'i3g'[[:space:]]+"
                                           "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)Cmds: 'iig' 'sensorsON' 'sensorsOFF' 'gpsON' 'gpsOFF' 'gr' 'gnl' 'gto' 'gfr'[[:space:]]+"
                                           "ID[[:digit:]]+:(?:\x01\x02|[[:blank:]]+)Cmds: 'gmf' 'scani2c' 'reset' 'reboot' '?'[[:space:]]*");


void WisardMote::initCmdTable()
{
    // Sampling Rate Cmds
    _commandTable[DATA_RATE_CMD] =          "dr\r";         // output data every 'val' seconds - can be very very large w/o declaring an error
    _cfgParameters[DATA_RATE_CMD] =         &_configMetaData._dataRateCfg;
    _commandTable[PWR_SAMP_RATE_CMD] =      "sp\r";         // output power data every 'val' * dr seconds
    _cfgParameters[PWR_SAMP_RATE_CMD] =     &_configMetaData._pwrSampCfg;
    _commandTable[SERNUM_RATE_CMD] =        "sn\r";         // output serial number data every 'val' * dr seconds
    _cfgParameters[SERNUM_RATE_CMD] =       &_configMetaData._serNumSampCfg;

    // Operating Mode Cmds
//    _commandTable[SAMP_MODE_CMD] =          "md\r";       // doesn't work???  select either self-timed, or timed on XBee wakeup
    _commandTable[NODE_ID_CMD] =            "id\r";         // set the node ID
    _cfgParameters[NODE_ID_CMD] =           &_configMetaData._idCfg;
    _commandTable[MSG_FMT_CMD] =            "mp\r";         // set the message format, Wisard binary, dsm printable (w/binary start/end chars, or ASCII
    _cfgParameters[MSG_FMT_CMD] =           &_configMetaData._msgFmtCfg;
    _commandTable[OUT_PORT_CMD] =           "pp\r";         // select either XBee or serial console
    _cfgParameters[OUT_PORT_CMD] =          &_configMetaData._portCfg;
    _commandTable[SENSORS_ON_CMD] =         "sensors\r";    // turn attached sensors ON/OFF
    _cfgParameters[SENSORS_ON_CMD] =        &_configMetaData._sensorsOnCfg;

    // Local file Cmds
    _commandTable[MSG_STORE_CMD] =          "fs\r";         // turn ON/OFF the local file system data storage
    _cfgParameters[MSG_STORE_CMD] =         &_configMetaData._fileEnableCfg;
    _commandTable[MSG_STORE_FLUSHRATE_CMD] =  "fsr\r";        // flush/cycle the local storage file every 'val' seconds
    _cfgParameters[MSG_STORE_FLUSHRATE_CMD] = &_configMetaData._fileFlushCfg;

    // Battery Monitor Cmds
    _commandTable[VMON_ENABLE_CMD] =        "vm\r";         // turn battery voltage monitoring ON/OFF
    _cfgParameters[VMON_ENABLE_CMD] =       &_configMetaData._vmonEnableCfg;
    _commandTable[VMON_LOW_CMD] =           "vl\r";         // turn Mote operation off at 'XXXX' volts, i.e. - 7000 == 7.000 V
    _cfgParameters[VMON_LOW_CMD] =          &_configMetaData._vmonLowCfg;
    _commandTable[VMON_RESTART_CMD] =       "vh\r";         // turn Mote operation on at 'XXXXX' volts, i.e. - 12300 == 12.3 V
    _cfgParameters[VMON_RESTART_CMD] =      &_configMetaData._vmonRestartCfg;
    _commandTable[VMON_SLEEP_CMD] =         "vs\r";         // after turning itself off, retest Vbatt every 'val' seconds
    _cfgParameters[VMON_SLEEP_CMD] =        &_configMetaData._vmonSleepCfg;

    // Calibration
    _commandTable[ADCALS_CMD] =            "adcals\r";     // report adc cal data
    _commandTable[VBG_CAL_CMD] =           "vbg\r";        // get/set gain for vbatt
    _cfgParameters[VBG_CAL_CMD] =           &_configMetaData._vbCalCfg;
    _commandTable[IIG_CAL_CMD] =           "iig\r";        // get/set gain for iIn
    _cfgParameters[IIG_CAL_CMD] =           &_configMetaData._iiCalCfg;
    _commandTable[I3G_CAL_CMD] =           "i3g\r";        // get/set gain for i3
    _cfgParameters[I3G_CAL_CMD] =           &_configMetaData._i3CalCfg;

    // EEPROM
    _commandTable[EE_CFG_CMD] =             "eecfg\r";      // report current operating settings stored in eeprom
    _commandTable[EE_UPDATE_CMD] =          "eeupdate\r";   // write eeprom w/settings in memory
    _commandTable[EE_INIT_CMD] =            "eeinit\r";     // write default settings to eeprom
    _commandTable[EE_LOAD_CMD] =            "eeload\r";     // reload eeprom settings into memory

//    // Xbee Radio Cmds
//    _commandTable[XB_AT_CMD] =              "xb\r";         // sends specific XBee command to the device
//    _commandTable[XB_RESET_TIMEOUT_CMD] =   "xr\r";         // auto reset of XBee in 'val' seconds unless heart beat (hb) is received
//    _commandTable[XB_STATUS_NOW_CMD] =      "xs\r";         // sends the current XBee status
//    _commandTable[XB_REBOOT_CMD] =          "rxb\r";        // reset the XBee immediately
//    _commandTable[XB_HEARTBEAT_MSG_CMD] =   "hb\r";         // ????? Is this a real command?
//    _commandTable[XB_RADIO_CMD] =           "xv\r";         // reprogram XBee device w/settings specified above

    // Bluetooth Radio
    _commandTable[BT_INTERACTIVE_CMD] =     "btradio\r";    // interactive mode to talk to bluetooth radio
    _commandTable[BT_CMD_MODE] =            "+++\r";        // put BT radio in command mode
    _commandTable[BT_GET_MACADDR] =         "atsi,1\r";     // get the BT MAC address
    _commandTable[BT_GET_NAME] =            "atsi,2\r";     // get the BT name
    _commandTable[BT_SET_NAME] =            "atsn,\r";      // set the BT name after the ','
    _commandTable[BT_GET_RFPWR] =           "atsi,14\r";    // get the BT output power
    _commandTable[BT_SET_RFPWR] =           "atspf,#,+/-\r";// set the BT output power: 5,- to 10+
    _commandTable[BT_SET_DATAMODE] =        "atmd\r";       // put BT in data mode
    _commandTable[BT_EXIT_BTRADIO] =        "\x03\r";       // exit btradio

    // GPS/Timing Cmds
    _commandTable[GPS_ENABLE_CMD] =         "gps\r";        // GPS ON/OFF
    _cfgParameters[GPS_ENABLE_CMD] =        &_configMetaData._gpsEnableCfg;
    _commandTable[GPS_SYNC_RATE_CMD] =      "gr\r";         // number of seconds between setting the RTCC from the GPS
    _cfgParameters[GPS_SYNC_RATE_CMD] =     &_configMetaData._gpsResyncCfg;
    _commandTable[GPS_LCKTMOUT_CMD] =       "gto\r";        // number of seconds to timeout if no lock acquired after power on
    _cfgParameters[GPS_LCKTMOUT_CMD] =      &_configMetaData._gpsTimeOutCfg;
    _commandTable[GPS_LCKFAIL_RETRY_CMD] =  "gfr\r";        // number of seconds to wait between lock retries
    _cfgParameters[GPS_LCKFAIL_RETRY_CMD] = &_configMetaData._gpsFailRetryCfg;
    _commandTable[GPS_NLOCKS_CNFRM_CMD] =   "gnl\r";        // number of sequential valid messages to confirm lock
    _cfgParameters[GPS_NLOCKS_CNFRM_CMD] =  &_configMetaData._gpsNumLocksCfg;
    _commandTable[GPS_SENDALL_MSGS_CMD] =   "gmf\r";        // toggles between 0 and 1
    _cfgParameters[GPS_SENDALL_MSGS_CMD] =  &_configMetaData._gpsMsgsCfg;

    // List commands, reset
	_commandTable[LIST_CMD] = 				"?\r";          // prints out a list of available commands
    _commandTable[RESET_CMD] =              "reset\r";      // soft reset
    _commandTable[REBOOT_CMD] =             "reboot\r";     // soft boot

	_commandTable[SENSOR_SRCH_CMD] = 		"scani2c\r";    // find sensors attached by i2c
}

void WisardMote::initScienceParams()
{
    // These are the default values. They can be changed from fromDOMElement()
    // They get packed into a vector so that there is some sense of ordering

    // GPS and message store to flash are typically not used
    updateScienceParameter(GPS_ENABLE_CMD, SensorCmdArg("OFF"));
    updateScienceParameter(MSG_STORE_CMD, SensorCmdArg("OFF"));
    updateScienceParameter(MSG_STORE_FLUSHRATE_CMD, SensorCmdArg(MSG_STORE_FLUSHRATE_DEFAULT));

    // Turn on all sensors
    updateScienceParameter(SENSORS_ON_CMD, SensorCmdArg("ON"));

    // Enable battery monitor
    updateScienceParameter(VMON_LOW_CMD, SensorCmdArg(VMON_LOW_DEFAULT));
    updateScienceParameter(VMON_RESTART_CMD, SensorCmdArg(VMON_HIGH_DEFAULT));
    updateScienceParameter(VMON_SLEEP_CMD, SensorCmdArg(VMON_SLEEP_TIME_DEFAULT));
    updateScienceParameter(VMON_ENABLE_CMD, SensorCmdArg(0));

    // NOTE: These following parameters actually get put into a separate attribute
    // called _epilogScienceParameter. These parameters are installed at the exit of
    // the configuration phase. This covers the case that fromDOMElement
    // may have added some changes to _scienceParameters.

    // Use MOTE binary msg format.
    updateScienceParameter(MSG_FMT_CMD, SensorCmdArg(MSG_FMT_DEFAULT));
    // Data rate enables the MOTE to send data on its own w/o prompting.
    updateScienceParameter(DATA_RATE_CMD, SensorCmdArg(DATA_RATE_DEFAULT));
    // update the eeprom w/the configs
    updateScienceParameter(EE_UPDATE_CMD);
}

void WisardMote::initPortCfgParams()
{
    // Let the SerialSensor base class know about WisardMote serial port limitations
    for (int i=0; i<NUM_PORT_TYPES; ++i) {
    	_portTypeList.push_back(SENSOR_PORT_TYPES[i]);
    }

    for (int i=0; i<NUM_SENSOR_BAUDS; ++i) {
    	_baudRateList.push_back(SENSOR_BAUDS[i]);
    }

    for (int i=0; i<NUM_SENSOR_WORD_SPECS; ++i) {
    	_serialWordSpecList.push_back(SENSOR_WORD_SPECS[i]);
    }
}

void WisardMote::fromDOMElement(const xercesc::DOMElement* node) throw(n_u::InvalidParameterException)
{
	try {
		SerialSensor::fromDOMElement(node);

	    XDOMElement xnode(node);

	    xercesc::DOMNode* child;
	    for (child = node->getFirstChild(); child != 0;
		    child=child->getNextSibling())
	    {
	        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE)
	            continue;
	        XDOMElement xchild((xercesc::DOMElement*) child);
	        const string& elname = xchild.getNodeName();

	        if (elname == "autoconfig") {
	            // get all the attributes of the node
	            xercesc::DOMNamedNodeMap *pAttributes = child->getAttributes();
	            int nSize = pAttributes->getLength();

	            for(int i=0; i<nSize; ++i) {
	                XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	                // get attribute name
	                const std::string& aname = attr.getName();
	                const std::string& aval = attr.getValue();

	                // xform everything to uppercase - this shouldn't affect numbers
	                string upperAval = aval;
	                std::transform(upperAval.begin(), upperAval.end(), upperAval.begin(), ::toupper);
	                DLOG(("WisardMote:fromDOMElement(): attribute: ") << aname << " : " << upperAval);
	                std::istringstream avalXformer;
	                int iArg = 0;
	                float fArg = 0.0;

	                // start with science parameters, assuming SerialSensor took care of any overrides to
	                // the default port config.
	                if (aname == "datarate") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> iArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(DATA_RATE_MIN, iArg, DATA_RATE_MAX)) {
                            updateScienceParameter(DATA_RATE_CMD, SensorCmdArg(iArg));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "pwrsamprate") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> iArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(PWR_SAMP_RATE_MIN, iArg, PWR_SAMP_RATE_MAX)) {
                            updateScienceParameter(PWR_SAMP_RATE_CMD, SensorCmdArg(iArg));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "snreportrate") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> iArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(SN_REPORT_RATE_MIN, iArg, SN_REPORT_RATE_MAX)) {
                            updateScienceParameter(SERNUM_RATE_CMD, SensorCmdArg(iArg));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
	                else if (aname == "nodeid") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(NODE_ID_MIN, iArg, NODE_ID_MAX)) {
	                		updateScienceParameter(NODE_ID_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "msgfmt") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(MSG_FMT_MIN, iArg, MSG_FMT_MAX)) {
	                		updateScienceParameter(MSG_FMT_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "outputport") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(OUT_PORT_MIN, iArg, OUT_PORT_MAX)) {
	                		updateScienceParameter(OUT_PORT_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
                    else if (aname == "enablesensors") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> iArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (upperAval == "ON" || upperAval == "TRUE" || upperAval == "OFF" || upperAval == "FALSE") {
                            updateScienceParameter(SENSORS_ON_CMD, SensorCmdArg("ON"));
                        }
                        else if (upperAval == "OFF" || upperAval == "FALSE") {
                            updateScienceParameter(SENSORS_ON_CMD, SensorCmdArg("OFF"));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "scansensors") {
                        updateScienceParameter(SENSOR_SRCH_CMD);
                    }
	                else if (aname == "msgstore") {
	                	if (upperAval == "ON" || upperAval == "TRUE") {
	                		updateScienceParameter(MSG_STORE_CMD, SensorCmdArg("ON"));
	                	}
	                	else if (upperAval == "OFF" || upperAval == "FALSE") {
	                		updateScienceParameter(MSG_STORE_CMD, SensorCmdArg("OFF"));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "msgstoreflush") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(MSG_STORE_FLUSHRATE_MIN, iArg, MSG_STORE_FLUSHRATE_MAX)) {
	                		updateScienceParameter(MSG_STORE_FLUSHRATE_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "battmon") {
                        if (upperAval == "ON" || upperAval == "TRUE" || upperAval == "ENABLE") {
                            updateScienceParameter(VMON_ENABLE_CMD, SensorCmdArg("ON"));
                        }
                        else if (upperAval == "OFF" || upperAval == "FALSE" || upperAval == "DISABLE" ) {
                                updateScienceParameter(VMON_ENABLE_CMD, SensorCmdArg("OFF"));
                            }
                            else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "battmonlo") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(VMON_LOW_MIN, iArg, VMON_LOW_MAX)) {
	                		updateScienceParameter(VMON_LOW_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "battmonhi") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(VMON_HIGH_MIN, iArg, VMON_HIGH_MAX)) {
	                		updateScienceParameter(VMON_RESTART_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "battmonsleep") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(VMON_SLEEP_TIME_MIN, iArg, VMON_SLEEP_TIME_MIN)) {
	                		updateScienceParameter(VMON_SLEEP_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
                    else if (aname == "vbattgaincal") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> fArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(VBATT_GAIN_CAL_MIN, fArg, VBATT_GAIN_CAL_MAX)) {
                            updateScienceParameter(VBG_CAL_CMD, SensorCmdArg(upperAval));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "i3gaincal") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> fArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(I3_GAIN_CAL_MIN, fArg, I3_GAIN_CAL_MAX)) {
                            updateScienceParameter(I3G_CAL_CMD, SensorCmdArg(upperAval));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "iingaincal") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> fArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(IIN_GAIN_CAL_MIN, fArg, IIN_GAIN_CAL_MAX)) {
                            updateScienceParameter(IIG_CAL_CMD, SensorCmdArg(upperAval));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
                    else if (aname == "gpssyncrate") {
                        avalXformer.clear();
                        avalXformer.str(upperAval);
                        try {
                            avalXformer >> iArg;
                        } catch (std::exception e) {
                            throw n_u::InvalidParameterException(
                                string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
                        }
                        if (RANGE_CHECK_INC(GPS_SYNC_RATE_MIN, iArg, GPS_SYNC_RATE_MAX)) {
                            updateScienceParameter(GPS_SYNC_RATE_CMD, SensorCmdArg(iArg));
                        }
                        else
                            throw n_u::InvalidParameterException(
                                    string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
                    }
	                else if (aname == "gpsenable") {
	                	if (upperAval == "ON" || upperAval == "TRUE" || upperAval == "ENABLE") {
	                		updateScienceParameter(	GPS_ENABLE_CMD, SensorCmdArg("ON"));
	                	}
	                	else if (upperAval == "OFF" || upperAval == "FALSE" || upperAval == "DISABLE") {
	                		updateScienceParameter(	GPS_ENABLE_CMD, SensorCmdArg("OFF"));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "gpslocks") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(GPS_REQ_LOCKS_MIN, iArg, GPS_REQ_LOCKS_MAX)) {
	                		updateScienceParameter(GPS_NLOCKS_CNFRM_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "gpsretry") {
	                	avalXformer.clear();
	                	avalXformer.str(upperAval);
	                	try {
	                		avalXformer >> iArg;
	                	} catch (std::exception e) {
							throw n_u::InvalidParameterException(
								string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval + " " + e.what());
	                	}
	                	if (RANGE_CHECK_INC(GPS_LCKFAIL_RETRY_MIN, iArg, GPS_LCKFAIL_RETRY_MAX)) {
	                		updateScienceParameter(GPS_LCKFAIL_RETRY_CMD, SensorCmdArg(iArg));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	                else if (aname == "gpsmsgs") {
	                	if (upperAval == "ALL" || upperAval == "ANY") {
	                		updateScienceParameter(	GPS_SENDALL_MSGS_CMD, SensorCmdArg(GPS_MSGS_ALL));
	                	}
	                	else if (upperAval == "LOCKED" || upperAval == "LOCK") {
	                		updateScienceParameter(	GPS_SENDALL_MSGS_CMD, SensorCmdArg(GPS_MSGS_LOCKED));
	                	}
	                    else
	                        throw n_u::InvalidParameterException(
									string("WisareMote::fromDOMElement(): ") + getName(), aname, upperAval);
	                }
	            }
	        }
	    }

	} catch (n_u::InvalidParameterException& e) {
		WLOG(("WisardMote::fromDOMElement(): SerialSensor::fromDOMElement() failed: ") << e.what());
	}
}

CFG_MODE_STATUS WisardMote::enterConfigMode()
{
    DLOG(("WisardMote::enterConfigMode()"));
    CFG_MODE_STATUS retVal = NOT_ENTERED;

    if (sendAndCheckSensorCmd(MSG_FMT_CMD, SensorCmdArg(2))) {
        if (sendAndCheckSensorCmd(DATA_RATE_CMD, SensorCmdArg(0))) {
            if (sendAndCheckSensorCmd(EE_UPDATE_CMD)) {
                if (sendAndCheckSensorCmd(RESET_CMD)) {
                    // Must only send EE_CFG_CMD after msg format is changed to ASCII...
                    if (sendAndCheckSensorCmd(EE_CFG_CMD)) {
                            retVal = ENTERED_RESP_CHECKED;
                    }
                }
            }
        }
    }
    return retVal;
}

void WisardMote::exitConfigMode()
{
    DLOG(("WisardMote::exitConfigMode()"));
    sendEpilogScienceParameters();
}

bool WisardMote::checkResponse()
{
    DLOG(("WisardMote::checkResponse()"));
	return sendAndCheckSensorCmd(EE_CFG_CMD);
}

void WisardMote::sendScienceParameters()
{
	DLOG(("WisardMote::sendScienceParameters()"));
	_scienceParametersOk = true;
    bool responseOk = false;

    for (size_t i=0; i<_scienceParameters.size(); ++i) {
	    // flush the serial port - read and write
	    serPortFlush(O_RDWR);
	    responseOk = sendAndCheckSensorCmd(static_cast<MOTE_CMDS>(_scienceParameters[i].cmd), _scienceParameters[i].arg);
		_scienceParametersOk = (_scienceParametersOk && responseOk);
	}
}

void WisardMote::sendEpilogScienceParameters()
{
    DLOG(("WisardMote::sendEpilogScienceParameters()"));
    bool responseOk = false;

    for (size_t i=0; i<_epilogScienceParameters.size(); ++i) {
        // flush the serial port - read and write
        serPortFlush(O_RDWR);
        responseOk = sendAndCheckSensorCmd(static_cast<MOTE_CMDS>(_epilogScienceParameters[i].cmd), _epilogScienceParameters[i].arg);
        _scienceParametersOk = (_scienceParametersOk && responseOk);
    }
}

bool WisardMote::checkScienceParameters()
{
	return _scienceParametersOk;
}

void WisardMote::updateScienceParameter(const MOTE_CMDS cmd, const SensorCmdArg& arg){
    ScienceParamVector* pParams = 0;
    if (cmd != DATA_RATE_CMD && cmd != EE_UPDATE_CMD && cmd != MSG_FMT_CMD) {
        pParams = &_scienceParameters;
    }
    else {
        pParams = &_epilogScienceParameters;
    }

    ScienceParamVector::iterator iter = pParams->begin();
    while (iter != pParams->end() && iter->cmd != cmd) {
        ++iter;
    }
    if (iter != pParams->end()) {
        iter->arg = arg;
    }
    else {
        pParams->push_back(SensorCmdData(cmd, arg));
    }
}

bool WisardMote::checkIfCmdNeeded(MOTE_CMDS cmd, SensorCmdArg arg)
{
    bool cmdNeeded = true;
    string argStr;
    string cmdStr = _commandTable[cmd];
    cmdStr.resize(cmdStr.find('\r'));

//    DLOG(("WisardMote::checkIfCmdNeeded(): cmd integer: %i", cmd));
    if (_cfgParameters.count(cmd)) {
        string& rCfgStr = *_cfgParameters[cmd];

        // check if same.
        if (!arg.argIsNull) {
            if (arg.argIsString) {
                argStr = arg.strArg;
                cmdNeeded = (rCfgStr != argStr);
            }
            else {
                std::ostringstream ostrm;
                ostrm << arg.intArg;
                argStr = ostrm.str();
                cmdNeeded = (rCfgStr != argStr);
            }
        }
        DLOG(("WisardMote::checkIfCmdNeeded(): cmd: %s is %s needed, as configuration: %s ==  arg: %s", cmdStr.c_str(), (cmdNeeded ? "" : "NOT"), rCfgStr.c_str(), argStr.c_str()));
    }
    else {
        DLOG(("WisardMote::checkIfCmdNeeded(): cmd: %s is needed, as it is not a part of the recorded configurations", cmdStr.c_str()));
    }

    return cmdNeeded;
}

void WisardMote::sendSensorCmd(MOTE_CMDS cmd, SensorCmdArg arg)
{
    CmdMap::iterator cmdIter = _commandTable.find(cmd);

    if (cmdIter != _commandTable.end()) {
        std::string cmdStr = cmdIter->second;
        if (!arg.argIsNull) {
            int insertIdx = cmdStr.find_first_of('\r');
            if (arg.argIsString) {
                cmdStr.insert(insertIdx, arg.strArg);
            }
            else {
                std::ostringstream argStr;
                argStr << arg.intArg;
                cmdStr.insert(insertIdx, "=" + argStr.str());
            }
        }

        serPortFlush(O_RDWR);

        DLOG(("WisardMote::sendSensorCmd() - sending command: ") << cmdStr);
        // write command out slowly
        for (unsigned int i = 0; i < cmdStr.length(); ++i) {
            write(&(cmdStr.c_str()[i]), 1);
            usleep(CHAR_WRITE_DELAY);
        }
    }

    else {
        std::ostringstream eString;
        eString << "WisardMote::sendSensorCmd(): unknown command index - " << cmd;
        throw InvalidParameterException(eString.str());
    }
}

bool WisardMote::checkCmdResponse(MOTE_CMDS cmd, SensorCmdArg arg)
{
	bool responseOK = false;
	bool checkMatch = true;
    static const int BUF_SIZE = 2048;
    int selectTimeout = 2000;
    if (cmd == RESET_CMD) {
        selectTimeout = 10000;
    }

    char respBuf[BUF_SIZE];
    memset(respBuf, 0, BUF_SIZE);
    int numCharsRead = readEntireResponse(respBuf, BUF_SIZE, selectTimeout);
    // regular expression specific to the cmd
    regex matchStr;
    // sub match to compare against
    int compareMatch = 0;
    // string composed of the sub match chars
    string valStr = "";
    // string composed of the primary match
    string resultsStr = "";

    if (numCharsRead) {
        char* buf = respBuf;
        if (cmd == RESET_CMD) {
            buf++;
        }
        DLOG(("WisardMote::checkCmdRepsonse(): Number of chars read - %i", numCharsRead));
        DLOG(("WisardMote::checkCmdRepsonse(): chars read - %s", buf));

		// get the matching regex
		switch (cmd) {
		    // Data Rates
            case DATA_RATE_CMD:
                matchStr = DATA_RATE_REGEX_STR;
                compareMatch = 1;
                break;
            case PWR_SAMP_RATE_CMD:
                matchStr = PWR_SMPRATE_REGEX_STR;
                compareMatch = 1;
                break;
            case SERNUM_RATE_CMD:
                matchStr = SERNUM_RATE_REGEX_STR;
                compareMatch = 1;
                break;

            // operating modes
            case NODE_ID_CMD:
                matchStr = NODEID_REGEX_STR;
                compareMatch = 1;
                break;
            case MSG_FMT_CMD:
                matchStr = MSG_FMT_REGEX_STR;
                compareMatch = 1;
                break;
            case OUT_PORT_CMD:
                matchStr = OUT_PORT_REGEX_STR;
                compareMatch = 1;
                break;
            case SENSORS_ON_CMD:
                matchStr = SENSORS_ON_REGEX_STR;
                compareMatch = 1;
                break;

            // local file store
            case MSG_STORE_CMD:
                if (arg.argIsString && arg.strArg == "ON") {
                    matchStr = MSG_STORE_ENABLE_REGEX_STR;
                }
                else {
                    matchStr = MSG_STORE_DISABLE_REGEX_STR;
                }
                break;
            case MSG_STORE_FLUSHRATE_CMD:
                matchStr = MSG_FLUSH_RATE_REGEX_STR;
                compareMatch = 1;
                break;

            // battery monitor
            case VMON_ENABLE_CMD:
                // May need to check this twice, since it's a toggle w/no ability to check first
                compareMatch = 1;
                responseOK = _checkSensorCmdResponse(cmd, arg, VMON_ENABLE_REGEX_STR, compareMatch, buf);
                if (!responseOK) {
                    sendSensorCmd(cmd, arg);
                    responseOK = _checkSensorCmdResponse(cmd, arg, VMON_ENABLE_REGEX_STR, compareMatch, buf);
                }
                checkMatch = false;
                break;
            case VMON_LOW_CMD:
                matchStr = VMON_LOW_REGEX_STR;
                compareMatch = 1;
                break;
            case VMON_RESTART_CMD:
                matchStr = VMON_RESTART_REGEX_STR;
                compareMatch = 1;
                break;
            case VMON_SLEEP_CMD:
                matchStr = VMON_SLEEP_REGEX_STR;
                compareMatch = 1;
                break;

            // calibrations
            case ADCALS_CMD:
                matchStr = ADC_CALS_REGEX_STR;
                break;
            case VBG_CAL_CMD:
                matchStr = VBG_CAL_REGEX_STR;
                compareMatch = 1;
                break;
            case IIG_CAL_CMD:
                matchStr = IIG_CAL_REGEX_STR;
                compareMatch = 1;
                break;
            case I3G_CAL_CMD:
                matchStr = I3G_CAL_REGEX_STR;
                compareMatch = 1;
                break;

            // eeprom
            case EE_CFG_CMD:
                responseOK = captureCfgData(respBuf);
                checkMatch = false;
                break;

            case EE_UPDATE_CMD:
                matchStr = EEUPDATE_REGEX_STR;
                compareMatch = 1;
                break;

            case EE_INIT_CMD:
                matchStr = EEINIT_REGEX_STR;
                compareMatch = 1;
                break;

            case EE_LOAD_CMD:
                matchStr = EELOAD_REGEX_STR;
                compareMatch = 1;
                break;

            // GPS
            case GPS_ENABLE_CMD:
                matchStr = GPS_ENABLE_REGEX_STR;
                compareMatch = 1;
                break;
            case GPS_SYNC_RATE_CMD:
                matchStr = GPS_SYNC_REGEX_STR;
                compareMatch = 1;
                break;
            case GPS_LCKTMOUT_CMD:
                matchStr = GPS_LCKTMOUT_REGEX_STR;
                compareMatch = 1;
                break;
            case GPS_LCKFAIL_RETRY_CMD:
                matchStr = GPS_LCKFAIL_REGEX_STR;
                compareMatch = 1;
                break;
            case GPS_NLOCKS_CNFRM_CMD:
                matchStr = GPS_NLOCKS_CNFRM_REGEX_STR;
                compareMatch = 1;
                break;
            case GPS_SENDALL_MSGS_CMD:
                matchStr = GPS_SENDALL_REGEX_STR;
                compareMatch = 1;
                break;
            // List, reset, reboot, misc
		    case LIST_CMD:
		        matchStr = LIST_COMMANDS_REGEX_STR;
		        break;
			case RESET_CMD:
			    responseOK = captureResetMetaData(buf);
			    checkMatch = false;
			    break;
			default:
				break;
		}

		if (checkMatch) {
		    responseOK = _checkSensorCmdResponse(cmd, arg, matchStr, compareMatch, respBuf);
		}
    }
	return responseOK;
}

bool WisardMote::_checkSensorCmdResponse(MOTE_CMDS cmd, SensorCmdArg arg, const regex& matchStr, int matchGroup, const char* buf)
{
    bool responseOK = false;
    // regular expression specific to the cmd
    // sub match to compare against
    // string composed of the sub match chars
    string valStr = "";
    // string composed of the primary match
    string resultsStr = "";

    DLOG(("WisardMote::checkVmonSensorCmdResponse(): matching: ") << matchStr);
    cmatch results;
    bool regexFound = regex_search(buf, results, matchStr);
    if (regexFound && results[0].matched) {
        resultsStr = std::string(results[0].first, (results[0].second - results[0].first));
        if (!arg.argIsNull) {
            if (cmd == MSG_STORE_CMD && (results[1].matched || results[2].matched)) {
                valStr = arg.strArg;
            }
            else if (results[matchGroup].matched) {
                if (matchGroup > 0) {
                    valStr = std::string(results[matchGroup].first, (results[matchGroup].second - results[matchGroup].first));

                    if (arg.argIsString) {
                        responseOK = (valStr == arg.strArg);
                    }
                    else {
                        std::ostringstream convertStream;
                        convertStream << arg.intArg;
                        responseOK = (valStr == convertStream.str());
                    }
                }
                else {
                    responseOK = true;
                }
            }
            else {
                DLOG(("WisardMote::checkCmdResponse(): Didn't find matches to argument as expected."));
            }
        }
        else {
            responseOK = true;
        }
    }
    else {
        DLOG(("WisardMote::checkCmdResponse(): Didn't find overall match to string as expected."));
    }

    updateCfgParam(cmd, valStr);

    string cmdStr = _commandTable[cmd];
    int idx = cmdStr.find('\r');
    cmdStr[idx] = 0;

    if (arg.argIsNull) {
            DLOG(("WisardMote::checkCmdResponse(): Results of checking command w/NULL argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("WisardMote::checkCmdResponse(): cmd: %s %s", cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED")));
        } else if (arg.argIsString) {
            DLOG(("WisardMote::checkCmdResponse(): Results of checking command w/string argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("WisardMote::checkCmdResponse(): cmd: %s: %s. expected: %s => saw: %s", cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED"), arg.strArg.c_str(), valStr.c_str()));
        }
        else {
            DLOG(("WisardMote::checkCmdResponse(): Results of checking command w/integer argument"));
            DLOG(("Overall match: ") << resultsStr);
            DLOG(("WisardMote::checkCmdResponse(): cmd: %s: %s. expected: %i => saw: %s", cmdStr.c_str(), (responseOK ? "SUCCEEDED" : "FAILED"), arg.intArg, valStr.c_str()));
    }

    return responseOK;
}

bool WisardMote::sendAndCheckSensorCmd(MOTE_CMDS cmd, SensorCmdArg arg)
{
    bool paramOK = true;

    if (checkIfCmdNeeded(cmd, arg)) {
        sendSensorCmd(cmd, arg);
        paramOK = checkCmdResponse(cmd, arg);
    }

    return paramOK;
}


void WisardMote::updateCfgParam(MOTE_CMDS cmd, std::string val)
{
    if (_cfgParameters.count(cmd)) {
        string cmdStr = _commandTable[cmd];
        cmdStr.resize(cmdStr.find('\r'));

        *_cfgParameters[cmd] = val;
        DLOG(("WisardMote::updateCfgParam(): cmd: %s, val: %s", cmdStr.c_str(), (*_cfgParameters[cmd]).c_str()));
    }
}

bool WisardMote::captureResetMetaData(const char* buf)
{
    bool responseOK = false;

    string modelIdStr;
    string resetSrcStr;
    string versionStr;
    string cpuSpeedStr;
    string timingSrcStr;
    string buildDateStr;
    string rtccCfgStr;
    string vinCalStr;
    string i3gCalStr;
    string iigCalStr;
    string tempSensorInitStr;
    string sensorSerialNumsStr;

    cmatch results;
    bool regexFound = regex_search(buf, results, MODEL_ID_REGEX_STR);
    bool matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        modelIdStr.append(results[1].first, results[1].second - results[1].first);
        setModel(modelIdStr);
        setManufacturer("UCAR/EOL");
    } else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the model ID string as expected."));
    }

    regexFound = regex_search(buf, results, RESET_SRC_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        resetSrcStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the reset source string as expected."));
    }

    regexFound = regex_search(buf, results, VERSION_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound) {
        if (results[1].matched) {
            versionStr.append(results[1].first, results[1].second - results[1].first);
            setFwVersion(versionStr);
        }
        else {
            responseOK = false;
            DLOG(("WisardMote::captureResetMetaData(): Didn't find version string match group as expected."));
        }
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find overall match to the version string as expected."));
    }

    regexFound = regex_search(buf, results, CPU_CLK_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        cpuSpeedStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the CPU speed string as expected."));
    }

    regexFound = regex_search(buf, results, TIMING_SRC_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        timingSrcStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the timing source string as expected."));
    }

    regexFound = regex_search(buf, results, BUILD_DATE_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        buildDateStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the build date string as expected."));
    }

    regexFound = regex_search(buf, results, RTCC_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        rtccCfgStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the RTCC config string as expected."));
    }

    regexFound = regex_search(buf, results, ADC_CALS_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound) {
        if (results[1].matched) {
            vinCalStr.append(results[1].first, results[1].second - results[1].first);
        }
        else {
            DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the Vin cal string as expected."));
        }
        if (results[2].matched) {
            i3gCalStr.append(results[2].first, results[2].second - results[2].first);
        }
        else {
            DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the Vin cal string as expected."));
        }
        if (results[3].matched) {
            iigCalStr.append(results[3].first, results[3].second - results[3].first);
        }
        else {
            DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the Vin cal string as expected."));
        }
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the ADC channel cal string as expected."));
    }

    regexFound = regex_search(buf, results, TEMP_SENSOR_INIT_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        tempSensorInitStr.append(results[1].first, results[1].second - results[1].first);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the temperature sensor config string as expected."));
    }

    regexFound = regex_search(buf, results, SENSOR_SERNUMS_REGEX_STR);
    matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (matchFound && results[1].matched) {
        sensorSerialNumsStr.append(results[1].first, results[1].second - results[1].first);
        setSerialNumber(sensorSerialNumsStr);
    }
    else {
        DLOG(("WisardMote::captureResetMetaData(): Didn't find matches to the sensor serial numbers string as expected."));
    }

    DLOG(("Mote: "));
    DLOG(("    Model ID:         ") << modelIdStr);
    DLOG(("    Reset Source:     ") << resetSrcStr);
    DLOG(("    SW Version:       ") << versionStr);
    DLOG(("    CPU Speed:        ") << cpuSpeedStr);
    DLOG(("    Timing Source:    ") << timingSrcStr);
    DLOG(("    Build Date:       ") << buildDateStr);
    DLOG(("    RTCC Cfg:         ") << rtccCfgStr);
    DLOG(("    Vin Cal:          ") << vinCalStr);
    DLOG(("    I3 Cal:           ") << i3gCalStr);
    DLOG(("    Iin Cal:          ") << iigCalStr);
    DLOG(("    Temp Sensor Init: ") << tempSensorInitStr);
    DLOG(("    Sensor Serial #s: ") << sensorSerialNumsStr);

    return responseOK;
}

bool WisardMote::captureCfgData(const char* buf)
{
    bool responseOK = false;

    cmatch results;
    bool regexFound = regex_search(buf, results, EECFG_REGEX_STR);
    bool matchFound = regexFound && results[0].matched;
    responseOK &= matchFound;
    if (!matchFound) {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the expected response of the eecfg command."));
        return responseOK;
    }

    if (results[1].matched) {
        _configMetaData._eeCfg.assign(results[1].first, results[1].second - results[1].first);
        responseOK = true;
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Eeprom status."));
    }

    responseOK &= results[2].matched;
    if (results[2].matched) {
        _configMetaData._idCfg.assign(results[2].first, results[2].second - results[2].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the sensor ID."));
    }

    responseOK &= results[3].matched;
    if (results[3].matched) {
        _configMetaData._portCfg.assign(results[3].first, results[3].second - results[3].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the output port."));
    }

    responseOK &= results[4].matched;
    if (results[4].matched) {
        _configMetaData._msgFmtCfg.assign(results[4].first, results[4].second - results[4].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the message format."));
    }

    responseOK &= results[5].matched;
    if (results[5].matched) {
        _configMetaData._dataRateCfg.assign(results[5].first, results[5].second - results[5].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the data rate."));
    }

    responseOK &= results[6].matched;
    if (results[6].matched) {
        _configMetaData._pwrSampCfg.assign(results[6].first, results[6].second - results[6].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the power sample rate."));
    }

    responseOK &= results[7].matched;
    if (results[7].matched) {
        _configMetaData._serNumSampCfg.assign(results[7].first, results[7].second - results[7].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the serial numbers sample rate."));
    }

    responseOK &= results[8].matched;
    if (results[8].matched) {
        _configMetaData._fileFlushCfg.assign(results[8].first, results[8].second - results[8].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the file flush rate."));
    }

    responseOK &= results[9].matched;
    if (results[9].matched) {
        _configMetaData._vmonEnableCfg.assign(results[9].first, results[9].second - results[9].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vmon enabled state."));
    }

    responseOK &= results[10].matched;
    if (results[10].matched) {
        _configMetaData._vmonLowCfg.assign(results[10].first, results[10].second - results[10].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vmon low volts."));
    }

    responseOK &= results[11].matched;
    if (results[11].matched) {
        _configMetaData._vmonRestartCfg.assign(results[11].first, results[11].second - results[11].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vmon restart voltage."));
    }

    responseOK &= results[12].matched;
    if (results[12].matched) {
        _configMetaData._vmonSleepCfg.assign(results[12].first, results[12].second - results[12].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vmon sleep recheck rate."));
    }

    responseOK &= results[13].matched;
    if (results[13].matched) {
        _configMetaData._vbCalCfg.assign(results[13].first, results[13].second - results[13].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vbat cal data."));
    }

    responseOK &= results[14].matched;
    if (results[14].matched) {
        _configMetaData._i3CalCfg.assign(results[14].first, results[14].second - results[14].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the i3g cal data."));
    }

    responseOK &= results[15].matched;
    if (results[15].matched) {
        _configMetaData._iiCalCfg.assign(results[15].first, results[15].second - results[15].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the Vmon sleep recheck rate."));
    }

    responseOK &= results[16].matched;
    if (results[16].matched) {
        _configMetaData._gpsResyncCfg.assign(results[16].first, results[16].second - results[16].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the GPS RTCC Resync rate data."));
    }

    responseOK &= results[17].matched;
    if (results[17].matched) {
        _configMetaData._gpsFailRetryCfg.assign(results[17].first, results[17].second - results[17].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the GPS RTCC Resync rate data."));
    }

    responseOK &= results[18].matched;
    if (results[18].matched) {
        _configMetaData._gpsNumLocksCfg.assign(results[18].first, results[18].second - results[18].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the GPS RTCC Resync rate data."));
    }

    responseOK &= results[19].matched;
    if (results[19].matched) {
        _configMetaData._gpsTimeOutCfg.assign(results[19].first, results[19].second - results[19].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the GPS RTCC Resync rate data."));
    }

    responseOK &= results[20].matched;
    if (results[20].matched) {
        _configMetaData._gpsMsgsCfg.assign(results[20].first, results[20].second - results[19].first);
    }
    else {
        DLOG(("WisardMote::captureCfgData(): Didn't find a match to the GPS RTCC Resync rate data."));
    }

    DLOG(("Mote Eeprom Cfg: ") << _configMetaData);
    return responseOK;
}
