
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/TestSampleClient.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

bool TestSampleClient::receive(const Sample *s) throw()
{
    cerr << dec << "timetag= " << s->getTimeTag() << " id= " << s->getId() <<
    	" len=" << s->getDataLength() << endl;
    return true;
}
