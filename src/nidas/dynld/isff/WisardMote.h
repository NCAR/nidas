
/*
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision:  $

    $LastChangedBy: dongl $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/WisardMote.h $

 */


#ifndef NIDAS_DYNLD_ISFF_WISARDMOTE_H
#define NIDAS_DYNLD_ISFF_WISARDMOTE_H

#include <nidas/core/DSMSensor.h>
#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <iostream>
#include <exception>

//#include <sstream>
#include <list>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;
using namespace nidas::util;
using namespace nidas::dynld;
using namespace std;
namespace n_u=nidas::util;

class WisardMote: public DSMSerialSensor {
public:
	WisardMote();
	virtual ~WisardMote() {};


	bool process(const Sample* insamp,std::list<const Sample*>& results) throw() ;

	void fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException);

	typedef void(WisardMote::*setFunc)(const unsigned char* cp, const unsigned char* eos);


private:
	const n_u::EndianConverter* fromLittle;

	/*  data and len */
	vector<float> data;	int msgLen;

	/*  nodeName and Id pairs  */
	map<string,unsigned int> nodeIds;
	string nname, lnname;   // nname keeps "height,location", lnname= nname+senosrtypeId


	/** push a pair of nodename and id to the map
	 *  @param id  	--  id=h16dsm  l16 sensor  (id+ sampleId = nidas complex id)
	 *  @param sensorTypeId	-- sensorTypeId
	 */
	void pushNodeName(unsigned int id, int sensorTypeId );

	/**
	 * cases of variable name and data
	 *
	 */
	//void readData(const unsigned char* cp, const unsigned char* eos, vector<float>& data, int& msgLen); // std::out_of_range ;
	void readData(const unsigned char* cp, const unsigned char* eos); // std::out_of_range ;

	/**
	 * find ID#, :, seq#, and msgType
	 */

	bool findHead(const unsigned char* cp, const unsigned char* eos, int& msgLen);

	/**
	 * check EOM
	 */
	bool findEOM(const unsigned char* cp, unsigned char len);

	/**
	 * verify crc
	 */
	bool findCRC (const unsigned char* cp, unsigned char len);

	/* claim methods to retrieve sensorType data    */
	void setPicTm(const unsigned char* cp, const unsigned char* eos);
	void setPicDT(const unsigned char* cp, const unsigned char* eos);

	void setTsoilData(const unsigned char* cp, const unsigned char* eos);
	void setGsoilData(const unsigned char* cp, const unsigned char* eos);
	void setQsoilData(const unsigned char* cp, const unsigned char* eos);
	void setTP01Data(const unsigned char* cp, const unsigned char* eos);

	void setRnetData(const unsigned char* cp, const unsigned char* eos);
	void setRswData(const unsigned char* cp, const unsigned char* eos);
	void setRlwData(const unsigned char* cp, const unsigned char* eos);

	void setPwrData(const unsigned char* cp, const unsigned char* eos);

	/*  stypeId to func ptr */
	/**
	 *  setFun is a function pointer that points to one of the setTsoilData, setGsoilData, etc.
	 *  I is mapped to its related function based on the sensor type id.
	 *  For example, Tsiol data can be the sensor ids (0x20 -0x23), it is mapped to  WisardMote::nnMap[0x20] = &WisardMote::setTsoilData, etc.
	 *  The initFuncMap is used to initialize the function with its sensor type id.
	 */
    public:	static std::map<unsigned char, setFunc> nnMap;
    static void initFuncMap();


};
}}} // nidas::dynld::isff
#endif /* WISARDMOTE_H_ */

