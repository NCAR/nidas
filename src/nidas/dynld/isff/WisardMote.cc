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
#include <nidas/core/DSMTime.h>
#include <nidas/core/DSMConfig.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <memory> // auto_ptr<>

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
const n_u::EndianConverter* WisardMote::_fromLittle =
	n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);


NIDAS_CREATOR_FUNCTION_NS(isff,WisardMote)


WisardMote::WisardMote():
	_moteId(-1),_version(-1)
	{
	initFuncMap();
	}

bool WisardMote::process(const Sample* samp,list<const Sample*>& results) throw()
{
	/* unpack a WisardMote packet, consisting of binary integer data from a variety
	 * of sensor types. */
	const unsigned char* cp= (const unsigned char*) samp->getConstVoidDataPtr();
	const unsigned char* eos = cp + samp->getDataByteLength();

	/*  check for good EOM  */
	if (!(eos = checkEOM(cp,eos))) return false;

	/*  verify crc for data  */
	if (!(eos = checkCRC(cp,eos))) return false;

	/*  read header */
	int mtype = readHead(cp, eos);
	if (_moteId < 0) return false;  // invalid
	if (mtype == -1) return false;  // invalid

	if (mtype != 1) return false;   // other than a data message

	while (cp < eos) {

		/* get sensor type id    */
		unsigned char sensorTypeId = *cp++;

		DLOG(("%s: moteId=%d, sensorid=%x, sensorTypeId=%x, time=",
				getName().c_str(),_moteId, getSensorId(), sensorTypeId) <<
				n_u::UTime(samp->getTimeTag()).format(true,"%c"));

		_data.clear();

		/* find the appropriate member function to unpack the data for this sensorTypeId */
		readFunc func = _nnMap[sensorTypeId];
		if (func == NULL) {
			WLOG(("%s: moteId=%d: no read data function for sensorTypeId=%x",
					getName().c_str(),_moteId, sensorTypeId));
			continue;
		}

		/* unpack the data for this sensorTypeId */
		cp = (this->*func)(cp,eos,samp->getTimeTag());

		/* create an output floating point sample */
		if (_data.size() == 0) 	continue;

		SampleT<float>* osamp = getSample<float>(_data.size());
		osamp->setTimeTag(samp->getTimeTag());
		osamp->setId(getId()+(_moteId << 8) + sensorTypeId);
		float* dout = osamp->getDataPtr();

		std::copy(_data.begin(),_data.end(),dout);
#ifdef DEBUG
		for (unsigned int i=0; i<_data.size(); i++) {
			DLOG(("data[%d]=%f",i, _data[i]));
		}
#endif
		/* push out */
		results.push_back(osamp);
	}
	return true;
}

void WisardMote::addSampleTag(SampleTag* stag) throw(n_u::InvalidParameterException) {
	for (int i = 0; ; i++)
	{
		unsigned int id = _samps[i].id;
		if (id == 0) break;

		SampleTag* newtag = new SampleTag(*stag);
		newtag->setSampleId(newtag->getSampleId()+id);
		int nv = sizeof(_samps[i].variables)/sizeof(_samps[i].variables[0]);

		//vars
		int len=1;
		for (int j = 0; j < nv; j++) {
			VarInfo vinf = _samps[i].variables[j];
			if (!vinf.name || sizeof(vinf.name)<=0 ) break;
			Variable* var = new Variable();
			var->setName(vinf.name);
			var->setUnits(vinf.units);
			var->setLongName(vinf.longname);
			var->setDynamic(vinf.dynamic);
			var->setLength(len);
			var->setSuffix(newtag->getSuffix());

			newtag->addVariable(var);
		}
		//add this new sample tag
		DSMSerialSensor::addSampleTag(newtag);
	}
	//delete old tag
	delete stag;
}

/**
 * read mote id, version.
 * return msgType: -1=invalid header, 0 = sensortype+SN, 1=seq+time+data,  2=err msg
 */
