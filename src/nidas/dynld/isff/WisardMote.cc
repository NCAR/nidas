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

NIDAS_CREATOR_FUNCTION_NS(isff,WisardMote)

bool WisardMote::process(const Sample* samp,list<const Sample*>& results) throw()
{
    /* sample input --- there are multiple data-msgs  */
    const unsigned char* cp = (const unsigned char*) samp->getConstVoidDataPtr();
    const unsigned char* eos = cp + samp->getDataByteLength();

    while(cp < eos) {
	string nname=""; int msgLen=0;
        bool ret=findHead(cp, eos, nname, msgLen);
	if (!ret) return false;
	if (cp == eos) return false;

	/*  get data  */
	vector<float> data;
	readData(cp, eos, data, msgLen);
	if (data.size() == 0) 	return false;

	/*  output    */
        SampleT<float>* osamp = getSample<float>(data.size());
        osamp->setTimeTag(samp->getTimeTag());
	osamp->setId((dsm_sample_id_t)nodeIds[nname]);
	for (unsigned int i=0; i<data.size(); i++) {
		osamp->getDataPtr()[i] = data[i];
	}

        /* push out   */
	results.push_back(osamp);
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

void WisardMote::pushNodeName(unsigned int id, string nodeName) {
	unsigned int sampleId = nodeIds[nodeName];
	if (sampleId == 0) {
		nodeNum++;
		sampleId = id + nodeNum;
		nodeIds[nodeName] = sampleId;
	}
}

void WisardMote::readData(const unsigned char* cp, const unsigned char* eos, vector<float>& data, int& msgLen)  {
	/* get varId    */
	int varId = *cp++; msgLen++;

	switch(varId) {
	case 0x01:
		/*  16 bit time */
		if (cp + sizeof(uint16_t) > eos) return;
		data.push_back( (fromLittle->uint16Value(cp))/10.0 );
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		break;

	case 0x20:
		/* unpack 16 bit time */
		for (int i=0; i<4; i++) {
			if (cp + sizeof(int16_t) > eos) return;
			data.push_back((fromLittle->int16Value(cp))/10.0);
			cp += sizeof(int16_t); msgLen+=sizeof(int16_t);
		}
		break;

	case 0x21:
		if (cp + sizeof(int16_t) > eos) return;
		data.push_back((fromLittle->int16Value(cp))/1.0);
		cp += sizeof(int16_t); msgLen+=sizeof(int16_t);
		break;

	case 0x22:
		if (cp + sizeof(uint16_t) > eos) return;
		data.push_back((fromLittle->uint16Value(cp))/1.0);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		break;
	case 0x23:
		//first 2 are singed, the 3rd unsigned
		for (int i=0; i<2; i++) {
			if (cp + sizeof(int16_t) > eos) return;
			data.push_back((fromLittle->int16Value(cp))/1.0);
			cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
		}
		if (cp + sizeof(uint16_t) > eos) return;
		data.push_back((fromLittle->uint16Value(cp))/1.0);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		break;

	case 0x30:
		if (cp + sizeof(int16_t) > eos) return;
		data.push_back((fromLittle->int16Value(cp))/10.0);
		cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
		break;

	case 0x31:
		if (cp + sizeof(uint16_t) > eos) return;
		data.push_back((fromLittle->uint16Value(cp))/10.0);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		break;
	case 0x32:
		if (cp + sizeof(uint16_t) > eos) return;
		data.push_back((fromLittle->uint16Value(cp))/10.0);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		break;
	case 0x33:
		for (int i=0; i<5; i++) {
			if (cp + sizeof(int16_t) > eos) return;
			data.push_back((fromLittle->int16Value(cp))/10.0);
			cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
		}
		break;
	case 0x34:
		for (int i=0; i<5; i++) {
			if (cp + sizeof(int16_t) > eos) return;
			data.push_back((fromLittle->int16Value(cp))/10.0);
			cp += sizeof(int16_t); msgLen+=sizeof(uint16_t);
		}
		break;

	case 0x49:
		for (int i=0; i<6; i++){
			if (cp + sizeof(uint16_t) > eos) return;
			data.push_back((fromLittle->uint16Value(cp))/10.0);
			cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		}
		break;
        default: 
                n_u::Logger::getInstance()->log(LOG_INFO,"Unknown VarType = %x ",  varId);
                data.clear();
                return;
	}

        /* calculate crc  */
        n_u::Logger::getInstance()->log(LOG_INFO,"Tol-msgLen= %i",  msgLen);
        unsigned char cksum = msgLen;
        for (int i=msgLen; i<=1; i--) {
            cksum ^= *(cp-i);
        }

        /* retrieve crc and EOM*/
        unsigned char crc = *cp++; //keep crc  -- assume mshType=2 err msg is not here
        if (cp+4 > eos ||(*cp++ !=0x03)||(*cp++ !=0x04 )||(*cp++ !=0x0d)||(*cp++ !=0x0a)) {
              n_u::Logger::getInstance()->log(LOG_ERR,"Message is not ended with EOM ! ");
        }
        
        //n_u::Logger::getInstance()->log(LOG_ERR,"expected crc = %x cal-crc= %x",  crc, cksum);
        printf("\n\n\n expected crc = %x cal-crc= %x",  crc, cksum);
        if (crc != cksum ) {
            n_u::Logger::getInstance()->log(LOG_ERR," CRC Err --- expected = %x real= %x ",  crc, cksum);
            data.clear();
        }
}

bool WisardMote::findHead(const unsigned char* cp, const unsigned char* eos, string& nname, int& msgLen) {
	/* look for nodeName */
	for ( ; cp < eos; cp++, msgLen++) {
		char c = *cp; 
		if (c!= ':') nname.push_back(c);
		else break;
	}
	if (*cp != ':') return false;

	//push nodename to list
	pushNodeName(getId(), nname); //getId()--get dsm and sensor ids

	cp++; msgLen++; //skip ':'
	if (cp == eos) return false;

	// sequence number
	int seq = *cp; msgLen++;
	if (++cp == eos) return false;

	// message type
	int mtype = *cp; msgLen++;
	if (++cp == eos) return false;

	int time;
	int sId=-1;

	switch(mtype) {
	case 0:
		/* unpack 1bytesId + 16 bit s/n */
		if (cp + 1 + sizeof(uint16_t) > eos) return false;
		sId = *cp++; msgLen++;
		int sn;
		sn = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		n_u::Logger::getInstance()->log(LOG_INFO,"NodeName= %s Sequence= %x MsgType= %x SN= %x hmsgLen= %i",
				nname.c_str(), seq, mtype, sn, msgLen);
		return false;
	case 1:
		/* unpack 16 bit time */
		if (cp + sizeof(uint16_t) > eos) return false;
		time = fromLittle->uint16Value(cp);
		cp += sizeof(uint16_t); msgLen+=sizeof(uint16_t);
		n_u::Logger::getInstance()->log(LOG_INFO,"NodeName= %s Sequence= %x MsgType= %x time= %i hmsgLen= %i",
				nname.c_str(), seq, mtype, time, msgLen);
		break;
	case 2:
		n_u::Logger::getInstance()->log(LOG_ERR,"NodeName= %s Sequence= %x MsgType= %x hmsgLen= %i ErrMsg= %s",
				nname.c_str(), seq, mtype, msgLen, cp);
		return false;//skip for now
	default:
		n_u::Logger::getInstance()->log(LOG_ERR,"Unknown msgType --- NodeName= %s Sequence= %x MsgType= %x hmsgLen= %i",
				nname.c_str(), seq, mtype, msgLen);
		return false;
	}
	return true;
}


