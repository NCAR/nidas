/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
        xmtr.doSelfTest();

	xmtr.printStatus();
    }
    catch (n_u::IOException& ioe) {
	xmtr.printStatus();
	std::cerr << ioe.what() << std::endl;
	throw n_u::Exception(ioe.what());
    }
    return 0;
}