int WisardMote::readHead(const unsigned char* &cp, const unsigned char* eos)
{
	_moteId = -1;

	/* look for mote id. First skip non-digits. */
	for ( ; cp < eos; cp++) if (::isdigit(*cp)) break;
	if (cp == eos) return -1;

	const unsigned char* colon = (const unsigned char*)::memchr(cp,':',eos-cp);
	if (!colon) return -1;

	// read the moteId
	string idstr((const char*)cp,colon-cp);
	{
		stringstream ssid(idstr);
		ssid >> std::dec >> _moteId;
		if (ssid.fail()) return -1;
	}

	DLOG(("idstr=%s moteId=$i", idstr.c_str(), _moteId));

	cp = colon + 1;

	// version number
	if (cp == eos) return -1;
	_version = *cp++;

	// message type
	if (cp == eos) return -1;
	int mtype = *cp++;

	switch(mtype) {
	case 0:
		/* unpack 1 bytesId + 1 byte s/n */
		if (cp + 1 >= eos) return false;
		{
			int sensorTypeId = *cp++;
			int serialNumber = *cp++;
			_sensorSerialNumbersByMoteIdAndType[_moteId][sensorTypeId] = serialNumber;
			DLOG(("mote=%s, id=%d, ver=%d MsgType=%d sensorTypeId=%d SN=%d",
					idstr.c_str(),_moteId,_version, mtype, sensorTypeId, serialNumber));
		}
		break;
	case 1:
		/* unpack 1byte sequence */
		if (cp == eos) return false;
		_sequenceNumbersByMoteId[_moteId] = *cp++;
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d seq=%d",
				idstr.c_str(), _moteId, _version, mtype, _sequenceNumbersByMoteId[_moteId]));
		break;
	case 2:
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d ErrMsg=\"",
				idstr.c_str(), _moteId, _version, mtype) << string((const char*)cp,eos-cp) << "\"");
		break;
	default:
		DLOG(("Unknown msgType --- mote=%s, id=%d, Ver=%d MsgType=%d, msglen=",
				idstr.c_str(),_moteId, _version, mtype, eos-cp));
		break;
	}
	return mtype;
}

/*
 * Check EOM (0x03 0x04 0xd). Return pointer to start of EOM.
 */
const unsigned char* WisardMote::checkEOM(const unsigned char* sos, const unsigned char* eos)
{

	if (eos - 4 < sos) {
		n_u::Logger::getInstance()->log(LOG_ERR,"Message length is too short --- len= %d", eos-sos );
		return 0;
	}

	// NIDAS will likely add a NULL to the end of the message. Check for that.
	if (*(eos - 1) == 0) eos--;
	eos -= 3;

	if (memcmp(eos,"\x03\x04\r",3) != 0) {
		WLOG(("Bad EOM --- last 3 chars= %x %x %x", *(eos), *(eos+1), *(eos+2)));
		return 0;
	}
	return eos;
}

/*
 * Check CRC. Return pointer to CRC, which is one past the end of the data portion.
 */
const unsigned char* WisardMote::checkCRC (const unsigned char* cp, const unsigned char* eos)
{
	// Initial value of eos points to one past the CRC.

	// retrieve CRC at end of message.
	if (eos-- <= cp) {
		WLOG(("Message length is too short --- len= %d", eos-cp ));
		return 0;
	}
	unsigned char crc = *eos;

	// Calculate Cksum. Start with length of message, not including checksum.
	unsigned char cksum = eos - cp;
	for (const unsigned char* cp2 = cp; cp2 < eos; ) cksum ^= *cp2++;

	if (cksum != crc ) {
		// Try to print out some header information.
		int mtype = readHead(cp, eos);
		if (!(_badCRCsByMoteId[_moteId]++ % 10)) {
			if (_moteId >= 0) {
				WLOG(("%s: %d bad CKSUMs for mote id %d, messsage type=%d, length=%d, crc=%x vs cksum=%x",
						getName().c_str(),_badCRCsByMoteId[_moteId],_moteId,mtype, (eos-cp), crc, cksum ));
			}
			else {
				WLOG(("%s: %d bad CKSUMs for unknown mote, length=%d, crc=%x vs cksum=%x",
						getName().c_str(),_badCRCsByMoteId[_moteId],(eos-cp), crc, cksum ));
			}
		}
		return 0;
	}
	return eos;
}

