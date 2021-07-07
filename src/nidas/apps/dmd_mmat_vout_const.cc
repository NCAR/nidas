// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
//  dmd_mmat_test.cc
//  NIDAS
//  Created by Ryan Orendorff on 2010-06-28.
//  Added in SVN Trunk Revision 5572 

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>
#include <signal.h>
#include <string.h> // strsignal

#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <nidas/linux/diamond/dmd_mmat.h>
#include <nidas/util/IOException.h>

#include <unistd.h>
#include <getopt.h>

using namespace std;

namespace n_u = nidas::util;

struct const_out
{
    int chan;
    float vout;
};

class DMD_MMAT_test
{
public:
    DMD_MMAT_test();
    int usage(const char* argv0);
    int parseRunstring(int argc, char* argv[]);
    void run() throw(n_u::IOException);
private:
    string _deviceName;
    vector<const_out> _const_out;
    bool _autocalA2D;
    bool _verbose;
};

DMD_MMAT_test::DMD_MMAT_test():
    _deviceName(),_const_out(),_autocalA2D(false),_verbose(false)
{
}

int DMD_MMAT_test::usage(const char *)
{
    cerr << "\n--- Output Constant Voltage from DAC Device ---\n\ 
    usage: [-h] [-v] [-a] -w chan,vout -d devicename\n\
    -v                        : verbose status, results\n\
    -a                        : perform autocalibration sequence\n\
    -w chan(uint),vout(float) : output given constant voltage on given channel (0-index)\n\
    -d devicename(str)        : DAC Device, e.g. /dev/dmmat_d2a0\n\
    Example: ./dmd_mmat_vout_const -a -w 0,4.5 -d /dev/dmmat_d2a0\n\
    ";
    return 1;
}

int DMD_MMAT_test::parseRunstring(int argc, char * argv[])
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "hvaw:d:")) != -1) {
        switch (opt_char) {
        case 'v':
            _verbose = true;
            break;
        case 'a':
            _autocalA2D = true;
            break;
        case 'd':
            _deviceName = optarg;
            break;
        case 'w':
            {
                const_out output;
                istringstream ist(optarg);
                char comma;
                ist >> output.chan >> comma;
                if (comma != ',' || ist.fail()) return usage(argv[0]);
                ist >> output.vout;
                if (ist.fail()) return usage(argv[0]);
                _const_out.push_back(output);
            }
            break;
        case 'h':
        case '?':
            return usage(argv[0]);
            break;
        }
    }
    if (optind < argc) _deviceName = argv[optind++];
    if (_deviceName.length() == 0) return usage(argv[0]);
    return 0;
}

void DMD_MMAT_test::run() throw(n_u::IOException)
{
    int res;
    int fd;
    if(_verbose) cerr << "Opening Device..." << endl;

    fd = ::open(_deviceName.c_str(),O_RDWR);
    if (fd < 0) throw n_u::IOException(_deviceName,"open",errno);

    if (_autocalA2D) {
        if (_verbose) cerr << "Autocal Device..." << endl;
        res = ::ioctl(fd,DMMAT_A2D_DO_AUTOCAL);
        if (res < 0) {
            ::close(fd);
            throw n_u::IOException(_deviceName,"ioctl(,DMMAT_A2D_DO_AUTOCAL,)",errno);
        }
        if (_verbose) cout << "Autocal of " << _deviceName << " succeeded" << endl;
        ::close(fd);
        return;
    }

    int nchan = ::ioctl(fd,DMMAT_D2A_GET_NOUTPUTS);
    if (nchan < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_GET_NOUTPUTS,)",errno);
    }

    struct DMMAT_D2A_Conversion conv;
    res = ::ioctl(fd,DMMAT_D2A_GET_CONVERSION,&conv);
    if (res < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_GET_CONVERSION,)",errno);
    }

    if (_verbose)
    {
        for (int i = 0; i < nchan; i++) {
            cout << "D2A conversion for chan= " << i << 
                ": vmin=" << conv.vmin[i] << ", vmax=" << conv.vmax[i] <<
                ", cmin=" << conv.cmin[i] << ", cmax=" << conv.cmax[i] << endl;
        }
    }

    struct DMMAT_D2A_Outputs outputs;
    if ((res = ::ioctl(fd,DMMAT_D2A_GET,&outputs)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_GET,)",errno);
    }
    outputs.nout = 4;

    for (unsigned int i_vconst = 0; i_vconst < _const_out.size(); i_vconst++) {
        unsigned int chan = _const_out[i_vconst].chan;
        float v = _const_out[i_vconst].vout;
        if (_verbose) cerr << "Voltage = " << v << endl;

        float outscale = 1.0 * (conv.cmax[chan] - conv.cmin[chan]) / (conv.vmax[chan] - conv.vmin[chan]);
        if (_verbose) cerr << "Scale Gain = " << outscale << endl;

        // Get DAC code from const voltage
        int dac_code = (int)rint((v - conv.vmin[chan]) * outscale + conv.cmin[chan]);
        if (_verbose) cerr << "DAC code = " << dac_code << endl;

        outputs.active[chan] = 1; //Only turn on the requested channel
        outputs.counts[chan] = dac_code;
    }

    if (_verbose)
    {
        cerr << "outputs: active " << outputs.active[0] << " " <<
                                      outputs.active[1] << " " <<
                                      outputs.active[2] << " " <<
                                      outputs.active[3] << endl;
        cerr << "outputs: nout " << outputs.nout << endl;
    }

    if ((res = ::ioctl(fd,DMMAT_D2A_SET, &outputs)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_SET,)",errno);
    }

    if (_verbose) cerr << "closing " << _deviceName << endl;
    ::close(fd);
}

void sigAction(int sig, siginfo_t*, void*)
{
    cerr << "received signal " << strsignal(sig) << "(" << sig << ")" << endl;
}

void setupSignals()
{
    sigset_t sigset;
    sigemptyset(&sigset);

    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

int main(int argc, char *argv[])
{
    int res;
    DMD_MMAT_test tester;

    if ((res = tester.parseRunstring(argc,argv)) != 0) return res;

    setupSignals();

    try {
        tester.run();
    }
    catch (const n_u::IOException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    return 0;
}
