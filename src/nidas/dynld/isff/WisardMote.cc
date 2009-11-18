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
#include <nidas/core/DSMConfig.h>
#include <cmath>
#include <iostream>
#include <memory> // auto_ptr<>

using namespace nidas::dynld;
using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
bool WisardMote::_functionsMapped = false;

/* static */
std::map<unsigned char, WisardMote::readFunc> WisardMote::_nnMap;

/* static */
const n_u::EndianConverter* WisardMote::_fromLittle =
	n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);


NIDAS_CREATOR_FUNCTION_NS(isff,WisardMote)


WisardMote::WisardMote():
	_moteId(-1),_version(-1),_badCRCs(0)
	{
	initFuncMap();
	}

bool WisardMote::process(const Sample* samp,list<const Sample*>& results) throw()
{
	/* unpack a WisardMote packet, consisting of binary integer data from a variety
	 * of sensor types. */
	const unsigned char* cp= (const unsigned char*) samp->getConstVoidDataPtr();
	const unsigned char* eos = cp + samp->getDataByteLength();

<<<<<<< .working
<<<<<<< .working
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\n\n process  ttag= %d getId()= %d samp->getId()= %d  getDsmId()=%d", samp->getTimeTag(),idkp, samp->getId(), getDSMId());
	//print out raw-input data for debug
	n_u::Logger::getInstance()->log(LOG_DEBUG, "raw data = ");
	for (int i= 0; i<len; i++) n_u::Logger::getInstance()->log(LOG_DEBUG, " %x ", *(cp+i)); ////printf(" %x ", *(cp+i));
=======
	/*  check for good EOM  */
	if (!(eos = checkEOM(cp,eos))) return false;
>>>>>>> .merge-right.r5099
=======
	/*  check for good EOM  */
	if (!(eos = checkEOM(cp,eos))) return false;
>>>>>>> .merge-right.r5099

	/*  verify crc for data  */
	if (!(eos = checkCRC(cp,eos))) {
		if (!(_badCRCs++ % 100)) WLOG(("%s: %d bad CRCs",getName().c_str(),_badCRCs));
		return false;
	}

	/*  read header */
	int mtype = readHead(cp, eos);
	if (mtype == -1) return false;  // invalid

	if (mtype != 1) return false;   // other than a data message

<<<<<<< .working
<<<<<<< .working
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\n\n process  nname= %s" , nname.c_str());
=======
	while (cp < eos) {
>>>>>>> .merge-right.r5099
=======
	while (cp < eos) {
>>>>>>> .merge-right.r5099

<<<<<<< .working
<<<<<<< .working
	// crc+eom+0x0(1+3+1) + sensorTypeId+data (1+1 at least) = 7
	while ((cp+7) <= eos) {
		/*  get data one set data  */
		/* get sTypeId    */
		unsigned char sTypeId = *cp++;  msgLen++;
		n_u::Logger::getInstance()->log(LOG_DEBUG,"\n\n --SensorTypeId = %x sTypeId=%d  getId()=%d   getId()+stypeId=%d  samp->getId()=%d samp->getRawId=%d ttag= %d ",sTypeId, sTypeId, idkp, (idkp+sTypeId),samp->getId(), samp->getRawId(), samp->getTimeTag());
		//pushNodeName(getId(), sTypeId);                     //getId()--get dsm and sensor
=======
		/* get sensor type id    */
		unsigned char sensorTypeId = *cp++;
>>>>>>> .merge-right.r5099
=======
		/* get sensor type id    */
		unsigned char sensorTypeId = *cp++;
>>>>>>> .merge-right.r5099

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
		cp = (this->*func)(cp,eos);

		/* create an output floating point sample */
		if (_data.size() == 0) 	continue;

		SampleT<float>* osamp = getSample<float>(_data.size());
		osamp->setTimeTag(samp->getTimeTag());
		osamp->setId(getId()+(_moteId << 8) + sensorTypeId);
		float* dout = osamp->getDataPtr();
<<<<<<< .working
<<<<<<< .working
		for (unsigned int i=0; i<data.size(); i++) {
			*dout++ = (float)data[i];
			n_u::Logger::getInstance()->log(LOG_DEBUG, "\ndata= %f  idx= %i", data[i], i);
=======

		std::copy(_data.begin(),_data.end(),dout);
#ifdef DEBUG
		for (unsigned int i=0; i<_data.size(); i++) {
			DLOG(("data[%d]=%f",i, _data[i]));
>>>>>>> .merge-right.r5099
=======

		std::copy(_data.begin(),_data.end(),dout);
#ifdef DEBUG
		for (unsigned int i=0; i<_data.size(); i++) {
			DLOG(("data[%d]=%f",i, _data[i]));
>>>>>>> .merge-right.r5099
		}
#endif
		/* push out */
		results.push_back(osamp);
<<<<<<< .working
<<<<<<< .working
		n_u::Logger::getInstance()->log(LOG_DEBUG,"sampleId= %x",getId()+sampleId+sTypeId);
		n_u::Logger::getInstance()->log(LOG_DEBUG,"\n end of loop-- cp= %d cp+7=%d eod=%d type= %x \n",cp,  cp+7, eos, sTypeId);
=======
>>>>>>> .merge-right.r5099
=======
>>>>>>> .merge-right.r5099
	}
	return true;
}

<<<<<<< .working
<<<<<<< .working
void WisardMote::fromDOMElement(
		const xercesc::DOMElement* node)
throw(n_u::InvalidParameterException)
{

	DSMSerialSensor::fromDOMElement(node);

	const std::list<const Parameter*>& params = getParameters();
	list<const Parameter*>::const_iterator pi;
	for (pi = params.begin(); pi != params.end(); ++pi) {
		const Parameter* param = *pi;
		const string& pname = param->getName();
		if (pname == "rate") {
			if (param->getLength() != 1)
				throw n_u::InvalidParameterException(getName(),"parameter",
						"bad rate parameter");
			//setScanRate((int)param->getNumericValue(0));
		}
	}
}

void WisardMote::addSampleTag(SampleTag* stag) throw(InvalidParameterException) {
	n_u::Logger::getInstance()->log(LOG_DEBUG,"entering addSampleTag...");
=======
void WisardMote::addSampleTag(SampleTag* stag) throw(n_u::InvalidParameterException) {
>>>>>>> .merge-right.r5099
=======
void WisardMote::addSampleTag(SampleTag* stag) throw(n_u::InvalidParameterException) {
>>>>>>> .merge-right.r5099
	for (int i = 0; ; i++)
	{
<<<<<<< .working
<<<<<<< .working
		unsigned int id= samps[i].id;
		if ( id==0  ) {
			break;
		}

		n_u::Logger::getInstance()->log(LOG_DEBUG,"samps[%i].id=%i", i, id);
=======
		unsigned int id = _samps[i].id;
		if (id == 0) break;

>>>>>>> .merge-right.r5099
=======
		unsigned int id = _samps[i].id;
		if (id == 0) break;

>>>>>>> .merge-right.r5099
		SampleTag* newtag = new SampleTag(*stag);
		newtag->setSampleId(newtag->getSampleId()+id);
<<<<<<< .working
<<<<<<< .working
=======
		int nv = sizeof(_samps[i].variables)/sizeof(_samps[i].variables[0]);
>>>>>>> .merge-right.r5099
=======
		int nv = sizeof(_samps[i].variables)/sizeof(_samps[i].variables[0]);
>>>>>>> .merge-right.r5099

<<<<<<< .working
<<<<<<< .working
		int nv = sizeof(samps[i].variables)/sizeof(samps[i].variables[0]);

=======
>>>>>>> .merge-right.r5099
=======
>>>>>>> .merge-right.r5099
		//vars
		int len=1;
		for (int j = 0; j < nv; j++) {
			VarInfo vinf = _samps[i].variables[j];
			if (!vinf.name || sizeof(vinf.name)<=0 ) break;
			Variable* var = new Variable();
			var->setName(vinf.name);
			var->setUnits(vinf.units);
			var->setLongName(vinf.longname);
<<<<<<< .working
<<<<<<< .working
		    var->setLength(len);
			var->setSuffix(newtag->getSuffix());

		    newtag->addVariable(var);
			n_u::Logger::getInstance()->log(LOG_DEBUG,"samps[%i].variable[%i]=%s", i, j,samps[i].variables[j].name);
=======
			var->setDynamic(vinf.dynamic);
			var->setLength(len);
			var->setSuffix(newtag->getSuffix());

			newtag->addVariable(var);
>>>>>>> .merge-right.r5099
=======
			var->setDynamic(vinf.dynamic);
			var->setLength(len);
			var->setSuffix(newtag->getSuffix());

			newtag->addVariable(var);
>>>>>>> .merge-right.r5099
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
	_moteId=0;

	/* look for mote id. First skip non-digits. */
	for ( ; cp < eos; cp++) if (::isdigit(*cp)) break;
	if (cp == eos) return -1;

	const unsigned char* colon = (const unsigned char*)::memchr(cp,':',eos-cp);
	if (!colon) return -1;

<<<<<<< .working
<<<<<<< .working
/**
 * find NodeName, version, MsgType (0-log sensortype+SN 1-seq+time+data 2-err msg)
 */
bool WisardMote::findHead(const unsigned char* cp, const unsigned char* eos, int& msgLen) {
	n_u::Logger::getInstance()->log(LOG_DEBUG, "findHead...");
	sampleId=0;
	/* look for nodeName */
	for ( ; cp < eos; cp++, msgLen++) {
		char c = *cp;
		if (c!= ':') nname.push_back(c);
		else break;
=======
	// read the moteId
	string idstr((const char*)cp,colon-cp);
	{
		stringstream ssid(idstr);
		ssid >> std::dec >> _moteId;
		if (ssid.fail()) return -1;
>>>>>>> .merge-right.r5099
=======
	// read the moteId
	string idstr((const char*)cp,colon-cp);
	{
		stringstream ssid(idstr);
		ssid >> std::dec >> _moteId;
		if (ssid.fail()) return -1;
>>>>>>> .merge-right.r5099
	}

<<<<<<< .working
<<<<<<< .working
	//get sampleId
	unsigned int i=0; //look for decimal
	for (; i<nname.size(); i++) {
		char c = nname.at(i);
		if (c <= '9' && c>='0')  break;
	}
	string sid= nname.substr(i, (nname.size()-i));
	stringstream ssid(sid); // Could of course also have done ss("1234") directly.
	unsigned int val;
	ssid >>std::dec>> val;
	sampleId= val<<8;
	n_u::Logger::getInstance()->log(LOG_DEBUG, "sid=%s sampleId=$i", sid.c_str(), sampleId);
=======
	DLOG(("idstr=%s moteId=$i", idstr.c_str(), _moteId));
>>>>>>> .merge-right.r5099
=======
	DLOG(("idstr=%s moteId=$i", idstr.c_str(), _moteId));
>>>>>>> .merge-right.r5099

	cp = colon + 1;

	// version number
	if (cp == eos) return -1;
	_version = *cp++;

	// message type
	if (cp == eos) return -1;
	int mtype = *cp++;

	switch(mtype) {
	case 0:
		/* unpack 1bytesId + 16 bit s/n */
<<<<<<< .working
<<<<<<< .working
		if (cp + 1 + 2 > eos) return false;
		int sId;
		sId = *cp++; msgLen++;
		int sn;
		sn = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+= sizeof(uint16_t);
		n_u::Logger::getInstance()->log(LOG_DEBUG,"NodeName=%s Ver=%x MsgType=%x STypeId=%x SN=%x hmsgLen=%i",
				nname.c_str(), ver, mtype, sId, sn, msgLen);
		return false;
=======
		if (cp + 1 + sizeof(uint16_t) > eos) return false;
		{
			int sensorTypeId = *cp++;
			int serialNumber = *cp++;
			_sensorSerialNumbersByType[sensorTypeId] = serialNumber;
			DLOG(("mote=%s, id=%d, ver=%d MsgType=%d sensorTypeId=%d SN=%d",
					idstr.c_str(),_moteId,_version, mtype, sensorTypeId, serialNumber));
		}
		break;
>>>>>>> .merge-right.r5099
=======
		if (cp + 1 + sizeof(uint16_t) > eos) return false;
		{
			int sensorTypeId = *cp++;
			int serialNumber = *cp++;
			_sensorSerialNumbersByType[sensorTypeId] = serialNumber;
			DLOG(("mote=%s, id=%d, ver=%d MsgType=%d sensorTypeId=%d SN=%d",
					idstr.c_str(),_moteId,_version, mtype, sensorTypeId, serialNumber));
		}
		break;
>>>>>>> .merge-right.r5099
	case 1:
<<<<<<< .working
<<<<<<< .working
		/* unpack 1byte seq + 16-bit time */
		if (cp + 1+ sizeof(uint16_t) > eos) return false;
		unsigned char seq;
		seq = *cp++;  msgLen++;
		n_u::Logger::getInstance()->log(LOG_DEBUG,"NodeName=%s Ver=%x MsgType=%x seq=%x hmsgLen=%i",
				nname.c_str(), ver, mtype, seq, msgLen);
=======
		/* unpack 1byte sequence */
		if (cp + 1 > eos) return false;
		_sequence = *cp++;
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d seq=%d",
				idstr.c_str(), _moteId, _version, mtype, _sequence));
>>>>>>> .merge-right.r5099
=======
		/* unpack 1byte sequence */
		if (cp + 1 > eos) return false;
		_sequence = *cp++;
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d seq=%d",
				idstr.c_str(), _moteId, _version, mtype, _sequence));
>>>>>>> .merge-right.r5099
		break;
	case 2:
<<<<<<< .working
<<<<<<< .working
		n_u::Logger::getInstance()->log(LOG_DEBUG,"NodeName=%s Ver=%x MsgType=%x hmsgLen=%i ErrMsg=%s",
				nname.c_str(), ver, mtype, msgLen, cp);
		return false;//skip for now

=======
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d ErrMsg=\"",
				idstr.c_str(), _moteId, _version, mtype) << string((const char*)cp,eos-cp) << "\"");
		break;
>>>>>>> .merge-right.r5099
=======
		DLOG(("mote=%s, id=%d, Ver=%d MsgType=%d ErrMsg=\"",
				idstr.c_str(), _moteId, _version, mtype) << string((const char*)cp,eos-cp) << "\"");
		break;
>>>>>>> .merge-right.r5099
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
<<<<<<< .working
<<<<<<< .working
bool WisardMote::findEOM(const unsigned char* cp, unsigned char len) {
	n_u::Logger::getInstance()->log(LOG_DEBUG, "findEOM len= %d ",len);
=======
const unsigned char* WisardMote::checkEOM(const unsigned char* sos, const unsigned char* eos)
{
>>>>>>> .merge-right.r5099
=======
const unsigned char* WisardMote::checkEOM(const unsigned char* sos, const unsigned char* eos)
{
>>>>>>> .merge-right.r5099

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
 * Check CRC. Return pointer to CRC.
 */
const unsigned char* WisardMote::checkCRC (const unsigned char* cp, const unsigned char* eos)
{
	// retrieve CRC at end of message.
	if (eos - 1 < cp) {
		WLOG(("Message length is too short --- len= %d", eos-cp ));
		return 0;
	}
	unsigned char crc= *(eos-1);

	//calculate Cksum
	unsigned char cksum = (eos - cp) - 1;  //skip CRC+EOM+0x0
	for( ; cp < eos - 1; ) {
		unsigned char c =*cp++;
<<<<<<< .working
<<<<<<< .working
=======
=======
>>>>>>> .merge-right.r5099
		cksum ^= c ;
>>>>>>> .merge-right.r5099
	}

	if (cksum != crc ) {
		n_u::Logger::getInstance()->log(LOG_ERR,"Bad CKSUM --- %x vs  %x ", crc, cksum );
		return 0;
	}
	return eos - 1;
}

/* type id 0x01 */
const unsigned char* WisardMote::readPicTm(const unsigned char* cp, const unsigned char* eos)
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

<<<<<<< .working
<<<<<<< .working
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\nPic_time = %d ",  val);
=======
>>>>>>> .merge-right.r5099
=======
>>>>>>> .merge-right.r5099
}

/* type id 0x04 */
const unsigned char* WisardMote::readGenShort(const unsigned char* cp, const unsigned char* eos)
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

<<<<<<< .working
<<<<<<< .working
void WisardMote::setTmSec(const unsigned char* cp, const unsigned char* eos){
	/* unpack  32 bit  t-tm ticks in sec */
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\ntotal-time-ticks in sec = %d ",  val);
=======
/* type id 0x05 */
const unsigned char* WisardMote::readGenLong(const unsigned char* cp, const unsigned char* eos)
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

>>>>>>> .merge-right.r5099
=======
/* type id 0x05 */
const unsigned char* WisardMote::readGenLong(const unsigned char* cp, const unsigned char* eos)
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

>>>>>>> .merge-right.r5099
}


/* type id 0x0B */
const unsigned char* WisardMote::readTmSec(const unsigned char* cp, const unsigned char* eos)
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
const unsigned char* WisardMote::readTmCnt(const unsigned char* cp, const unsigned char* eos)
{
	/* unpack  32 bit  tm-count in  */
<<<<<<< .working
<<<<<<< .working
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\ntime-count = %d ",  val);
=======
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
//	if (val!= miss4byteValue)
		_data.push_back(val);
	//else
		//_data.push_back(floatNAN);
	return cp;
>>>>>>> .merge-right.r5099
=======
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
//	if (val!= miss4byteValue)
		_data.push_back(val);
	//else
		//_data.push_back(floatNAN);
	return cp;
>>>>>>> .merge-right.r5099
}


/* type id 0x0E */
const unsigned char* WisardMote::readTm10thSec(const unsigned char* cp, const unsigned char* eos)
{
	/* unpack  32 bit  t-tm-ticks in 10th sec */
<<<<<<< .working
<<<<<<< .working
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val/10);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\ntotal-time-ticks-10th sec = %d ",  val);
=======
	unsigned int	val = 0;// miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
//	if (val!= miss4byteValue)
		//TODO convert to diff of currenttime-val. Users want to see the diff, not the raw count
		_data.push_back(val/10.);
	//else {
		//_data.push_back(floatNAN);
	//}
	return cp;
>>>>>>> .merge-right.r5099
=======
	unsigned int	val = 0;// miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
//	if (val!= miss4byteValue)
		//TODO convert to diff of currenttime-val. Users want to see the diff, not the raw count
		_data.push_back(val/10.);
	//else {
		//_data.push_back(floatNAN);
	//}
	return cp;
>>>>>>> .merge-right.r5099
}


/* type id 0x0D */
const unsigned char* WisardMote::readTm100thSec(const unsigned char* cp, const unsigned char* eos)
{
	/* unpack  32 bit  t-tm-100th in sec */
<<<<<<< .working
<<<<<<< .working
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val/100);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\ntotal-time-ticks in 100th sec = %d ",  val);
=======
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
	//if (val!= miss4byteValue)
		_data.push_back(val/100.0);
	//else
	//	_data.push_back(floatNAN);
	return cp;
>>>>>>> .merge-right.r5099
=======
	unsigned int	val = 0;//miss4byteValue;
	if (cp + sizeof(uint32_t) <= eos) val = _fromLittle->uint32Value(cp);
	cp += sizeof(uint32_t);
	//if (val!= miss4byteValue)
		_data.push_back(val/100.0);
	//else
	//	_data.push_back(floatNAN);
	return cp;
>>>>>>> .merge-right.r5099
}

/* type id 0x0F */
const unsigned char* WisardMote::readPicDT(const unsigned char* cp, const unsigned char* eos)
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
<<<<<<< .working
<<<<<<< .working
		data.push_back(floatNAN);
	//printf("\n jday= %x hh=%x mm=%x ss=%d",  jday, hh, mm, ss);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\n jday= %d hh=%d mm=%d ss=%d",  jday, hh, mm, ss);
=======
		_data.push_back(floatNAN);

	return cp;
>>>>>>> .merge-right.r5099
=======
		_data.push_back(floatNAN);

	return cp;
>>>>>>> .merge-right.r5099
}

/* type id 0x20-0x23 */
const unsigned char* WisardMote::readTsoilData(const unsigned char* cp, const unsigned char* eos)
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
const unsigned char* WisardMote::readGsoilData(const unsigned char* cp, const unsigned char* eos)
{
	short val = missValueSigned;
	if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
	cp += sizeof(int16_t);
	if (val!= missValueSigned)
		_data.push_back(val/1.0);
	else
		_data.push_back(floatNAN);
	return cp;
}

/* type id 0x28-0x2B */
const unsigned char* WisardMote::readQsoilData(const unsigned char* cp, const unsigned char* eos)
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
const unsigned char* WisardMote::readTP01Data(const unsigned char* cp, const unsigned char* eos)
{
	// 5 signed
	for (int i=0; i<5; i++) {
		short val = missValueSigned;   // signed
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val!= (signed)0xFFFF8000){
			switch (i) {
			case 0: _data.push_back(val/10000.0);
			case 1: _data.push_back(val/1.0);
			case 2: _data.push_back(val/1.0);
			case 3: _data.push_back(val/100.0);
			case 4: _data.push_back(val/1000.0);
			}
		}
		else
			_data.push_back(floatNAN);
	}
	return cp;
}

/* type id 0x40 status-id */
const unsigned char* WisardMote::readStatusData(const unsigned char* cp, const unsigned char* eos)
{
	unsigned char val = missByteValue;
	if (cp + 1 <= eos) val = *cp++;
	if (val!= missByteValue)
		_data.push_back(val);
	else
		_data.push_back(floatNAN);
	return cp;

}

<<<<<<< .working
<<<<<<< .working
		if (val!= 0x8000 ) {                    //not null
		    if (i>0)
	                data.push_back(val/100.0);          // tcase and tdome1-3
                     else
                        data.push_back(val/10.0);           // tpile
                } else                                  //null
			data.push_back(floatNAN);
=======
/* type id 0x49 pwr */
const unsigned char* WisardMote::readPwrData(const unsigned char* cp, const unsigned char* eos)
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
>>>>>>> .merge-right.r5099
=======
/* type id 0x49 pwr */
const unsigned char* WisardMote::readPwrData(const unsigned char* cp, const unsigned char* eos)
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
>>>>>>> .merge-right.r5099
	}
	return cp;
}



/* type id 0x50-0x53 */
const unsigned char* WisardMote::readRnetData(const unsigned char* cp, const unsigned char* eos)
{
	short val = missValueSigned;   // signed
	if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
	cp += sizeof(int16_t);
	if (val!= missValueSigned)
		_data.push_back(val/10.0);
	else
<<<<<<< .working
<<<<<<< .working
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\nStatus= %d ",  val);
=======
		_data.push_back(floatNAN);
	return cp;
}
>>>>>>> .merge-right.r5099
=======
		_data.push_back(floatNAN);
	return cp;
}
>>>>>>> .merge-right.r5099

/* type id 0x54-0x5B */
const unsigned char* WisardMote::readRswData(const unsigned char* cp, const unsigned char* eos)
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

<<<<<<< .working
<<<<<<< .working
void WisardMote::setPwrData(const unsigned char* cp, const unsigned char* eos){
	n_u::Logger::getInstance()->log(LOG_DEBUG,"\nPower Data= ");
	for (int i=0; i<6; i++){
		if (cp + sizeof(uint16_t) > eos) return;
		int val = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
=======
/* type id 0x5C-0x63 */
const unsigned char* WisardMote::readRlwData(const unsigned char* cp, const unsigned char* eos)
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
>>>>>>> .merge-right.r5099
=======
/* type id 0x5C-0x63 */
const unsigned char* WisardMote::readRlwData(const unsigned char* cp, const unsigned char* eos)
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
>>>>>>> .merge-right.r5099


/* type id 0x64-0x6B */
const unsigned char* WisardMote::readRlwKZData(const unsigned char* cp, const unsigned char* eos)
{
	for (int i=0; i<2; i++) {
		short val = missValueSigned;   // signed
		if (cp + sizeof(int16_t) <= eos) val = _fromLittle->int16Value(cp);
		cp += sizeof(int16_t);
		if (val != missValueSigned ) {
			if (i==0) _data.push_back(val/10.0);          // RPile
			else _data.push_back(val/100.0);          	// tcase
		} else
<<<<<<< .working
<<<<<<< .working
			data.push_back(floatNAN);
		n_u::Logger::getInstance()->log(LOG_DEBUG," %d ", val);
=======
			_data.push_back(floatNAN);
>>>>>>> .merge-right.r5099
=======
			_data.push_back(floatNAN);
>>>>>>> .merge-right.r5099
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
		{0x0E, {{"TTime-kicks","secs","Total Time kick", true},}},

		{0x20,{
				{"Tsoil.a.1","degC","Soil Temperature", true},
				{"Tsoil.a.2","degC","Soil Temperature", true},
				{"Tsoil.a.3","degC","Soil Temperature", true},
				{"Tsoil.a.4","degC","Soil Temperature", true}, }
		},
		{0x21,{
				{"Tsoil.b.1","degC","Soil Temperature", true},
				{"Tsoil.b.2","degC","Soil Temperature", true},
				{"Tsoil.b.3","degC","Soil Temperature", true},
				{"Tsoil.b.4","degC","Soil Temperature", true}, }
		},
		{0x22,{
				{"Tsoil.c.1","degC","Soil Temperature", true},
				{"Tsoil.c.2","degC","Soil Temperature", true},
				{"Tsoil.c.3","degC","Soil Temperature", true},
				{"Tsoil.c.4","degC","Soil Temperature", true}, }
		},
		{0x23,{
				{"Tsoil.d.1","degC","Soil Temperature", true},
				{"Tsoil.d.2","degC","Soil Temperature", true},
				{"Tsoil.d.3","degC","Soil Temperature", true},
				{"Tsoil.d.4","degC","Soil Temperature", true}, }
		},

		{0x24, {{"Gsoil.a", "W/m^2", "Soil Heat Flux", true},}},
		{0x25, {{"Gsoil.b", "W/m^2", "Soil Heat Flux", true},}},
		{0x26, {{"Gsoil.c", "W/m^2", "Soil Heat Flux", true},}},
		{0x27, {{"Gsoil.d", "W/m^2", "Soil Heat Flux", true},}},

		{0x28,{{"QSoil.a", "vol%", "Soil Moisture", true},}},
		{0x29,{{"QSoil.b", "vol%", "Soil Moisture", true},}},
		{0x2A,{{"QSoil.c", "vol%", "Soil Moisture", true},}},
		{0x2B,{{"QSoil.d", "vol%", "Soil Moisture", true},}},

		{0x2C,{
				{"Vheat.a","V","Soil Thermal, heat volt", true},
				{"Vpile-on.a","microV","Soil Thermal, transducer volt", true},
				{"Vpile-off.a","microV","Soil Thermal, heat volt", true},
				{"Tau63.a","secs","Soil Thermal, time diff", true},
				{"L.a","W/mDegk","Thermal property", true}, }
		},
		{0x2D,{
				{"Vheat.b","V","Soil Thermal, heat volt", true},
				{"Vpile-on.b","microV","Soil Thermal, transducer volt", true},
				{"Vpile-off.b","microV","Soil Thermal, heat volt", true},
				{"Tau63.b","secs","Soil Thermal, time diff", true},
				{"L.b","W/mDegk","Thermal property", true}, }
		},
		{0x2E,{
				{"Vheat.c","V","Soil Thermal, heat volt", true},
				{"Vpile-on.c","microV","Soil Thermal, transducer volt", true},
				{"Vpile-off.c","microV","Soil Thermal, heat volt", true},
				{"Tau63.c","secs","Soil Thermal, time diff", true},
				{"L.c","W/mDegk","Thermal property", true}, }
		},
		{0x2F,{
				{"Vheat.d","V","Soil Thermal, heat volt", true},
				{"Vpile-on.d","microV","Soil Thermal, transducer volt", true},
				{"Vpile-off.d","microV","Soil Thermal, heat volt", true},
				{"Tau63.d","secs","Soil Thermal, time diff", true},
				{"L.d","W/mDegk","Thermal property", true}, }
		},

		{0x40, {{"StatusId","Count","Sampling mode", true},}},

		{0x50, {{"Rnet.a","W/m^2","Net Radiation", true},}},
		{0x51, {{"Rnet.b","W/m^2","Net Radiation", true},}},
		{0x52, {{"Rnet.c","W/m^2","Net Radiation", true},}},
		{0x53, {{"Rnet.d","W/m^2","Net Radiation", true},}},

		{0x54, {{"Rsw.i.a","W/m^2","Incoming Short Wave", true},}},
		{0x55, {{"Rsw.i.b","W/m^2","Incoming Short Wave", true},}},
		{0x56, {{"Rsw.i.c","W/m^2","Incoming Short Wave", true},}},
		{0x57, {{"Rsw.i.d","W/m^2","Incoming Short Wave", true},}},

		{0x58, {{"Rsw.o.a","W/m^2","Outgoing Short Wave", true},}},
		{0x59, {{"Rsw.o.b","W/m^2","Outgoing Short Wave", true},}},
		{0x5A, {{"Rsw.o.c","W/m^2","Outgoing Short Wave", true},}},
		{0x5B, {{"Rsw.o.d","W/m^2","Outgoing Short Wave", true},}},

		{0x5C,{
				{"Rlw.i.tpile.a","W/m^2","Incoming Long Wave", true},
				{"Rlw.i.tcase.a","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome1.a","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome2.a","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome3.a","degC","Incoming Long Wave", true},}
		},
		{0x5D,{
				{"Rlw.i.tpile.b","W/m^2","Incoming Long Wave", true},
				{"Rlw.i.tcase.b","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome1.b","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome2.b","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome3.b","degC","Incoming Long Wave", true},}
		},
		{0x5E,{
				{"Rlw.i.tpile.c","W/m^2","Incoming Long Wave", true},
				{"Rlw.i.tcase.c","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome1.c","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome2.c","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome3.c","degC","Incoming Long Wave", true},}
		},
		{0x5F,{
				{"Rlw.i.tpile.d","W/m^2","Incoming Long Wave", true},
				{"Rlw.i.tcase.d","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome1.d","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome2.d","degC","Incoming Long Wave", true},
				{"Rlw.i.tdome3.d","degC","Incoming Long Wave", true},}
		},

		{0x60,{
				{"Rlw-o.tpile.a","W/m^2","Outgoing Long Wave", true},
				{"Rlw-o.tcase.a","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome1.a","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome2.a","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome3.a","degC","Outgoing Long Wave", true},}
		},
		{0x61,{
				{"Rlw-o.tpile.b","W/m^2","Outgoing Long Wave", true},
				{"Rlw-o.tcase.b","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome1.b","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome2.b","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome3.b","degC","Outgoing Long Wave", true},}
		},
		{0x62,{
				{"Rlw-o.tpile.c","W/m^2","Outgoing Long Wave", true},
				{"Rlw-o.tcase.c","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome1.c","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome2.c","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome3.c","degC","Outgoing Long Wave", true},}
		},
		{0x63,{
				{"Rlw-o.tpile.d","W/m^2","Outgoing Long Wave", true},
				{"Rlw-o.tcase.d","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome1.d","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome2.d","degC","Outgoing Long Wave", true},
				{"Rlw-o.tdome3.d","degC","Outgoing Long Wave", true},}
		},

		{0x64,{
				{"Riwkz.i.Rpile.a","W/m^2","Incoming Long Wave K&Z", true},
				{"Riwkz.i.tcase.a","degC","Incoming Long Wave K&Z", true},}
		},
		{0x65,{
				{"Riwkz.i.Rpile.b","W/m^2","Incoming Long Wave K&Z", true},
				{"Riwkz.i.tcase.b","degC","Incoming Long Wave K&Z", true},}
		},
		{0x66,{
				{"Riwkz.i.Rpile.c","W/m^2","Incoming Long Wave K&Z", true},
				{"Riwkz.i.tcase.c","degC","Incoming Long Wave K&Z", true},}
		},
		{0x67,{
				{"Riwkz.i.Rpile.d","W/m^2","Incoming Long Wave K&Z", true},
				{"Riwkz.i.tcase.d","degC","Incoming Long Wave K&Z", true},}
		},
		{0x68,{
				{"Riwkz.o.Rpile.a","W/m^2","Outgoing  Long Wave K&Z", true},
				{"Riwkz.o.tcase.a","degC","Outgoing  Long Wave K&Z", true},}
		},
		{0x69,{
				{"Riwkz.o.Rpile.b","W/m^2","Outgoing  Long Wave K&Z", true},
				{"Riwkz.o.tcase.b","degC","Outgoing  Long Wave K&Z", true},}
		},
		{0x6A,{
				{"Riwkz.o.Rpile.c","W/m^2","Outgoing  Long Wave K&Z", true},
				{"Riwkz.o.tcase.c","degC","Outgoing  Long Wave K&Z", true},}
		},
		{0x6B,{
				{"Riwkz.o.Rpile.d","W/m^2","Outgoing  Long Wave K&Z", true},
				{"Riwkz.o.tcase.d","degC","Outgoing  Long Wave K&Z", true},}
		},

		{0,{{},}}

};