/* type id 0x01 */
const unsigned char* WisardMote::readPicTm(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  16 bit pic-time */
	unsigned short 	val = missValue;
	if (cp + sizeof(uint16_t) <= eos) val = _fromLittle->uint16Value(cp);
	if (val!= missValue)
		_data.push_back(val/10.0);
	else
		_data.push_back(floatNAN);
	cp += sizeof(uint16_t);
	return cp;

}

/* type id 0x04 */
const unsigned char* WisardMote::readGenShort(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  16 bit gen-short */
	unsigned short	val = missValue;
	if (cp + sizeof(uint16_t) <= eos) val = _fromLittle->uint16Value(cp);
	if (val!= missValue)
		_data.push_back(val/1.0);
	else
		_data.push_back(floatNAN);
	cp += sizeof(uint16_t);
	return cp;

}

/* type id 0x05 */
const unsigned char* WisardMote::readGenLong(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  32 bit gen-long */
	unsigned int	val = 0;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	//if (val!= miss4byteValue)
	_data.push_back(val/1.0);
	//else
	//	_data.push_back(floatNAN);
	cp += sizeof(uint16_t);
	return cp;

}


/* type id 0x0B */
const unsigned char* WisardMote::readTmSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  32 bit  t-tm ticks in sec */
	unsigned int	val = 0;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
	//	if (val!= miss4byteValue)
	_data.push_back(val);
	//else
	//	_data.push_back(floatNAN);
	return cp;
}

/* type id 0x0C */
const unsigned char* WisardMote::readTmCnt(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  32 bit  tm-count in  */
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
	_data.push_back(val);
	return cp;
}


/* type id 0x0E */
const unsigned char* WisardMote::readTm10thSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t  ttag) //ttag=microSec
{
	/* unpack  32 bit  t-tm-ticks in 10th sec */
	/* unsigned int (32 bit) can hold number of milliseconds in a year */
	unsigned int val = 0;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp) * 100;  // convert to milliseconds
	cp += sizeof(uint32_t);

	//convert sample time tag to milliseconds since 00:00 UTC
	int mSOfDay = (ttag/USECS_PER_MSEC) % MSECS_PER_DAY;

	// convert mote time to milliseconds since 00:00 UTC
	val %= MSECS_PER_DAY;

	int diff = mSOfDay - val; //mSec

	if (abs(diff) > MSECS_PER_HALF_DAY) {
		if (diff < -MSECS_PER_HALF_DAY) diff += MSECS_PER_DAY;
		else if (diff > MSECS_PER_HALF_DAY) diff -= MSECS_PER_DAY;
	}
	float fval = (float)diff / MSECS_PER_SEC;  // seconds

	// keep track of the first time difference.
	if (_tdiffByMoteId[_moteId] == 0) _tdiffByMoteId[_moteId] = diff;

	// subtract the first difference from each succeeding difference.
	// This way we can check the mote clock drift relative to the adam
	// when the mote is not initialized with an absolute time.
	diff -= _tdiffByMoteId[_moteId];
	if (abs(diff) > MSECS_PER_HALF_DAY) {
		if (diff < -MSECS_PER_HALF_DAY) diff += MSECS_PER_DAY;
		else if (diff > MSECS_PER_HALF_DAY) diff -= MSECS_PER_DAY;
	}

	float fval2 = (float)diff / MSECS_PER_SEC;

	_data.push_back(fval);
	_data.push_back(fval2);
	return cp;
}


/* type id 0x0D */
const unsigned char* WisardMote::readTm100thSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack  32 bit  t-tm-100th in sec */
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
	//if (val!= miss4byteValue)
	_data.push_back(val/100.0);
	//else
	//	_data.push_back(floatNAN);
	return cp;
}

