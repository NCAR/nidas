// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/raf/Aircraft.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(raf,Aircraft)

Aircraft::Aircraft()
{
    _allowedParameterNames.push_back("tailNumber");
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

