/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-25 12:22:19 -0600 (Tue, 25 Oct 2005) $

    $LastChangedRevision: 3073 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Aircraft.cc $
 ********************************************************************

*/

#include <GroundStation.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(GroundStation)

GroundStation::GroundStation():number(-1)
{
    allowedParameterNames.push_back("number");
}

GroundStation::~GroundStation()
{
}

/*
 * An example of an GroundStation attribute that is maintained as a
 * Site parameter.
 */
int GroundStation::getNumber() const
{
    if (number >= 0) return number;
    const Parameter* tail = getParameter("number");
    if (tail && tail->getLength() > 0)
    	number= (int)tail->getNumericValue(0);
    return number;
}

void GroundStation::setNumber(int val)
{
    number = val;
    ParameterT<int>* param = new ParameterT<int>;
    param->setName("number");
    param->setValue(val);
    addParameter(param);
}

