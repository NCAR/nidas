/*
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision:  $

    $LastChangedBy: dongl $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/WisardMote.h $

 */

#include "WisardMote.h"
#include <nidas/util/Logger.h>
#include <cmath>
#include <iostream>
#include <memory> // auto_ptr<>

using namespace nidas::dynld;
using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;
namespace n_u = nidas::util;


std::map<unsigned char, WisardMote::setFunc> WisardMote::nnMap;
static bool mapped = false;


NIDAS_CREATOR_FUNCTION_NS(isff,WisardMote)


WisardMote::WisardMote() {
	//static bool mapped = false;
	fromLittle = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN);

	initFuncMap();
}

bool WisardMote::process(const Sample* samp,list<const Sample*>& results) throw()
{
	/*  sample input --- there are multiple data-  */
	const unsigned char* cp= (const unsigned char*) samp->getConstVoidDataPtr();
	const unsigned char* eos = cp + samp->getDataByteLength();
	int len = samp->getDataByteLength();
	dsm_sample_id_t sampId= getId();

	n_u::Logger::getInstance()->log(LOG_INFO,"\n\n process  ttag= %d getId()= %d samp->getId()= %d  getDsmId()=%d", samp->getTimeTag(),getId(), samp->getId(), getDSMId());
	//print out raw-input data for debug
	n_u::Logger::getInstance()->log(LOG_INFO, "raw data = ");
	for (int i= 0; i<len; i++) n_u::Logger::getInstance()->log(LOG_INFO, " %x ", *(cp+i)); ////printf(" %x ", *(cp+i));

	/*  find EOM  */
	if (!findEOM(cp, samp->getDataByteLength())) return false;

	/*  verify crc for data  */
	if (!findCRC(cp, samp->getDataByteLength())) return false;

	/*  get header  -- return data header, ignore other headers */
	nname="";
	msgLen=0;
	if (!findHead(cp, eos, msgLen)) return false;

	/*  move cp point to process data   */
	cp +=msgLen;

	//printf("\n\n process  nname= %s" , nname.c_str());

	// crc+eom+0x0(1+3+1) + sensorTypeId+data (1+1 at least) = 7
	while ((cp+7) <= eos) {
		/*  get data one set data  */
		/* get sTypeId    */
		unsigned char sTypeId = *cp++;  msgLen++;
		/* push nodename+sStypeId to list  */
		n_u::Logger::getInstance()->log(LOG_INFO,"\n\n --SensorTypeId = %x ttag= %d ",sTypeId, samp->getTimeTag() );
		//pushNodeName(getId(), sTypeId);                     //getId()--get dsm and sensor +sample ids

		/* getData  */
		msgLen=0;
		data.clear();
		if ( nnMap[sTypeId]==NULL  ) {
			n_u::Logger::getInstance()->log(LOG_ERR, "\n process--getData--cannot find the setFunc. nname=%s sTypeId = %x ...   No data... ",lnname.c_str(), sTypeId);
			//printf( "\n process--getData--cannot find the setFunc. nname=%s sTypeId = %x ...   No data... ",lnname.c_str(), sTypeId);
			return false;
		}
		//printf("\n call nnMap[stypeId] type= %x ", sTypeId );
		(this->*nnMap[sTypeId])(cp,eos);

		/* move cp to right position */
		cp += msgLen;

		//printf(" \ndatasiez()= %d", data.size() );
		/*  output    */
		if (data.size() == 0) 	continue;

		SampleT<float>* osamp = getSample<float>(data.size());
		osamp->setTimeTag(samp->getTimeTag());
		osamp->setId(sampId);
		float* dout = osamp->getDataPtr();
		for (unsigned int i=0; i<data.size(); i++) {
			*dout++ = (float)data[i];
			//osamp->getDataPtr()[i] = (float)data[i];
			n_u::Logger::getInstance()->log(LOG_INFO, "\ndata= %f  idx= %i", data[i], i);
			//printf( "\ndata= %f  idx= %i", data[i], i);
		}
		/* push out   */
		results.push_back(osamp);
		//printf("\nsample-d= %d", sampId);
	    //printf("\n end of loop-- cp= %d cp+7=%d eod=%d type= %x \n",cp,  cp+7, eos, sTypeId);
	}
	return true;
}

