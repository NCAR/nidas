
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
	WisardMote():fromLittle ( n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_LITTLE_ENDIAN)), nodeNum(0)
	{};
	virtual ~WisardMote() {};


	bool process(const Sample* insamp,std::list<const Sample*>& results) throw() ;

	void fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException);

private:
	const n_u::EndianConverter* fromLittle;
	map<string,unsigned int> nodeIds;
	int nodeNum;

	/** push a pair of nodename and id to the map
	 *  @param id  	--  id=h16dsm  l16 sensor  (id+ sampleId = nidas complex id)
	 *  @param nm	--  nm=node name
	 */
	void pushNodeName(unsigned int id, string nm);

	/**
	 * cases of variable name and data
	 *
	 */
	void readData(const unsigned char* cp, const unsigned char* eos, vector<float>& data); // std::out_of_range ;

};
}}} // nidas::dynld::isff
#endif /* WISARDMOTE_H_ */

