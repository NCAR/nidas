/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Aircraft.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(Aircraft)

Aircraft::Aircraft()
{
    allowedParameterNames.push_back("tailNumber");
}

Aircraft::~Aircraft()
{
}

/*
 * An example of an Aircraft attribute that is maintained as a
 * Site parameter.
 */
string Aircraft::getTailNumber() const {
    const Parameter* tail = getParameter("tailNumber");
    if (tail && tail->getLength() > 0) return tail->getStringValue(0);
    else return "unknown";
}

void Aircraft::setTailNumber(const string& val) {
    ParameterT<string>* param = new ParameterT<string>;
    param->setName("tailNumber");
    param->setValue(val);
    addParameter(param);
}

