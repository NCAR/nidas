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

#include <iostream>

using namespace std;

using namespace nidas::core;

int main(int argc, char** argv)
{
    int res = DSMEngine::main(argc,argv);	// deceptively simple
    cerr << "dsm exiting, status=" << res << endl;
    return res;
}
