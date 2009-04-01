/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMEngine.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;

int main(int argc, char** argv)
{
    int res = DSMEngine::main(argc,argv);	// deceptively simple
    ILOG(("dsm exiting, status=") << res);
    return res;
}
