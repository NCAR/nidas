// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
// 
//  dmd_mmat_test.cc
//  NIDAS
//  
//  Created by Ryan Orendorff on 2010-06-28.
//  Copyright 2010 UCAR/NCAR. All rights reserved.
// 
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


struct waveout
{
    int chan;
    int len;
    float vmin;
    float vmax;
};

class DMD_MMAT_test
{
public:
    DMD_MMAT_test();
    int usage(const char* argv0);
    int parseRunstring(int argc, char* argv[]);
    void run() throw(n_u::IOException);
private:
    float _rate;
    string _deviceName;
    vector<waveout> _waveforms;
    bool _autocalA2D;
};

DMD_MMAT_test::DMD_MMAT_test():
    _rate(1.0),_deviceName(),_waveforms(),_autocalA2D(false)
{
}

int DMD_MMAT_test::usage(const char * argv0)
{
    cerr << "usage: " << argv0 << " [-a] [-r rate] [-w chan,len,vmin,vmax ...] devicename\n\
    -a: perform MM32XAT A2D autocalibration sequence.\n\
    -r rate: waveform output rate in Hz\n\
    -w chan,vmin,vmax: output a sawtooth waveform of given length on a channel, within the voltage range\n\
    ";
    return 1;
}

int DMD_MMAT_test::parseRunstring(int argc, char * argv[])
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "ad:r:w:")) != -1) {
        switch (opt_char) {
        case 'a':
            _autocalA2D = true;
            break;
        case 'd':
            _deviceName = optarg;
            break;
        case 'r':
            {
                istringstream ist(optarg);
                ist >> _rate;
                if (ist.fail()) return usage(argv[0]);
            }
            break;
        case 'w':
            {
                waveout output;
                istringstream ist(optarg);
                char comma;
                ist >> output.chan >> comma;
                if (comma != ',' || ist.fail()) return usage(argv[0]);
                ist >> output.len >> comma;
                if (comma != ',' || ist.fail()) return usage(argv[0]);
                ist >> output.vmin >> comma;
                if (comma != ',' || ist.fail()) return usage(argv[0]);
                ist >> output.vmax;
                if (ist.fail()) return usage(argv[0]);
                _waveforms.push_back(output);
            }
            break;
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
    int fd = ::open(_deviceName.c_str(),O_RDWR);
    if (fd < 0) throw n_u::IOException(_deviceName,"open",errno);

    if (_autocalA2D) {
        res = ::ioctl(fd,DMMAT_A2D_DO_AUTOCAL);
        if (res < 0) {
            ::close(fd);
            throw n_u::IOException(_deviceName,"ioctl(,DMMAT_A2D_DO_AUTOCAL,)",errno);
        }
        cout << "Autocal of " << _deviceName << " succeeded" << endl;
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

    for (int i = 0; i < nchan; i++) {
        cout << "D2A conversion for chan= " << i << 
            ": vmin=" << conv.vmin[i] << ", vmax=" << conv.vmax[i] <<
            ", cmin=" << conv.cmin[i] << ", cmax=" << conv.cmax[i] << endl;
    }

    // Set the output rate in _Hz.
    struct D2A_Config cfg;
    cfg.waveformRate = (int)rint(_rate);
    if ((res = ::ioctl(fd,DMMAT_D2A_SET_CONFIG, &cfg)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_SET_CONFIG,)",errno);
    }

    for (unsigned int iwave = 0; iwave < _waveforms.size(); iwave++) {
        int chan = _waveforms[iwave].chan;
        int wlen = _waveforms[iwave].len;

        float outscale = (conv.cmax[chan] - conv.cmin[chan]) / (conv.vmax[chan] - conv.vmin[chan]);

        float dv = (_waveforms[iwave].vmax - _waveforms[iwave].vmin) / wlen;

        D2A_WaveformWrapper wave(chan,wlen);
        D2A_Waveform *wavep = wave.c_ptr();

        // create a simple ramp from vmin to vmax
        for(int ip = 0; ip < wlen; ip++){
            float v = _waveforms[iwave].vmin + ip * dv;
            wavep->point[ip] = (int)rint((v - conv.vmin[chan]) * outscale + conv.cmin[chan]);
        }

        if ((res = ::ioctl(fd,DMMAT_ADD_WAVEFORM, wavep)) < 0) {
            ::close(fd);
            throw n_u::IOException(_deviceName,"ioctl(,DMMAT_ADD_WAVEFORM,)",errno);
        }
    }

    // Tell the D2A device to start.	
    if ((res = ::ioctl(fd,DMMAT_START)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_START,)",errno);
    }

    cerr << "do ctrl-C to stop" << endl;
    ::pause();
    // sleep(30);
    
    cerr << "closing " << _deviceName << endl;
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
