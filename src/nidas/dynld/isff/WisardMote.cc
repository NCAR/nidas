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

std::map<unsigned char, string> WisardMote::_typeNames;

/* static */
const n_u::EndianConverter * WisardMote::_fromLittle =
	n_u::EndianConverter::getConverter(
			n_u::EndianConverter::EC_LITTLE_ENDIAN);

NIDAS_CREATOR_FUNCTION_NS(isff, WisardMote)

WisardMote::WisardMote() :
	_moteId(-1), _version(-1)
{
    setDuplicateIdOK(true);
	initFuncMap();
}
WisardMote::~WisardMote()
{
    clearMaps();
}

void WisardMote::open(int flags)
        throw(n_u::IOException,n_u::InvalidParameterException)

{
    clearMaps();
    DSMSerialSensor::open(flags);
}

void WisardMote::init()
        throw(n_u::InvalidParameterException)
{
    clearMaps();
    DSMSerialSensor::init();
}

void WisardMote::clearMaps()
{
    // clear sample tag maps we don't need anymore
    _sensorTypeToSampleId.clear();

    map<unsigned int,SampleTag*>::iterator si = _sampleTagsBySensorType.begin();
    for ( ; si != _sampleTagsBySensorType.end(); ++si) {
        SampleTag* stag = si->second;
        delete stag;
    }
    _sampleTagsBySensorType.clear();
}

bool WisardMote::process(const Sample * samp, list<const Sample *>&results) throw () {
	/* unpack a WisardMote packet, consisting of binary integer data from a variety
	 * of sensor types. */
	const unsigned char *cp =
		(const unsigned char *) samp->getConstVoidDataPtr();
	const unsigned char *eos = cp + samp->getDataByteLength();
	string ttag = n_u::UTime(samp->getTimeTag()).format(true, "%c");

	/*  check for good EOM  */
	if (!(eos = checkEOM(cp, eos)))
		return false;

	/*  verify crc for data  */
	if (!(eos = checkCRC(cp, eos, ttag)))
		return false;

	/*  read header */
	int mtype = readHead(cp, eos);
	if (_moteId < 0)
		return false; // invalid
	if (mtype == -1)
		return false; // invalid

	if (mtype != 1)
		return false; // other than a data message

	while (cp < eos) {

		/* get Wisard sensor type */
		unsigned char sensorType = *cp++;

		DLOG(("%s: moteId=%d, sensorid=%x, sensorType=%x, time=",
				getName().c_str(), _moteId, getSensorId(),sensorType) <<
				n_u::UTime(samp->getTimeTag()).format(true, "%c"));


		/* find the appropriate member function to unpack the data for this sensorType */
		readFunc func = _nnMap[sensorType];

		if (func == NULL) {
			if (!( _numBadSensorTypes[_moteId][sensorType]++ % 100))
				WLOG(("%s: moteId=%d: sensorType=%x, no data function. #times=%u",
						getName().c_str(), _moteId, sensorType,_numBadSensorTypes[_moteId][sensorType]));
			continue;
		}

		/* unpack the data for this sensorType */
		vector<float> data;
		cp = (this->*func)(cp, eos, samp->getTimeTag(),data);

		/* create an output floating point sample */
		if (data.size() == 0)
			continue;

                // sample id of processed sample
                unsigned int sid = getId() + (_moteId << 8) + sensorType;
                SampleTag* stag = _sampleTagsById[sid];

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
                            const Variable* var = vars[nv];
                            if (nv >= data.size() || *fp == var->getMissingValue()) *fp = floatNAN;
                            else if (*fp < var->getMinValue() || *fp > var->getMaxValue())
                                *fp = floatNAN;
                            else if (getApplyVariableConversions()) {
                                VariableConverter* conv = var->getConverter();
                                if (conv) *fp = conv->convert(samp->getTimeTag(),*fp);
                            }
                        }
                }
                else {
                        osamp = getSample<float> (data.size());
                        osamp->setId(sid);
                        std::copy(data.begin(), data.end(),osamp->getDataPtr());
                }
		osamp->setTimeTag(samp->getTimeTag());

#ifdef DEBUG
		for (unsigned int i = 0; i < data.size(); i++) {
			DLOG(("data[%d]=%f", i, data[i]));
		}
#endif

		/* push out */
		results.push_back(osamp);
	}
	return true;
}

