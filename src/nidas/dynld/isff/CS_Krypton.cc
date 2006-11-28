/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-03 14:24:50 -0700 (Fri, 03 Feb 2006) $

    $LastChangedRevision: 3262 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/CSAT3_Sonic.cc $

*/

#include <nidas/dynld/isff/CS_Krypton.h>
#include <nidas/core/CalFile.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CS_Krypton)

CS_Krypton::CS_Krypton(): calFile(0),calTime(0)
{
    // readonable defaults
    setPathLength(1.3);
    setKw(-0.150);
    setV0(5000);
    setBias(0.0);
}

CS_Krypton::CS_Krypton(const CS_Krypton& x): calFile(0),calTime(0)
{
    if (x.calFile) calFile = new CalFile(*x.calFile);
}

CS_Krypton* CS_Krypton::clone() const
{
    return new CS_Krypton(*this);
}

CS_Krypton::~CS_Krypton()
{
    delete calFile;
}

void CS_Krypton::setCalFile(CalFile* val)
{
    calFile = val;
}

std::string CS_Krypton::toString() const
{
    return "CS_Krypton::toString() unsupported";
}

void CS_Krypton::fromString(const std::string&) 
    	throw(n_u::InvalidParameterException)
{
    throw n_u::InvalidParameterException(
    	"CS_Krypton::fromString() not supported yet");
}

float CS_Krypton::convert(dsm_time_t t,float volts)
{
    if (calFile) {
        while(t >= calTime) {
            float d[5];
            try {
                int n = calFile->readData(d,sizeof d/sizeof(d[0]));
                if (n > 3) {
                    setKw(d[0]);
                    setV0(d[1]);
                    setPathLength(d[2]);
                    setBias(d[3]);
                }
                calTime = calFile->readTime().toUsecs();
            }
            catch(const n_u::EOFException& e) {}
            catch(const n_u::IOException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setPathLength(floatNAN);
                setBias(floatNAN);
                calTime = LONG_LONG_MAX;
            }
            catch(const n_u::ParseException& e)
            {
                n_u::Logger::getInstance()->log(LOG_WARNING,"%s: %s",
                    calFile->getName().c_str(),e.what());
                setKw(floatNAN);
                setV0(floatNAN);
                setBias(floatNAN);
                setPathLength(floatNAN);
                calTime = LONG_LONG_MAX;
            }
        }
    }
    // convert to millivolts
    volts *= 1000.0;

    if (volts < 0.1) volts = 0.1;

    float h2o = (::log(volts) - logV0) / pathLengthKw - bias;
    if (h2o < 0.0) h2o = 0.0;
    return h2o;
}
    	

void CS_Krypton::fromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{

    VariableConverter::fromDOMElement(node);

    static struct ParamSet {
        const char* name;	// parameter name
        void (CS_Krypton::* setFunc)(float);
        			// ptr to setXXX member function
				// for setting parameter.
    } paramSet[] = {
	{ "Kw",		&CS_Krypton::setKw },
	{ "V0",		&CS_Krypton::setV0 },
	{ "PathLength",	&CS_Krypton::setPathLength },
	{ "Bias",	&CS_Krypton::setBias },
    };

    for (unsigned int i = 0; i < sizeof(paramSet) / sizeof(paramSet[0]); i++) {
	const Parameter* param = getParameter(paramSet[i].name);
	if (!param) continue;
	if (param->getLength() != 1) 
	    throw n_u::InvalidParameterException("CS_Krypton",
		"parameter", string("bad length for ") + paramSet[i].name);
	// invoke setXXX member function
	(this->*paramSet[i].setFunc)(param->getNumericValue(0));
    }
}