/* type id 0x0F */
const unsigned char* WisardMote::readPicDT(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/*  16 bit jday */
	unsigned short jday = missValue;
	if (cp + sizeof(uint16_t) <= eos) jday = _fromLittle->uint16Value(cp);
	cp += sizeof(uint16_t);
	if (jday!= missValue)
		_data.push_back(jday);
	else
		_data.push_back(floatNAN);

	/*  8 bit hour+ 8 bit min+ 8 bit sec  */
	unsigned char hh = missByteValue;
	if (cp + sizeof(uint8_t) <= eos) hh = *cp;
	cp += sizeof(uint8_t);
	if (hh != missByteValue)
		_data.push_back(hh);
	else
		_data.push_back(floatNAN);

	unsigned char mm = missByteValue;
	if (cp + sizeof(uint8_t) <= eos) mm = *cp;
	cp += sizeof(uint8_t);
	if (mm != missByteValue)
		_data.push_back(mm);
	else
		_data.push_back(floatNAN);

	unsigned char ss = missByteValue;
	if (cp + sizeof(uint8_t) <= eos) ss = *cp;
	cp += sizeof(uint8_t);
	if (ss != missByteValue)
		_data.push_back(ss);
	else
		_data.push_back(floatNAN);

	return cp;
}

/* type id 0x20-0x23 */
const unsigned char* WisardMote::readTsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	/* unpack 16 bit  */
	for (int i=0; i<4; i++) {
		short val = missValueSigned;
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val!= missValueSigned)
			_data.push_back(val/100.0);
		else
			_data.push_back(floatNAN);
	}
	return cp;
}

/* type id 0x24-0x27 */
const unsigned char* WisardMote::readGsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	short val = missValueSigned;
	if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
	cp += sizeof(int16_t);
	if (val!= missValueSigned)
		_data.push_back(val/10.0);
	else
		_data.push_back(floatNAN);
	return cp;
}

/* type id 0x28-0x2B */
const unsigned char* WisardMote::readQsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	unsigned short val = missValue;
	if (cp + sizeof(uint16_t) <= eos) val = _fromLittle->uint16Value(cp);
	cp += sizeof(uint16_t);
	if (val!= missValue)
		_data.push_back(val/1.0);
	else
		_data.push_back(floatNAN);
	return cp;
}

/* type id 0x2C-0x2F */
const unsigned char* WisardMote::readTP01Data(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	// 5 signed
	for (int i=0; i<5; i++) {
		short val = missValueSigned;   // signed
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val!= (signed)0xFFFF8000){
			switch (i) {
			case 0: _data.push_back(val/10000.0); 	break;
			case 1: _data.push_back(val/1.0);		break;
			case 2: _data.push_back(val/1.0);		break;
			case 3: _data.push_back(val/100.0);		break;
			case 4: _data.push_back(val/1000.0);	break;
			}
		}
		else
			_data.push_back(floatNAN);
	}
	return cp;
}

/* type id 0x40 status-id */
const unsigned char* WisardMote::readStatusData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	unsigned char val = missByteValue;
	if (cp + 1 <= eos) val = *cp++;
	if (val!= missByteValue)
		_data.push_back(val);
	else
		_data.push_back(floatNAN);
	return cp;

}

/* type id 0x49 pwr */
const unsigned char* WisardMote::readPwrData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	for (int i=0; i<6; i++){
		unsigned short val = missValue;
		if (cp + sizeof(uint16_t) <= eos) val = _fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t);
		if (val!= missValue) {
			if (i==0 || i==3) 	_data.push_back(val/10.0);  //voltage 10th
			else _data.push_back(val/1.0);					//miliamp
		} else
			_data.push_back(floatNAN);
	}
	return cp;
}



/* type id 0x50-0x53 */
const unsigned char* WisardMote::readRnetData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	short val = missValueSigned;   // signed
	if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
	cp += sizeof(int16_t);
	if (val!= missValueSigned)
		_data.push_back(val/10.0);
	else
		_data.push_back(floatNAN);
	return cp;
}

/* type id 0x54-0x5B */
const unsigned char* WisardMote::readRswData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	unsigned short val = missValue;
	if (cp + sizeof(uint16_t) <= eos) val = _fromLittle->uint16Value(cp);
	cp += sizeof(uint16_t);
	if (val!= missValue)
		_data.push_back(val/10.0);
	else
		_data.push_back(floatNAN);
	return cp;
}

