/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 11:30:55 -0700 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/apps/ck_config.cc $
 ********************************************************************
*/

#include <nidas/dynld/isff/SE_GOESXmtr.h>

#include <iostream>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

int usage(const char* argv0) 
{
    cerr << argv0 << " devname" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2) return usage(argv[0]);
    SE_GOESXmtr xmtr;
    try {
	xmtr.setName(argv[1]);
	xmtr.setChannel(95);
	xmtr.setId(0x36414752);
	xmtr.open();

	xmtr.init();

	xmtr.printStatus();
    }
    catch (n_u::IOException& ioe) {
	xmtr.printStatus();
	std::cerr << ioe.what() << std::endl;
	throw n_u::Exception(ioe.what());
    }
    return 0;
}