void WisardMote::validate()
    throw (n_u::InvalidParameterException)
{
    list<SampleTag*> moteTags;
    const Parameter* motes = getParameter("motes");
    if (motes) {
        if (motes->getType() != Parameter::INT_PARAM)
            throw n_u::InvalidParameterException(getName(),"motes","should be integer type");
        for (int i = 0; i < motes->getLength(); i++) {
            unsigned int mote = (unsigned int) motes->getNumericValue(i);
            SampleTag* tag = new SampleTag();
	    tag->setDSMConfig(getDSMConfig());
	    tag->setDSMSensor(this);
            tag->setDSMId(getDSMId());
            tag->setSensorId(getSensorId());
            tag->setSampleId(mote << 8);
// #define DEBUG
#ifdef DEBUG
            cerr << getName() << ": mote=" << mote << " tag id=" << GET_DSM_ID(tag->getId()) << ',' <<
                    hex << GET_SPS_ID(tag->getId()) << dec << endl;
#endif
            addSampleTag(tag);
        }
    }

    list<SampleTag*> configTags = _sampleTags;
    _sampleTags.clear();
    list<SampleTag*>::iterator ti = configTags.begin();

    // cerr << getName() << ": configTags.size()=" << configTags.size() << endl;

    // loop over all sample tags, creating the ones we want
    // using the "motes" and "stypes" parameters.
    for ( ; ti != configTags.end(); ) {
        SampleTag* stag = *ti;
        removeSampleTag((const SampleTag*)stag);
        processSampleTag(stag);
        ti = configTags.erase(ti);
    }

    // cerr << "final getSampleTags().size()=" << getSampleTags().size() << endl;
    // cerr << "final _sampleTags.size()=" << _sampleTags.size() << endl;
    assert(_sampleTags.size() == getSampleTags().size());

#ifdef DEBUG
    const list<const SampleTag*>& tags = getSampleTags();
    list<const SampleTag*>::const_iterator ti2 = tags.begin();
    for ( ; ti2 != tags.end(); ++ti2 ) {
        const SampleTag* stag = *ti2;
        const vector<const Variable*>& vars = stag->getVariables();
        cerr << "wisard: " << GET_DSM_ID(stag->getId()) << ',' << hex << GET_SPS_ID(stag->getId()) << dec << ": " <<
                vars[0]->getName() << endl;
    }
#endif

    DSMSerialSensor::validate();
}