/* type id 0x5C-0x63 */
const unsigned char* WisardMote::readRlwData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	for (int i=0; i<5; i++) {
		short val = missValueSigned;   // signed
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val != missValueSigned ) {
			if (i>0)
				_data.push_back(val/100.0);          // tcase and tdome1-3
			else
				_data.push_back(val/10.0);           // tpile
		} else                                  //null
			_data.push_back(floatNAN);
	}
	return cp;
}


/* type id 0x64-0x6B */
const unsigned char* WisardMote::readRlwKZData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag)
{
	for (int i=0; i<2; i++) {
		short val = missValueSigned;   // signed
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val != missValueSigned ) {
			if (i==0) _data.push_back(val/10.0);          // RPile
			else _data.push_back(val/100.0);          	// tcase
		} else
			_data.push_back(floatNAN);
	}
	return cp;
}


void WisardMote::initFuncMap() {
	if (! _functionsMapped) {
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

		_nnMap[0x40] = &WisardMote::readStatusData;
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
		_functionsMapped = true;
	}
}

SampInfo WisardMote::_samps[] = {
		{0x0E, {
				{"Tdiff","secs","Time difference, adam-mote", "-10 10",true, true},
				{"Tdiff2","secs","Time difference, adam-mote-first_diff","-10 10", true, true},
		}},

		{0x20,{
				{"Tsoil.a.1","degC","Soil Temperature","-20 30", true, true},
				{"Tsoil.a.2","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.a.3","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.a.4","degC","Soil Temperature", "-20 30", true, true},
		}},
		{0x21,{
				{"Tsoil.b.1","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.b.2","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.b.3","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.b.4","degC","Soil Temperature", "-20 30", true, true},
		}},
		{0x22,{
				{"Tsoil.c.1","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.c.2","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.c.3","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.c.4","degC","Soil Temperature", "-20 30", true, true},
		}},
		{0x23,{
				{"Tsoil.d.1","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.d.2","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.d.3","degC","Soil Temperature", "-20 30", true, true},
				{"Tsoil.d.4","degC","Soil Temperature", "-20 30", true, true},
		}},

		{0x24, {{"Gsoil.a", "W/m^2", "Soil Heat Flux", "-200 200", true, true},}},
		{0x25, {{"Gsoil.b", "W/m^2", "Soil Heat Flux", "-200 200", true, true},}},
		{0x26, {{"Gsoil.c", "W/m^2", "Soil Heat Flux", "-200 200", true, true},}},
		{0x27, {{"Gsoil.d", "W/m^2", "Soil Heat Flux", "-200 200", true, true},}},

		{0x28,{{"QSoil.a", "vol%", "Soil Moisture", "-20 60", true, true},}},
		{0x29,{{"QSoil.b", "vol%", "Soil Moisture", "-20 60", true, true},}},
		{0x2A,{{"QSoil.c", "vol%", "Soil Moisture", "-20 60", true, true},}},
		{0x2B,{{"QSoil.d", "vol%", "Soil Moisture", "-20 60", true, true},}},

		{0x2C,{
				{"Vheat.a","V","Soil Thermal, heat volt", "0 1.5", true, true},
				{"Vpile.on.a","microV","Soil Thermal, transducer volt", "-100 800", true, true},
				{"Vpile.off.a","microV","Soil Thermal, heat volt", "-100 800", true, true},
				{"Tau63.a","secs","Soil Thermal, time diff", "0 20", true, true},
				{"lambdasoil.a","W/mDegk","Thermal property", "0 1", true, true},
		}},
		{0x2D,{
				{"Vheat.b","V","Soil Thermal, heat volt", "0 1.5", true, true},
				{"Vpile.on.b","microV","Soil Thermal, transducer volt", "-100 800", true, true},
				{"Vpile.off.b","microV","Soil Thermal, heat volt", "-100 800", true, true},
				{"Tau63.b","secs","Soil Thermal, time diff", "0 20", true, true},
				{"lambdasoil.b","W/mDegk","Thermal property", "0 1", true, true},
		}},
		{0x2E,{
				{"Vheat.c","V","Soil Thermal, heat volt", "0 1.5", true, true},
				{"Vpile.on.c","microV","Soil Thermal, transducer volt", "-100 800", true, true},
				{"Vpile.off.c","microV","Soil Thermal, heat volt", "-100 800", true, true},
				{"Tau63.c","secs","Soil Thermal, time diff", "0 20", true, true},
				{"lambdasoil.c","W/mDegk","Thermal property", "0 1", true, true},
		}},
		{0x2F,{
				{"Vheat.d","V","Soil Thermal, heat volt", "0 1.5", true, true},
				{"Vpile.on.d","microV","Soil Thermal, transducer volt", "-100 800", true, true},
				{"Vpile.off.d","microV","Soil Thermal, heat volt", "-100 800", true, true},
				{"Tau63.d","secs","Soil Thermal, time diff", "0 20", true, true},
				{"lambdasoil.d","W/mDegk","Thermal property", "0 1", true, true},
		}},

		{0x40, {{"StatusId","Count","Sampling mode", "-10 10", true, false},}},

		{0x49, {
				{"Vin","V","Volt supply", "0 20", true, true},
				{"Iin","mAmp","I-Current supply", "0 300", true, false},
				{"I3.3","mAmp","I-Current 3.3 ", "0 300", true, false},
				{"V3.3","V","Volt 3.3", "3 4", true, false},
				{"Ixb","mAmp","I-current Ixbee", "0 300", true, false},
				{"I9x","mAmp","I-current Isensor I9x", "0 300", true, false},
		}},

		{0x50, {{"Rnet.a","W/m^2","Net Radiation", "-200 800", true, true},}},
		{0x51, {{"Rnet.b","W/m^2","Net Radiation", "-200 800", true, true},}},
		{0x52, {{"Rnet.c","W/m^2","Net Radiation", "-200 800", true, true},}},
		{0x53, {{"Rnet.d","W/m^2","Net Radiation", "-200 800", true, true},}},

		{0x54, {{"Rsw.in.a","W/m^2","Incoming Short Wave", "0 1000", true, true},}},
		{0x55, {{"Rsw.in.b","W/m^2","Incoming Short Wave", "0 1000", true, true},}},
		{0x56, {{"Rsw.in.c","W/m^2","Incoming Short Wave", "0 1000", true, true},}},
		{0x57, {{"Rsw.in.d","W/m^2","Incoming Short Wave", "0 1000", true, true},}},

		{0x58, {{"Rsw.out.a","W/m^2","Outgoing Short Wave", "0 300", true, true},}},
		{0x59, {{"Rsw.out.b","W/m^2","Outgoing Short Wave", "0 300", true, true},}},
		{0x5A, {{"Rsw.out.c","W/m^2","Outgoing Short Wave", "0 300", true, true},}},
		{0x5B, {{"Rsw.out.d","W/m^2","Outgoing Short Wave", "0 300", true, true},}},

		{0x5C,{
				{"Rpile.in.a","W/m^2","Epply pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.a","degC","Epply case temperature, incoming", "-20 30", true, true},
				{"Tdome1.in.a","degC","Epply dome temperature #1, incoming", "-20 30", true, true},
				{"Tdome2.in.a","degC","Epply dome temperature #2, incoming", "-20 30", true, true},
				{"Tdome3.in.a","degC","Epply dome temperature #3, incoming", "-20 30", true, true},
		}},
		{0x5D,{
				{"Rpile.in.b","W/m^2","Epply pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.b","degC","Epply case temperature, incoming", "-20 30", true, true},
				{"Tdome1.in.b","degC","Epply dome temperature #1, incoming", "-20 30", true, true},
				{"Tdome2.in.b","degC","Epply dome temperature #2, incoming", "-20 30", true, true},
				{"Tdome3.in.b","degC","Epply dome temperature #3, incoming", "-20 30", true, true},
		}},
		{0x5E,{
				{"Rpile.in.c","W/m^2","Epply pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.c","degC","Epply case temperature, incoming", "-20 30", true, true},
				{"Tdome1.in.c","degC","Epply dome temperature #1, incoming", "-20 30", true, true},
				{"Tdome2.in.c","degC","Epply dome temperature #2, incoming", "-20 30", true, true},
				{"Tdome3.in.c","degC","Epply dome temperature #3, incoming", "-20 30", true, true},
		}},
		{0x5F,{
				{"Rpile.in.d","W/m^2","Epply pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.d","degC","Epply case temperature, incoming", "-20 30", true, true},
				{"Tdome1.in.d","degC","Epply dome temperature #1, incoming", "-20 30", true, true},
				{"Tdome2.in.d","degC","Epply dome temperature #2, incoming", "-20 30", true, true},
				{"Tdome3.in.d","degC","Epply dome temperature #3, incoming", "-20 30", true, true},
		}},
		{0x60,{
				{"Rpile.out.a","W/m^2","Epply pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.a","degC","Epply case temperature, outgoing", "-20 30", true, true},
				{"Tdome1.out.a","degC","Epply dome temperature #1, outgoing", "-20 30", true, true},
				{"Tdome2.out.a","degC","Epply dome temperature #2, outgoing", "-20 30", true, true},
				{"Tdome3.out.a","degC","Epply dome temperature #3, outgoing", "-20 30", true, true},
		}},
		{0x61,{
				{"Rpile.out.b","W/m^2","Epply pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.b","degC","Epply case temperature, outgoing", "-20 30", true, true},
				{"Tdome1.out.b","degC","Epply dome temperature #1, outgoing", "-20 30", true, true},
				{"Tdome2.out.b","degC","Epply dome temperature #2, outgoing", "-20 30", true, true},
				{"Tdome3.out.b","degC","Epply dome temperature #3, outgoing", "-20 30", true, true},
		}},
		{0x62,{
				{"Rpile.out.c","W/m^2","Epply pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.c","degC","Epply case temperature, outgoing", "-20 30", true, true},
				{"Tdome1.out.c","degC","Epply dome temperature #1, outgoing", "-20 30", true, true},
				{"Tdome2.out.c","degC","Epply dome temperature #2, outgoing", "-20 30", true, true},
				{"Tdome3.out.c","degC","Epply dome temperature #3, outgoing", "-20 30", true, true},
		}},
		{0x63,{
				{"Rpile.out.d","W/m^2","Epply pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.out.d","degC","Epply case temperature, incoming", "-20 30", true, true},
				{"Tdome1.out.d","degC","Epply dome temperature #1, incoming", "-20 30", true, true},
				{"Tdome2.out.d","degC","Epply dome temperature #2, incoming", "-20 30", true, true},
				{"Tdome3.out.d","degC","Epply dome temperature #3, incoming", "-20 30", true, true},
		}},

		{0x64,{
				{"Rpile.in.akz","W/m^2","K&Z pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.akz","degC","K&Z case temperature, incoming", "-20 30", true, true},
		}},
		{0x65,{
				{"Rpile.in.bkz","W/m^2","K&Z pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.bkz","degC","K&Z case temperature, incoming", "-20 30", true, true},
		}},
		{0x66,{
				{"Rpile.in.ckz","W/m^2","K&Z pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.ckz","degC","K&Z case temperature, incoming", "-20 30", true, true},
		}},
		{0x67,{
				{"Rpile.in.dkz","W/m^2","K&Z pyranometer thermopile, incoming", "-200 200", true, true},
				{"Tcase.in.dkz","degC","K&Z case temperature, incoming", "-20 30", true, true},
		}},
		{0x68,{
				{"Rpile.out.akz","W/m^2","K&Z pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.akz","degC","K&Z case temperature, outgoing", "-20 30", true, true},
		}},
		{0x69,{
				{"Rpile.out.bkz","W/m^2","K&Z pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.bkz","degC","K&Z case temperature, outgoing", "-20 30", true, true},
		}},
		{0x6A,{
				{"Rpile.out.ckz","W/m^2","K&Z pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.ckz","degC","K&Z case temperature, outgoing", "-20 30", true, true},
		}},
		{0x6B,{
				{"Rpile.out.dkz","W/m^2","K&Z pyranometer thermopile, outgoing", "-200 200", true, true},
				{"Tcase.out.dkz","degC","K&Z case temperature, outgoing", "-20 30", true, true},
		}},

		{0,{{},}}

};

