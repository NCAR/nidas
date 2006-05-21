/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-03 14:24:50 -0700 (Fri, 03 Feb 2006) $

    $LastChangedRevision: 3262 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/CSAT3_Sonic.cc $

*/

#include <CS_Krypton.h>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(CS_Krypton)

CS_Krypton::CS_Krypton()
{
    // readonable defaults
    setPathLength(1.3);
    setKw(-0.150);
    setV0(5000);
    setBias(0.0);
}

CS_Krypton* CS_Krypton::clone() const
{
    return new CS_Krypton(*this);
}

std::string CS_Krypton::toString() const
{
    return "CS_Krypton::toString() unsupported";
}

void CS_Krypton::fromString(const std::string&) 
    	throw(atdUtil::InvalidParameterException)
{
    throw atdUtil::InvalidParameterException(
    	"CS_Krypton::fromString() not supported yet");
}

float CS_Krypton::convert(float volts) const
{
    // convert to millivolts
    volts *= 1000.0;

    if (volts < 0.1) volts = 0.1;

    float h2o = (::log(volts) - logV0) / pathLengthKw - bias;
    if (h2o < 0.0) h2o = 0.0;
    return h2o;
}
    	