void WisardMote::fromDOMElement(
		const xercesc::DOMElement* node)
throw(n_u::InvalidParameterException)
{

	DSMSensor::fromDOMElement(node);

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

void WisardMote::pushNodeName(unsigned int id, int sTypeId) {
	lnname = nname;
	lnname.push_back(',');
	char buffer [5];
	sprintf (buffer, "%x",sTypeId);
	lnname += buffer;
	remove(lnname.begin(), lnname.end(), ' ');
	n_u::Logger::getInstance()->log(LOG_INFO,"\n lnname= %s getId()= %i ", lnname.c_str(), id);

	unsigned int sampleId = nodeIds[lnname];
	n_u::Logger::getInstance()->log(LOG_INFO, "retrieved sample id= %d llname=%s", sampleId, lnname.c_str());
	if (sampleId == 0) {
		sampleId = id + sTypeId;
		n_u::Logger::getInstance()->log(LOG_INFO,"\n pushNodeName cannot find nodename,create one: lnname= %s sampleId= %i ", lnname.c_str(), sampleId);
		nodeIds[lnname] = sampleId;
	}
}

//void WisardMote::readData(const unsigned char* cp, const unsigned char* eos, vector<float>& data, int& msgLen)  {
void WisardMote::readData(const unsigned char* cp, const unsigned char* eos)  {

	/* get sTypeId    */
	int sTypeId = *cp++; msgLen++;
	//n_u::Logger::getInstance()->log(LOG_INF,"\n readData--SensorTypeId = %x \n",sTypeId);

	/* push nodename+sStypeId to list  */
	pushNodeName(getId(), sTypeId);                     //getId()--get dsm and sensor ids

	/* getData  */
	(this->*nnMap[sTypeId])(cp,eos);
}

/**
 * find NodeName, version, MsgType (0-log sensortype+SN 1-seq+time+data 2-err msg)
 */
bool WisardMote::findHead(const unsigned char* cp, const unsigned char* eos, int& msgLen) {
	n_u::Logger::getInstance()->log(LOG_INFO, "findHead...");
	/* look for nodeName */
	for ( ; cp < eos; cp++, msgLen++) {
		char c = *cp;
		if (c!= ':') nname.push_back(c);
		else break;
	}
	if (*cp != ':') return false;

	cp++; msgLen++; //skip ':'
	if (cp == eos) return false;

	// version number
	int ver = *cp; msgLen++;
	if (++cp == eos) return false;

	// message type
	int mtype = *cp++; msgLen++;
	if (cp == eos) return false;

	switch(mtype) {
	case 0:
		/* unpack 1bytesId + 16 bit s/n */
		if (cp + 1 + 2 > eos) return false;
		int sId;
		sId = *cp++; msgLen++;
		int sn;
		sn = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+= sizeof(uint16_t);
		n_u::Logger::getInstance()->log(LOG_INFO,"NodeName=%s Ver=%x MsgType=%x STypeId=%x SN=%x hmsgLen=%i",
				nname.c_str(), ver, mtype, sId, sn, msgLen);
		return false;
	case 1:
		/* unpack 1byte seq + 16-bit time */
		if (cp + 1+ sizeof(uint16_t) > eos) return false;
		unsigned char seq;
		seq = *cp++;  msgLen++;
		n_u::Logger::getInstance()->log(LOG_INFO,"NodeName=%s Ver=%x MsgType=%x seq=%x hmsgLen=%i",
				nname.c_str(), ver, mtype, seq, msgLen);
		//printf("\n NodeName=%s Ver=%x MsgType=%x seq=%x hmsgLen=%i",
		//nname.c_str(), ver, mtype, seq, msgLen);
		break;
	case 2:
		n_u::Logger::getInstance()->log(LOG_INFO,"NodeName=%s Ver=%x MsgType=%x hmsgLen=%i ErrMsg=%s",
				nname.c_str(), ver, mtype, msgLen, cp);
		return false;//skip for now

	default:
		n_u::Logger::getInstance()->log(LOG_ERR,"Unknown msgType --- NodeName=%s Ver=%x MsgType=%x hmsgLen=%i",
				nname.c_str(), ver, mtype, msgLen);
		return false;
	}

	return true;
}

/**
 * EOM (0x03 0x04 0xd) + 0x0
 */
bool WisardMote::findEOM(const unsigned char* cp, unsigned char len) {
	n_u::Logger::getInstance()->log(LOG_INFO, "findEOM len= %d ",len);

	if (len< 4 ) {
		n_u::Logger::getInstance()->log(LOG_ERR,"Message length is too short --- len= %d", len );
		return false;
	}

	unsigned char lidx =len-1;
	if (*(cp+lidx)!= 0x0 ||*(cp+lidx-1)!= 0xd ||*(cp+lidx-2)!= 0x4 ||*(cp+lidx-3)!= 0x3 ) {
		n_u::Logger::getInstance()->log(LOG_ERR,"Bad EOM --- last 4 chars= %x %x %x %x ", *(cp+lidx-3), *(cp+lidx-2), *(cp+lidx-1), *(cp+lidx) );
		return false;
	}
	return true;
}


bool WisardMote::findCRC (const unsigned char* cp, unsigned char len) {
	unsigned char lidx =len-1;

	// retrieve CRC-- 3byteEOM  + 1byte0x0
	unsigned char crc= *(cp+lidx-4);

	//calculate Cksum
	unsigned char cksum = len - 5;  //skip CRC+EOM+0x0
	for(int i=0; i< (len-5); i++) {
		unsigned char c =*cp++;
		////printf("crc-cal-char= %x \n", c);
		cksum ^= c ;
	}

	if (cksum != crc ) {
		n_u::Logger::getInstance()->log(LOG_ERR,"Bad CKSUM --- %x vs  %x ", crc, cksum );
		return false;
	}
	return true;
}


void WisardMote::setPicTm(const unsigned char* cp, const unsigned char* eos){
	/* unpack  16 bit pic-time */
	if (cp + sizeof(uint16_t) > eos) return;
	int	val;
	val=  (fromLittle->uint16Value(cp));
	cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
	if (val!= 0x8000)
		data.push_back(val/10.0);
	else
		data.push_back(floatNAN);

	n_u::Logger::getInstance()->log(LOG_INFO,"\nPic_time = %d ",  val);
}



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
	n_u::Logger::getInstance()->log(LOG_INFO,"\ntotal-time-ticks in sec = %d ",  val);
}



