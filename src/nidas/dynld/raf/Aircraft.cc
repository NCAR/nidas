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
#include <nidas/core/Project.h>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

NIDAS_CREATOR_FUNCTION_NS(raf,Aircraft)

Aircraft::Aircraft()
{
    /*
     * Do we want DSMSensor::process methods at this site to apply
     * variable conversions? This can be changed with the
     * "applyCals" boolean parameter in the XML.  Let it default
     * to false, the original behaviour, until things are settled 
     * in this area.
     */
    _applyCals = false;
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


Aircraft*
Aircraft::
getAircraft(Project* project)
{
    const std::list<Site*>& sites = project->getSites();
    std::list<Site*>::const_iterator si;
  
    Aircraft* aircraft = 0;
    for (si = sites.begin(); !aircraft && si != sites.end(); ++si)
    {
        aircraft = dynamic_cast<Aircraft*>(*si);
    }
    return aircraft;
}
