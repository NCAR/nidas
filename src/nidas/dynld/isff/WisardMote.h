
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
#include <nidas/core/DSMTime.h>
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

using namespace nidas::core;
using namespace nidas::dynld;

struct VarInfo
{
	const char* name;
	const char* units;
	const char* longname;
	bool 		dynamic;
};

struct SampInfo
{
	unsigned int id;
	struct VarInfo variables[6];
};



class WisardMote: public DSMSerialSensor {
public:
	static const  short missValueSigned = (signed)0x8000;
	static const  unsigned short missValue = (unsigned)0x8000;
	static const  unsigned char missByteValue = 0x80;
	//	static const  unsigned int miss4byteValue = 0x80000000;

	WisardMote();

	virtual ~WisardMote() {};

	bool process(const Sample* insamp,  list<const Sample*>& results) throw() ;

	typedef const unsigned char*(WisardMote::*readFunc)(const unsigned char* cp, const unsigned char* eos, dsm_time_t ttag_msec);

private:
	static const nidas::util::EndianConverter* _fromLittle;

	/**
	 * Mote id, read from initial digits in message, up to colon.
	 * For example the number XX from "IDXX:"
	 */
	int _moteId;

	/**
	 * Sensor serial numbers, from message.
	 */
	std::map<int,int> _sensorSerialNumbersByType;

	/**
	 * Version number of current message.
	 */
	int _version;

	int _sequence;

	int _badCRCs;

	/**
	 * data unpacked from message.
	 */
	vector<float> _data;

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
	 * Check for correct EOM. Return pointer to the beginning of the eom,
	 * (which is one past the CRC) or NULL if a correct EOM is not found.
	 */
	const unsigned char* checkEOM(const unsigned char* cp, const unsigned char* eom);

	/**
	 * Verify crc. Return pointer to the CRC (which is one past the end of the data),
	 * or NULL if a correct CRC is not found.
	 */
	const unsigned char* checkCRC (const unsigned char* cp, const unsigned char* eom);

	/**
	 * Read mote id, find ID#, :, seq#, and msgType. Return msgType.
	 */
	int readHead(const unsigned char* &cp, const unsigned char* eom);

	/* claim methods to retrieve sensorType data    */
	const unsigned char* readPicTm(const unsigned char* cp, const unsigned char* eos, dsm_time_t ttag);
	const unsigned char* readGenShort(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readGenLong(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);

	const unsigned char* readTmCnt(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readTmSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readTm100thSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readTm10thSec(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readPicDT(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);

	const unsigned char* readTsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readGsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readQsoilData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readTP01Data(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);

	const unsigned char* readRnetData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readRswData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readRlwData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readRlwKZData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);

	const unsigned char* readStatusData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);
	const unsigned char* readPwrData(const unsigned char* cp, const unsigned char* eos,  dsm_time_t ttag);

	static SampInfo _samps[];

	static bool _functionsMapped;

	static std::map<unsigned char, WisardMote::readFunc> _nnMap;

	static void initFuncMap();
};
}}} // nidas::dynld::isff
#endif /* WISARDMOTE_H_ */