void WisardMote::setTmCnt(const unsigned char* cp, const unsigned char* eos){
	/* unpack  32 bit  tm-count in  */
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_INFO,"\ntime-count = %d ",  val);
}


void WisardMote::setTm10thSec(const unsigned char* cp, const unsigned char* eos){
	/* unpack  32 bit  t-tm-ticks in 10th sec */
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val/10);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_INFO,"\ntotal-time-ticks-10th sec = %d ",  val);
}


void WisardMote::setTm100thSec(const unsigned char* cp, const unsigned char* eos){
	/* unpack  32 bit  t-tm-100th in sec */
	if (cp + sizeof(uint32_t) > eos) return;
	int	val;
	val=  (fromLittle->uint32Value(cp));
	cp += sizeof(uint32_t); msgLen+=sizeof(uint32_t);
	if (val!= 0x8000)
		data.push_back(val/100);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_INFO,"\ntotal-time-ticks in 100th sec = %d ",  val);
}

void WisardMote::setPicDT(const unsigned char* cp, const unsigned char* eos){
	/*  16 bit jday */
	if (cp + sizeof(uint16_t) > eos) return;
	int jday;
	jday= (fromLittle->uint16Value(cp));
	cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
	if (jday!= 0x8000)
		data.push_back(jday);
	else
		data.push_back(floatNAN);
	/*  8 bit hour+ 8 bit min+ 8 bit sec  */
	if ((cp + 3* sizeof(uint8_t)) > eos) return;
	int hh,mm,ss;
	hh= *cp;
	cp += sizeof(uint8_t); msgLen+=sizeof(uint8_t);
	if (hh!= 0x8000)
		data.push_back(hh);
	else
		data.push_back(floatNAN);
	mm= *cp;
	cp += sizeof(uint8_t); msgLen+=sizeof(uint8_t);
	if (mm!= 0x8000)
		data.push_back(mm);
	else
		data.push_back(floatNAN);
	ss= *cp;
	cp += sizeof(uint8_t); msgLen+=sizeof(uint8_t);
	if (ss!= 0x8000)
		data.push_back(ss);
	else
		data.push_back(floatNAN);
	//printf("\n jday= %x hh=%x mm=%x ss=%d",  jday, hh, mm, ss);
	n_u::Logger::getInstance()->log(LOG_INFO,"\n jday= %d hh=%d mm=%d ss=%d",  jday, hh, mm, ss);
}