void WisardMote::processSampleTag(SampleTag* stag)
    throw (n_u::InvalidParameterException)
{
        // The sensor+sample id of stag, returned by getSpSId(), will
        // contain the sensor id (which by convention for base motes, is 0x8000),
        // and a mote id of 0 , 0x100 or 0x200, etc, up to 0x7f00).
        // The bottom 8 bits are the mote sensor type.
        //
        // The sensor type may be zero, in which case this stag is a sample
        // tag to provide information (like suffix) for all sensor types on a mote.
        //
        // If the bottom 8 bits are not zero, then stag should also contain
        // a Parameter, called stypes, specifing one or more sensor types
        // that the stag should be applied to.
        // stag can also contain a Parameter, "motes"
        // specifying the motes that the sample is for.
        //
        // If there is no motes parameter, and the mote bits of the stag sample
        // id are 0, then stag is to be used for samples of the sensor type
        // from all motes
        //
        // This is how variable names and conversions for samples from
        // a given sensor type can be set in the configuration, rather
        // than being set by hardcoded defaults in this class.

        unsigned int inid = stag->getId();

// #define DEBUG_DSM 1
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "inid=" << hex << GET_DSM_ID(inid) << ',' << GET_SPS_ID(inid) << dec << endl;
#endif

        // check if the sensor type field (low-order 8 bits) is non-zero
        if ((inid & 0x000000ff)) {
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "sid=" << hex << GET_DSM_ID(sid) << ',' << GET_SPS_ID(sid) << dec << endl;
#endif
            // dsm+sensor id
            unsigned int dsid =  (inid - stag->getSampleId());

            // mote portion of sample id, may be 0
            unsigned int mote = ((inid - dsid) & 0x00ff00) >> 8;

            const Parameter* motes = stag->getParameter("motes");
            int nmotes = 1;
            if (motes) {
                if (motes->getType() != Parameter::INT_PARAM)
                        throw n_u::InvalidParameterException(getName(),"sample parameter motes","should be integer type");
                nmotes = motes->getLength();
            }
            for (int j = 0; j < nmotes; j++) {
                if (motes)
                    mote = (unsigned int) motes->getNumericValue(j);
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "mote=" << mote << endl;
#endif
                mote <<= 8;
                unsigned int stype = inid & 0xff;
                // A sample id with non-zero bits in the first 8
                // overrides the hard-coded defaults for a sensor type id.
                // This sample applies to all sensor type ids in the "stypes" parameter.
                // inid is a full sample id (dsm,sensor,mote,sensor type), except
                // that mote may be 0 indicating it applies to all motes.
                const Parameter* stypes = stag->getParameter("stypes");
                if (stypes) {
                    if (stypes->getType() != Parameter::INT_PARAM)
                        throw n_u::InvalidParameterException(getName(),"stypes","should be integer type");
                    for (int i = 0; i < stypes->getLength(); i++) {
                        stype = (unsigned int) stypes->getNumericValue(i);
                        _sensorTypeToSampleId[dsid + mote + stype] = inid;
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "dsid+mote+stype=" << hex << (dsid + mote + stype) << dec << endl;
#endif
                    }
                }
                else {
                    _sensorTypeToSampleId[dsid + mote + stype] = inid;
#ifdef DEBUG_DSM
                    if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "dsid+mote+stype=" << hex << (dsid + mote + stype) << dec << endl;
#endif
                }
            }

            // delete previous
            if (_sampleTagsBySensorType[inid] != 0)
                delete _sampleTagsBySensorType[inid];
            _sampleTagsBySensorType[inid] = stag;
            return;
        }

        // inid has 0 for lowest 8 bits, this describes a mote.
        // Add all possible sample ids for this mote.
        // If the user has configured some samples with sensor type ids,
        // use them.
	for (int ist = 0;; ist++) {
		unsigned int stype = _samps[ist].id;  // 2 byte mote sensor type
		if (stype == 0)
			break;

                // sum of dsm, sensor, mote and sensor type id
                unsigned int fid = inid  + stype;

                // check if user has overridden this sample in the XML
                unsigned int cid = _sensorTypeToSampleId[fid];
                if (cid != 0) {
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "fid=" << hex << GET_DSM_ID(fid) << ',' << GET_SPS_ID(fid) << dec << endl;
#endif
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "cid1=" << hex << GET_DSM_ID(cid) << ',' << GET_SPS_ID(cid) << dec << endl;
#endif
                    // user specified a configured sample with a non-zero mote id
                    // cid is the sample id of the configured sample
                    // which will have a sensor id (0x8000), mote number and
                    // sensor type id
                    unsigned int id = inid + (cid & 0x00ff);
                    SampleTag* tag = _sampleTagsById[id];
                    if (tag) {
                        _sampleTagsById[fid] = tag;
                        continue;
                    }
                    assert(_sampleTagsBySensorType[cid]);
                    tag = buildSampleTag(stag,_sampleTagsBySensorType[cid]);

                    // in process method, look up the sample tag
                    // by dsm + sensor + mote + mote sensor type
                    _sampleTagsById[id] = tag;
                    if (fid != id) _sampleTagsById[fid] = tag;
                    addSampleTag(tag);
                    continue;
                }
                else {
                    // check for a configured sample with 0 for mote id
                    cid = _sensorTypeToSampleId[getId() + stype];
                    if (cid != 0) {
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "getId()+stype=" << hex << GET_DSM_ID(getId()+stype) << ',' << GET_SPS_ID(getId()+stype) << dec << endl;
#endif
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "cid2=" << hex << GET_DSM_ID(cid) << ',' << GET_SPS_ID(cid) << dec << endl;
#endif
                        // user specified a configured sample without a mote id
                        // cid is the sample id of the configured sample
                        // which will have a sensor id (0x8000), mote=0, and sensor type id
                        unsigned int cidWithMote = inid + (cid & 0xff);
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "cidWithMote=" << hex << GET_DSM_ID(cidWithMote) << ',' << GET_SPS_ID(cidWithMote) << dec << endl;
#endif
                        SampleTag* tag = _sampleTagsById[cidWithMote];
                        if (tag) {
#ifdef DEBUG_DSM
                            cerr << "got tag from _sampleTagsById[cidWithMote]" << endl;
#endif
                            _sampleTagsById[fid] = tag;
                            continue;
                        }
                        assert(_sampleTagsBySensorType[cid]);
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "buildSample, stag id=" <<
                        GET_DSM_ID(stag->getId()) << ',' << hex << GET_SPS_ID(stag->getId()) <<
                        ", tag2id=" << GET_DSM_ID(_sampleTagsBySensorType[cid]->getId()) <<
                        ',' << 
                            GET_SPS_ID(_sampleTagsBySensorType[cid]->getId()) << dec << endl;
#endif
                        tag = buildSampleTag(stag,_sampleTagsBySensorType[cid]);
                        _sampleTagsById[cidWithMote] = tag;
                        if (fid != cidWithMote) _sampleTagsById[fid] = tag;
#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "built tag=" << hex << GET_DSM_ID(tag->getId()) << ',' << GET_SPS_ID(tag->getId()) << dec << endl;
#endif
                        addSampleTag(tag);
                        continue;
                    }
                }
// #undef DEBUG_DSM

                // If user has overridden some variables, require match on all
                if (_sensorTypeToSampleId.size() > 0) continue;

                // build sample using hard-coded variable names
		SampleTag *newtag = new SampleTag(*stag);
		newtag->setSampleId(newtag->getSampleId() + stype);

                int mote = (inid - getId()) >> 8;

                ostringstream moteost;
                moteost << mote;
                string motestr = moteost.str();

#ifdef DEBUG_DSM
        if (GET_DSM_ID(inid) == DEBUG_DSM) cerr << "newtag=" << hex << GET_DSM_ID(newtag->getId()) << ',' << GET_SPS_ID(newtag->getId()) << dec << endl;
#endif
		int nv = sizeof(_samps[ist].variables) / sizeof(_samps[ist].variables[0]);

		//vars
		int len = 1;
		for (int iv = 0; iv < nv; iv++) {
			VarInfo vinf = _samps[ist].variables[iv];
			if (vinf.name == NULL)
				break;
			Variable *var = new Variable();
			var->setName(n_u::replaceChars(vinf.name,"%m",motestr));

			var->setUnits(vinf.units);
			var->setLongName(vinf.longname);
			var->setDynamic(vinf.dynamic);
			//      var->setDisplay(vinf.display);
			var->setLength(len);
			var->setSuffix(newtag->getSuffix());

			//ddd plot-range
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
		//add this new sample tag
		addSampleTag(newtag);
	}
	// delete old tag
	delete stag;
}

SampleTag* WisardMote::buildSampleTag(SampleTag * motetag, SampleTag* stag)
{
        // motetag is the tag containing only the mote information, no sample type
        SampleTag *newtag = new SampleTag(*motetag);

        newtag->setSensorId(motetag->getSensorId());
        newtag->setSampleId(motetag->getSampleId() + (stag->getId() & 0x00ff));

        int mote = (motetag->getId() - getId()) >> 8;

        // motestr = (ostringstream() << mote).str();

        ostringstream n;
        n << mote;
        string motestr = n.str();

        const vector<const Variable*>& vars = stag->getVariables();

        for (unsigned int i = 0; i < vars.size(); i++)
        {
                const Variable* var = vars[i];

                Variable *newvar = new Variable(*var);
                newvar->setDynamic(false);

                // replace %m in name with mote number
                newvar->setPrefix(n_u::replaceChars(newvar->getPrefix(),"%m",motestr));
                newvar->setSuffix(n_u::replaceChars(newtag->getSuffix(),"%m",motestr));

                if (newtag->getSite()) newvar->setSiteAttributes(newtag->getSite());
                newtag->addVariable(newvar);
        }
        return newtag;
}

/**
 * read mote id, version.
 * return msgType: -1=invalid header, 0 = sensortype+SN, 1=seq+time+data,  2=err msg
 */
int WisardMote::readHead(const unsigned char *&cp, const unsigned char *eos) {
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

	DLOG(("idstr=%s moteId=$i", idstr.c_str(), _moteId));

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
				ILOG(("%s: mote=%s, sensorType=%#x SN=%d, typeName=%s",
						getName().c_str(),idstr.c_str(), sensorType,
						serialNumber,_typeNames[sensorType].c_str()));
			}
		}
		break;
	case 1:
		/* unpack 1byte sequence */
		if (cp == eos)
			return false;
		_sequenceNumbersByMoteId[_moteId] = *cp++;
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d seq=%d",
				idstr.c_str(), _moteId, _version, mtype,
				_sequenceNumbersByMoteId[_moteId]));
		break;
	case 2:
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d ErrMsg=\"",
				idstr.c_str(), _moteId, _version,
				mtype) << string((const char *) cp, eos - cp) << "\"");
		break;
	default:
		DLOG(("Unknown msgType --- mote=%s, id=%d, Ver=%d MsgType=%d, msglen=", idstr.c_str(), _moteId, _version, mtype, eos - cp));
		break;
	}
	return mtype;
}

