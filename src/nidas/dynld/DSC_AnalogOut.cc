// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#include <nidas/dynld/DSC_AnalogOut.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <cmath>

using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

DSC_AnalogOut::DSC_AnalogOut():
    _devName(),_fd(-1),_noutputs(0),_conv()
{
}

DSC_AnalogOut::~DSC_AnalogOut()
{
    try {
        if (_fd >= 0) close();
    }
    catch(const n_u::IOException& e) {}
}

void DSC_AnalogOut::open() throw(n_u::IOException)
{

    if ((_fd = ::open(_devName.c_str(),O_RDWR)) < 0)
        throw n_u::IOException(_devName,"open",errno);

    if ((_noutputs = ::ioctl(_fd,DMMAT_D2A_GET_NOUTPUTS,0)) < 0)
        throw n_u::IOException(_devName,"ioctl GET_NOUTPUTS",errno);

    if (::ioctl(_fd,DMMAT_D2A_GET_CONVERSION,&_conv) < 0)
        throw n_u::IOException(_devName,"ioctl GET_CONVERSION",errno);

}

void DSC_AnalogOut::close() throw(n_u::IOException)
{
    // don't bother checking for error.
    if (_fd >= 0) ::close(_fd);
    _fd = -1;
}


float DSC_AnalogOut::getMinVoltage(int i) const
{
    if (i < 0 || i >= _noutputs) return 0.0;
    return _conv.vmin[i];
}

float DSC_AnalogOut::getMaxVoltage(int i) const
{
    if (i < 0 || i >= _noutputs) return 0.0;
    return _conv.vmax[i];
}

void DSC_AnalogOut::setVoltages(const vector<int>& which,
    const vector<float>& val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    if (which.size() != val.size()) {
        ostringstream ost;
        ost << "length of which=" << which.size() <<
            " is not equal to " << val.size();
        throw n_u::InvalidParameterException(getName(),"setVoltage",
            ost.str());
    }

    DMMAT_D2A_Outputs out;
    for (int i = 0; i < _noutputs; i++) out.active[i] = out.counts[i] = 0;

    out.nout = 0;
    for (unsigned int i = 0; i < which.size(); i++) {

        int ic = which[i];
        if (ic < 0 || ic >= _noutputs)  {
            ostringstream ost;
            ost << "output number=" << ic <<
                " is out of the range 0:" << _noutputs - 1;
            throw n_u::InvalidParameterException(getName(),"setVoltage",
                ost.str());
        }

        // number of channels to change
        out.nout = std::max(out.nout,ic+1);
        out.active[ic] = 1;
        int cout = _conv.cmin[i] +
            (int)rint(val[i] /
                (_conv.vmax[i] - _conv.vmin[i]) * (_conv.cmax[i] - _conv.cmin[i]));
        cout = std::max(cout,_conv.cmin[i]);
        cout = std::min(cout,_conv.cmax[i]);
        out.counts[ic] = cout;
#ifdef DEBUG
        cerr << "which[" << i << "]=" << ic <<
            ", out.counts[" << ic << "]=" << out.counts[ic] << endl;
#endif
    }

    if (::ioctl(_fd,DMMAT_D2A_SET,&out) < 0)
        throw n_u::IOException(_devName,"ioctl SET_OUTPUT",errno);
}

void DSC_AnalogOut::setVoltage(int which,float val)
            throw(nidas::util::IOException,
                nidas::util::InvalidParameterException)
{
    vector<int> whiches;
    whiches.push_back(which);
    vector<float> vals;
    vals.push_back(val);
    setVoltages(whiches,vals);
}