void WisardMote::setTsoilData(const unsigned char* cp, const unsigned char* eos){
	/* unpack 16 bit  */
	for (int i=0; i<4; i++) {
		if (cp + sizeof(int16_t) > eos) return;
		int val = fromLittle->int16Value(cp);
		if (val!= 0x8000)
			data.push_back(val/10.0);
		else
			data.push_back(floatNAN);
		cp += sizeof(int16_t); msgLen+=sizeof(int16_t);
	}
}
void WisardMote::setGsoilData(const unsigned char* cp, const unsigned char* eos){
	if (cp + sizeof(int16_t) > eos) return;
	int val = fromLittle->int16Value(cp);
	if (val!= 0x8000)
		data.push_back(val/1.0);
	else
		data.push_back(floatNAN);

	//data.push_back((fromLittle->int16Value(cp))/1.0);
	cp += sizeof(int16_t); msgLen+=sizeof(int16_t);
}
void WisardMote::setQsoilData(const unsigned char* cp, const unsigned char* eos){
	if (cp + sizeof(uint16_t) > eos) return;
	int val = fromLittle->uint16Value(cp);
	if (val!= 0x8000)
		data.push_back(val/1.0);
	else
		data.push_back(floatNAN);
	//data.push_back((fromLittle->uint16Value(cp))/1.0);
	cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
}
void WisardMote::setTP01Data(const unsigned char* cp, const unsigned char* eos){
	// 3 are singed
	for (int i=0; i<3; i++) {
		if (cp + sizeof(int16_t) > eos) return;
		int val = fromLittle->int16Value(cp);
		if (val!= 0x8000)
			data.push_back(val/1.0);
		else
			data.push_back(floatNAN);
		//data.push_back((fromLittle->int16Value(cp))/1.0);
		cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
	}
}

void WisardMote::setRnetData(const unsigned char* cp, const unsigned char* eos){
	if (cp + sizeof(int16_t) > eos) return;
	int val = fromLittle->int16Value(cp);
	if (val!= 0x8000)
		data.push_back(val/10.0);
	else
		data.push_back(floatNAN);
	//data.push_back((fromLittle->int16Value(cp))/10.0);
	cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
}
void WisardMote::setRswData(const unsigned char* cp, const unsigned char* eos){
	if (cp + sizeof(uint16_t) > eos) return;
	int val = fromLittle->uint16Value(cp);
	if (val!= 0x8000)
		data.push_back(val/10.0);
	else
		data.push_back(floatNAN);
	//data.push_back((fromLittle->uint16Value(cp))/10.0);
	cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
}
void WisardMote::setRlwData(const unsigned char* cp, const unsigned char* eos){
	for (int i=0; i<5; i++) {
		if (cp + sizeof(int16_t) > eos) return;
		int val = fromLittle->int16Value(cp);
		cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);

		if (val!= 0x8000)
			data.push_back(val/10.0);
		else
			data.push_back(floatNAN);
	}
}