/*
 * Check EOM (0x03 0x04 0xd). Return pointer to start of EOM.
 */
const unsigned char *WisardMote::checkEOM(const unsigned char *sos,
		const unsigned char *eos) {

	if (eos - 4 < sos) {
		n_u::Logger::getInstance()->log(LOG_ERR,
				"Message length is too short --- len= %d", eos - sos);
		return 0;
	}
	// NIDAS will likely add a NULL to the end of the message. Check for that.
	if (*(eos - 1) == 0)
		eos--;
	eos -= 3;

	if (memcmp(eos, "\x03\x04\r", 3) != 0) {
		WLOG(("Bad EOM --- last 3 chars= %x %x %x", *(eos), *(eos + 1),
				*(eos + 2)));
		return 0;
	}
	return eos;
}

/*
 * Check CRC. Return pointer to CRC, which is one past the end of the data portion.
 */
const unsigned char *WisardMote::checkCRC(const unsigned char *cp,
		const unsigned char *eos, string ttag) {
	// Initial value of eos points to one past the CRC.
	if (eos-- <= cp) {
		WLOG(("Message length is too short --- len= %d", eos - cp));
		return 0;
	}

	// retrieve CRC at end of message.
	unsigned char crc = *eos;

	// Calculate Cksum. Start with length of message, not including checksum.
	unsigned char cksum = eos - cp;
	for (const unsigned char *cp2 = cp; cp2 < eos;)
		cksum ^= *cp2++;

	if (cksum != crc) {
		//skip the non-print bogus
		int bogusErr = 0;
		while (!isprint(*cp)) {
			cp++;
			bogusErr++;
		}
		//try once more time
		if (bogusErr > 0) {
			return checkCRC(cp, eos, ttag);
		}
		// Try to print out some header information.
		int mtype = readHead(cp, eos);
		if (!(_badCRCsByMoteId[_moteId]++ % 10)) {
			if (_moteId >= 0) {
				WLOG(("%s: %d bad CKSUMs for mote id %d, messsage type=%d, length=%d, ttag=%s, tx crc=%x, calc crc=%x", getName().c_str(), _badCRCsByMoteId[_moteId], _moteId, mtype, (eos - cp), ttag.c_str(),crc, cksum));
			} else {
				WLOG(("%s: %d bad CKSUMs for unknown mote, length=%d, tx crc=%x, calc crc=%x", getName().c_str(), _badCRCsByMoteId[_moteId], (eos - cp), crc, cksum));
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

/* type id 0x0B */
const unsigned char *WisardMote::readTmSec(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
        return readUint32(cp,eos,1,1.0,data);
}

/* type id 0x0C */
const unsigned char *WisardMote::readTmCnt(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
        return readUint32(cp,eos,1,1.0,data);
}

/* type id 0x0E */
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

/* type id 0x0D */
const unsigned char *WisardMote::readTm100thSec(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data) {
        return readUint32(cp,eos,1,0.01,data);
}

/* type id 0x0F */
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

/* type id 0x28-0x2B */
const unsigned char *WisardMote::readQsoilData(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
        return readUint16(cp,eos,1,0.01,data);
}

/* type id 0x2C-0x2F */
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
        return readUint16(cp,eos,5,1.0,data);
}

/* type id 0x34 -- 0x37  */
const unsigned char *WisardMote::readG4ChData(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
        return readUint16(cp,eos,4,1.0,data);
}

/* type id 0x38 -- ox3B  */
const unsigned char *WisardMote::readG1ChData(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
        return readUint16(cp,eos,5,1.0,data);
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

/* type id 0x54-0x5B */
const unsigned char *WisardMote::readRswData(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
        return readInt16(cp,eos,1,0.1,data);
}

/* type id 0x5C-0x63 */
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

/* type id 0x64-0x6B */
const unsigned char *WisardMote::readRlwKZData(const unsigned char *cp,
		const unsigned char *eos, dsm_time_t ttag, vector<float>& data)
{
        cp = readInt16(cp,eos,2,1.0,data);
        data[0] /= 10.0; // Rpile
        data[1] /= 100.0; // Tcase
	return cp;
}

/* type id 0x6C-0x6F */
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

		_nnMap[0x0B] = &WisardMote::readTmSec;
		_nnMap[0x0C] = &WisardMote::readTmCnt;
		_nnMap[0x0D] = &WisardMote::readTm100thSec;
		_nnMap[0x0E] = &WisardMote::readTm10thSec;
		_nnMap[0x0F] = &WisardMote::readPicDT;

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
		_nnMap[0x2A] = &WisardMote::readQsoilData;
		_nnMap[0x2B] = &WisardMote::readQsoilData;

		_nnMap[0x2C] = &WisardMote::readTP01Data;
		_nnMap[0x2D] = &WisardMote::readTP01Data;
		_nnMap[0x2E] = &WisardMote::readTP01Data;
		_nnMap[0x2F] = &WisardMote::readTP01Data;

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
		_nnMap[0x3A] = &WisardMote::readG1ChData;
		_nnMap[0x3B] = &WisardMote::readG1ChData;

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
		_nnMap[0x5A] = &WisardMote::readRswData;
		_nnMap[0x5B] = &WisardMote::readRswData;

		_nnMap[0x5C] = &WisardMote::readRlwData;
		_nnMap[0x5D] = &WisardMote::readRlwData;
		_nnMap[0x5E] = &WisardMote::readRlwData;
		_nnMap[0x5F] = &WisardMote::readRlwData;

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
		_nnMap[0x6A] = &WisardMote::readRlwKZData;
		_nnMap[0x6B] = &WisardMote::readRlwKZData;

		_nnMap[0x6C] = &WisardMote::readCNR2Data;
		_nnMap[0x6D] = &WisardMote::readCNR2Data;
		_nnMap[0x6E] = &WisardMote::readCNR2Data;
		_nnMap[0x6F] = &WisardMote::readCNR2Data;

		_nnMap[0x70] = &WisardMote::readRswData2;
		_nnMap[0x71] = &WisardMote::readRswData2;
		_nnMap[0x72] = &WisardMote::readRswData2;
		_nnMap[0x73] = &WisardMote::readRswData2;

		_typeNames[0x01] = "PicTm";
		_typeNames[0x04] = "GenShort";
		_typeNames[0x05] = "GenLong";

		_typeNames[0x0B] = "TmSec";
		_typeNames[0x0C] = "TmCnt";
		_typeNames[0x0D] = "Tm100thSec";
		_typeNames[0x0E] = "Tm10thSec";
		_typeNames[0x0F] = "PicDT";

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
		_typeNames[0x2A] = "Qsoil";
		_typeNames[0x2B] = "Qsoil";

		_typeNames[0x2C] = "TP01";
		_typeNames[0x2D] = "TP01";
		_typeNames[0x2E] = "TP01";
		_typeNames[0x2F] = "TP01";

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
		_typeNames[0x3A] = "G1CH";
		_typeNames[0x3B] = "G1CH";

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
		_typeNames[0x5A] = "Downlooking Pyranometer (Rsw.out)";
		_typeNames[0x5B] = "Downlooking Pyranometer (Rsw.out)";

		_typeNames[0x5C] = "Uplooking Epply Pyrgeometer (Rlw.in)";
		_typeNames[0x5D] = "Uplooking Epply Pyrgeometer (Rlw.in)";
		_typeNames[0x5E] = "Uplooking Epply Pyrgeometer (Rlw.in)";
		_typeNames[0x5F] = "Uplooking Epply Pyrgeometer (Rlw.in)";

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
		_typeNames[0x6A] = "Downlooking K&Z Pyrgeometer (Rlw.out)";
		_typeNames[0x6B] = "Downlooking K&Z Pyrgeometer (Rlw.out)";

		_typeNames[0x6C] = "CNR2 Net Radiometer";
		_typeNames[0x6D] = "CNR2 Net Radiometer";
		_typeNames[0x6E] = "CNR2 Net Radiometer";
		_typeNames[0x6F] = "CNR2 Net Radiometer";

		_typeNames[0x70] = "Diffuse shortwave";
		_typeNames[0x71] = "Diffuse shortwave";
		_typeNames[0x72] = "Diffuse shortwave";
		_typeNames[0x73] = "Diffuse shortwave";
		_functionsMapped = true;
	}
}

//  %m in the variable names below will be replaced by the decimal mote number
SampInfo WisardMote::_samps[] = {
		{ 0x0E, { { "Tdiff.m%m", "secs","Time difference, adam-mote", "$ALL_DEFAULT", true },
			{ "Tdiff2.m%m", "secs", "Time difference, adam-mote-first_diff", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x20, {
                        {"Tsoil.0.6cm.a_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.1.9cm.a_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.3.1cm.a_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.4.4cm.a_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x21, {
                        {"Tsoil.0.6cm.b_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.1.9cm.b_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.3.1cm.b_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.4.4cm.b_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x22, {
                        {"Tsoil.0.6cm.c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.1.9cm.c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.3.1cm.c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.4.4cm.c_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x23, {
                        {"Tsoil.0.6cm.d_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.1.9cm.d_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.3.1cm.d_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{"Tsoil.4.4cm.d_m%m", "degC", "Soil Temperature", "$TSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x24, {
                        { "Gsoil.a_m%m", "W/m^2", "Soil Heat Flux", "$GSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x25, {
                        { "Gsoil.b_m%m", "W/m^2", "Soil Heat Flux", "$GSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x26, {
                        { "Gsoil.c_m%m", "W/m^2", "Soil Heat Flux", "$GSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x27, {
                        { "Gsoil.d_m%m", "W/m^2", "Soil Heat Flux", "$GSOIL_RANGE",true },
			{ 0, 0, 0, 0, true } } },
		{ 0x28, {
                        { "Qsoil.a_m%m", "vol%", "Soil Moisture", "$QSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x29, {
                        { "Qsoil.b_m%m", "vol%", "Soil Moisture", "$QSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2A, {
                        { "Qsoil.c_m%m", "vol%", "Soil Moisture", "$QSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2B, {
                        { "Qsoil.d_m%m", "vol%", "Soil Moisture", "$QSOIL_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2C, {
                        { "Vheat.a_m%m", "V", "TP01 heater voltage", "$VHEAT_RANGE", true },
			{ "Vpile.on.a_m%m", "microV", "TP01 thermopile after heating", "$VPILE_RANGE", true },
			{ "Vpile.off.a_m%m", "microV", "TP01 thermopile before heating", "$VPILE_RANGE", true },
			{ "Tau63.a_m%m", "secs", "TP01 time to decay to 37% of Vpile.on-Vpile.off", "$TAU63_RANGE", true },
			{ "lambdasoil.a_m%m", "W/mDegk", "TP01 derived thermal conductivity", "$LAMBDA_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2D, {
                        { "Vheat.b_m%m", "V", "TP01 heater voltage", "$VHEAT_RANGE", true },
			{ "Vpile.on.b_m%m", "microV", "TP01 thermopile after heating", "$VPILE_RANGE", true },
			{ "Vpile.off.b_m%m", "microV", "TP01 thermopile before heating", "$VPILE_RANGE", true },
			{ "Tau63.b_m%m", "secs", "TP01 time to decay to 37% of Vpile.on-Vpile.off", "$TAU63_RANGE", true },
			{ "lambdasoil.b_m%m", "W/mDegk", "TP01 derived thermal conductivity", "$LAMBDA_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2E, {
                        { "Vheat.c_m%m", "V", "TP01 heater voltage", "$VHEAT_RANGE", true },
			{ "Vpile.on.c_m%m", "microV", "TP01 thermopile after heating", "$VPILE_RANGE", true },
			{ "Vpile.off.c_m%m", "microV", "TP01 thermopile before heating", "$VPILE_RANGE", true },
			{ "Tau63.c_m%m", "secs", "TP01 time to decay to 37% of Vpile.on-Vpile.off", "$TAU63_RANGE", true },
			{ "lambdasoil.c_m%m", "W/mDegk", "TP01 derived thermal conductivity", "$LAMBDA_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x2F, {
                        { "Vheat.d_m%m", "V", "TP01 heater voltage", "$VHEAT_RANGE", true },
			{ "Vpile.on.d_m%m", "microV", "TP01 thermopile after heating", "$VPILE_RANGE", true },
			{ "Vpile.off.d_m%m", "microV", "TP01 thermopile before heating", "$VPILE_RANGE", true },
			{ "Tau63.d_m%m", "secs", "TP01 time to decay to 37% of Vpile.on-Vpile.off", "$TAU63_RANGE", true },
			{ "lambdasoil.d_m%m", "W/mDegk", "TP01 derived thermal conductivity", "$LAMBDA_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x30, {
                        { "G5_c1.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c2.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c3.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c4.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c5.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x31, {
                        { "G5_c1.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c2.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c3.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c4.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c5.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x32, {   
                        { "G5_c1.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c2.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c3.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c4.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c5.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x33, {
                        { "G5_c1.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c2.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c3.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c4.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G5_c5.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x34, {
                        { "G4_c1.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c2.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c3.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c4.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x35, {
                        { "G4_c1.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c2.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c3.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c4.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x36, {
                        { "G4_c1.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c2.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c3.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c4.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x37, {
                        { "G4_c1.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c2.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c3.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ "G4_c4.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x38, {
                        { "G1_c1.a_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x39, {
                        { "G1_c1.b_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x3A, {
                        { "G1_c1.c_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x3B, {
                        { "G1_c1.d_m%m", "",	"", "$ALL_DEFAULT", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x49, {
                        { "Vin.m%m", "V", "Supply voltage", "$VIN_RANGE", true },
                        { "Iin.m%m", "A", "Supply current", "$IIN_RANGE", true },
                        { "I33.m%m", "A", "3.3 V current", "$IIN_RANGE", true },
                        { "Isensors.m%m", "A", "Sensor current", "$IIN_RANGE", true },
			{0, 0, 0, 0, true } } },
                { 0x50, { { "Rnet.a_m%m", "W/m^2", "Net Radiation", "$RNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
                { 0x51, { { "Rnet.b_m%m", "W/m^2", "Net Radiation", "$RNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x52, { { "Rnet.c_m%m", "W/m^2", "Net Radiation", "$RNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x53, { { "Rnet.d_m%m", "W/m^2", "Net Radiation", "$RNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x54, { { "Rsw.in.a_m%m", "W/m^2", "Incoming Short Wave", "$RSWIN_RANGE",	true },
			{ 0, 0, 0, 0, true } } },
		{ 0x55, { { "Rsw.in.b_m%m",	"W/m^2", "Incoming Short Wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x56, { { "Rsw.in.c_m%m", "W/m^2", "Incoming Short Wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x57, { { "Rsw.in.d_m%m", "W/m^2", "Incoming Short Wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x58, { { "Rsw.out.a_m%m", "W/m^2", "Outgoing Short Wave", "$RSWOUT_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x59, { { "Rsw.out.b_m%m", "W/m^2", "Outgoing Short Wave", "$RSWOUT_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5A, { { "Rsw.out.c_m%m", "W/m^2", "Outgoing Short Wave", "$RSWOUT_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5B, { { "Rsw.out.d_m%m", "W/m^2", "Outgoing Short Wave", "$RSWOUT_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5C, { { "Rpile.in.a_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.a_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE", true },
			{ "Tdome1.in.a_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE", true },
			{ "Tdome2.in.a_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE", true },
			{ "Tdome3.in.a_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5D, { { "Rpile.in.b_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.b_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE", true },
			{ "Tdome1.in.b_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE", true },
			{ "Tdome2.in.b_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE", true },
			{ "Tdome3.in.b_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5E, { { "Rpile.in.c_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.c_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE", true },
			{ "Tdome1.in.c_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE", true },
			{ "Tdome2.in.c_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE", true },
			{ "Tdome3.in.c_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x5F, { { "Rpile.in.d_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.d_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE", true },
			{ "Tdome1.in.d_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE", true },
			{ "Tdome2.in.d_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE", true },
			{ "Tdome3.in.d_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x60, { { "Rpile.out.a_m%m", "W/m^2", "Epply pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.a_m%m", "degC", "Epply case temperature, outgoing", "$TCASE_RANGE", true },
			{ "Tdome1.out.a_m%m", "degC", "Epply dome temperature #1, outgoing", "$TDOME_RANGE", true },
			{ "Tdome2.out.a_m%m", "degC", "Epply dome temperature #2, outgoing", "$TDOME_RANGE", true },
			{ "Tdome3.out.a_m%m", "degC", "Epply dome temperature #3, outgoing", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x61, { { "Rpile.out.b_m%m", "W/m^2", "Epply pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.b_m%m", "degC", "Epply case temperature, outgoing", "$TCASE_RANGE", true },
			{ "Tdome1.out.b_m%m", "degC", "Epply dome temperature #1, outgoing", "$TDOME_RANGE", true },
			{ "Tdome2.out.b_m%m", "degC", "Epply dome temperature #2, outgoing", "$TDOME_RANGE", true },
			{ "Tdome3.out.b_m%m", "degC", "Epply dome temperature #3, outgoing", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x62, { { "Rpile.out.c_m%m", "W/m^2", "Epply pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.c_m%m", "degC", "Epply case temperature, outgoing", "$TCASE_RANGE", true },
			{ "Tdome1.out.c_m%m", "degC", "Epply dome temperature #1, outgoing", "$TDOME_RANGE", true },
			{ "Tdome2.out.c_m%m", "degC", "Epply dome temperature #2, outgoing", "$TDOME_RANGE", true },
			{ "Tdome3.out.c_m%m", "degC", "Epply dome temperature #3, outgoing", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x63, { { "Rpile.out.d_m%m", "W/m^2", "Epply pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.out.d_m%m", "degC", "Epply case temperature, incoming", "$TCASE_RANGE", true },
			{ "Tdome1.out.d_m%m", "degC", "Epply dome temperature #1, incoming", "$TDOME_RANGE", true },
			{ "Tdome2.out.d_m%m", "degC", "Epply dome temperature #2, incoming", "$TDOME_RANGE", true },
			{ "Tdome3.out.d_m%m", "degC", "Epply dome temperature #3, incoming", "$TDOME_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x64, { { "Rpile.in.akz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.akz_m%m", "degC", "K&Z case temperature, incoming", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x65, { { "Rpile.in.bkz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.bkz_m%m", "degC", "K&Z case temperature, incoming", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x66, { { "Rpile.in.ckz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.ckz_m%m", "degC", "K&Z case temperature, incoming", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x67, { { "Rpile.in.dkz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, incoming", "$RPILE_RANGE", true },
			{ "Tcase.in.dkz_m%m", "degC", "K&Z case temperature, incoming", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x68, { { "Rpile.out.akz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.akz_m%m", "degC", "K&Z case temperature, outgoing", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x69, { { "Rpile.out.bkz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.bkz_m%m", "degC", "K&Z case temperature, outgoing", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6A, { { "Rpile.out.ckz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.ckz_m%m", "degC", "K&Z case temperature, outgoing", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6B, { { "Rpile.out.dkz_m%m", "W/m^2", "K&Z pyrgeometer thermopile, outgoing", "$RPILE_RANGE", true },
			{ "Tcase.out.dkz_m%m", "degC", "K&Z case temperature, outgoing", "$TCASE_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6C, { { "Rsw.net.a_m%m", "W/m^2", "CNR2 net short-wave radiation", "$RSWNET_RANGE", true },
			{ "Rlw.net.a_m%m", "W/m^2", "CNR2 net long-wave radiation", "$RLWNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6D, { { "Rsw.net.b_m%m", "W/m^2", "CNR2 net short-wave radiation", "$RSWNET_RANGE", true },
			{ "Rlw.net.b_m%m", "W/m^2", "CNR2 net long-wave radiation", "$RLWNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6E, { { "Rsw.net.c_m%m", "W/m^2", "CNR2 net short-wave radiation", "$RSWNET_RANGE", true },
			{ "Rlw.net.c_m%m", "W/m^2", "CNR2 net long-wave radiation", "$RLWNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x6F, { { "Rsw.net.d_m%m", "W/m^2", "CNR2 net short-wave radiation", "$RSWNET_RANGE", true },
			{ "Rlw.net.d_m%m", "W/m^2", "CNR2 net long-wave radiation", "$RLWNET_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x70, { { "Rsw.dfs.a_m%m", "W/m^2", "Diffuse short wave", "$RSWIN_RANGE", true },
			{ "Rsw.direct.a_m%m", "W/m^2", "Direct short wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x71, { { "Rsw.dfs.b_m%m", "W/m^2", "Diffuse short wave", "$RSWIN_RANGE", true },
			{ "Rsw.direct.b_m%m", "W/m^2", "Direct short wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x72, { { "Rsw.dfs.c_m%m", "W/m^2", "Diffuse short wave", "$RSWIN_RANGE", true },
			{ "Rsw.direct.c_m%m", "W/m^2", "Direct short wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0x73, { { "Rsw.dfs.d_m%m", "W/m^2", "Diffuse short wave", "$RSWIN_RANGE", true },
			{ "Rsw.direct.d_m%m", "W/m^2", "Direct short wave", "$RSWIN_RANGE", true },
			{ 0, 0, 0, 0, true } } },
		{ 0, { { }, } },
};
