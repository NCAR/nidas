
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
#include <nidas/util/InvalidParameterException.h>

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


struct VarInfo
{
	const char* name;
	const char* units;
	const char* longname;
};

struct SampInfo
{
    unsigned int id;
    struct VarInfo variables[5];
};


class WisardMote: public DSMSerialSensor {
public:
	WisardMote();
	virtual ~WisardMote() {};


	bool process(const Sample* insamp,  list<const Sample*>& results) throw() ;

	void fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException);

	typedef void(WisardMote::*setFunc)(const unsigned char* cp, const unsigned char* eos);

	static SampInfo samps[];

private:
	const n_u::EndianConverter* fromLittle;

	/*  data and len */
	vector<float> data;	int msgLen;

	string nname;// nname keeps "height,location-> IDXXX", lnname= nname+senosrtypeId
	int sampleId; //IDXXX: sampleId= XXX<<8;

	/**
	 * overwrite addSampleTag
	 * Add a sample to this sensor.
	 * Throw an exception the DSMSensor cannot support
	 * the sample (bad rate, wrong number of variables, etc).
	 * DSMSensor will own the pointer.
	 * Note that a SampleTag may be changed after it has
	 * been added. addSampleTag() is called when a sensor is initialized
	 * from the sensor catalog.  The SampleTag may be modified later
	 * if it is overridden in the actual sensor entry.
	 * For this reason, it is probably better to scan the SampleTags
	 * of a DSMSensor in the validate(), init() or open() methods.
	 */
	void addSampleTag(SampleTag* val) throw(nidas::util::InvalidParameterException);


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
	void setTmSec(const unsigned char* cp, const unsigned char* eos);
	void setTmCnt(const unsigned char* cp, const unsigned char* eos);
	void setTm100thSec(const unsigned char* cp, const unsigned char* eos);
	void setTm10thSec(const unsigned char* cp, const unsigned char* eos);
	void setPicDT(const unsigned char* cp, const unsigned char* eos);

	void setTsoilData(const unsigned char* cp, const unsigned char* eos);
	void setGsoilData(const unsigned char* cp, const unsigned char* eos);
	void setQsoilData(const unsigned char* cp, const unsigned char* eos);
	void setTP01Data(const unsigned char* cp, const unsigned char* eos);

	void setRnetData(const unsigned char* cp, const unsigned char* eos);
	void setRswData(const unsigned char* cp, const unsigned char* eos);
	void setRlwData(const unsigned char* cp, const unsigned char* eos);

	void setStatusData(const unsigned char* cp, const unsigned char* eos);
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