void WisardMote::setStatusData(const unsigned char* cp, const unsigned char* eos){

	if (cp + 1 > eos) return;
	int val = *cp++; msgLen++;

	if (val!= 0x8000)
		data.push_back(val/1.0);
	else
		data.push_back(floatNAN);
	n_u::Logger::getInstance()->log(LOG_INFO,"\nStatus= %d ",  val);

}

void WisardMote::setPwrData(const unsigned char* cp, const unsigned char* eos){
	n_u::Logger::getInstance()->log(LOG_INFO,"\nPower Data= ");
	for (int i=0; i<6; i++){
		if (cp + sizeof(uint16_t) > eos) return;
		int val = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);

		if (val!= 0x8000) {
			if (i==0 || i==3) 	data.push_back(val/10.0);  //voltage 10th
			else data.push_back(val/1.0);
		} else
			data.push_back(floatNAN);
		n_u::Logger::getInstance()->log(LOG_INFO," %d ", val);
	}
}

void WisardMote::initFuncMap() {
	if (! mapped) {
		WisardMote::nnMap[0x01] = &WisardMote::setPicTm;
		WisardMote::nnMap[0x0B] = &WisardMote::setTmSec;
		WisardMote::nnMap[0x0C] = &WisardMote::setTmCnt;
		WisardMote::nnMap[0x0D] = &WisardMote::setTm100thSec;
		WisardMote::nnMap[0x0E] = &WisardMote::setTm10thSec;
		WisardMote::nnMap[0x0f] = &WisardMote::setPicDT;

		WisardMote::nnMap[0x20] = &WisardMote::setTsoilData;
		WisardMote::nnMap[0x21] = &WisardMote::setTsoilData;
		WisardMote::nnMap[0x22] = &WisardMote::setTsoilData;
		WisardMote::nnMap[0x23] = &WisardMote::setTsoilData;

		WisardMote::nnMap[0x24] = &WisardMote::setGsoilData;
		WisardMote::nnMap[0x25] = &WisardMote::setGsoilData;
		WisardMote::nnMap[0x26] = &WisardMote::setGsoilData;
		WisardMote::nnMap[0x27] = &WisardMote::setGsoilData;

		WisardMote::nnMap[0x28] = &WisardMote::setQsoilData;
		WisardMote::nnMap[0x29] = &WisardMote::setQsoilData;
		WisardMote::nnMap[0x2A] = &WisardMote::setQsoilData;
		WisardMote::nnMap[0x2B] = &WisardMote::setQsoilData;

		WisardMote::nnMap[0x2C] = &WisardMote::setTP01Data;
		WisardMote::nnMap[0x2D] = &WisardMote::setTP01Data;
		WisardMote::nnMap[0x2E] = &WisardMote::setTP01Data;
		WisardMote::nnMap[0x2F] = &WisardMote::setTP01Data;

		WisardMote::nnMap[0x40] = &WisardMote::setStatusData;
		WisardMote::nnMap[0x49] = &WisardMote::setPwrData;

		WisardMote::nnMap[0x50] = &WisardMote::setRnetData;
		WisardMote::nnMap[0x51] = &WisardMote::setRnetData;
		WisardMote::nnMap[0x52] = &WisardMote::setRnetData;
		WisardMote::nnMap[0x53] = &WisardMote::setRnetData;

		WisardMote::nnMap[0x54] = &WisardMote::setRswData;
		WisardMote::nnMap[0x55] = &WisardMote::setRswData;
		WisardMote::nnMap[0x56] = &WisardMote::setRswData;
		WisardMote::nnMap[0x57] = &WisardMote::setRswData;

		WisardMote::nnMap[0x58] = &WisardMote::setRswData;
		WisardMote::nnMap[0x59] = &WisardMote::setRswData;
		WisardMote::nnMap[0x5A] = &WisardMote::setRswData;
		WisardMote::nnMap[0x5B] = &WisardMote::setRswData;

		WisardMote::nnMap[0x5C] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x5D] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x5E] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x5F] = &WisardMote::setRlwData;

		WisardMote::nnMap[0x60] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x61] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x62] = &WisardMote::setRlwData;
		WisardMote::nnMap[0x63] = &WisardMote::setRlwData;
		mapped = true;
	}
}
