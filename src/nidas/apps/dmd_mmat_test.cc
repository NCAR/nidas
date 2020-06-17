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


struct waveout
{
    int chan;      // Output Voltage Channel
    int len;       // Ramp length
    float vstart;  // Ramp min voltage
    float vend;    // Ramp max voltage
    bool constant; // Waveform is a constant value, T/F
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
    cerr << "usage: " << argv0 << " [-a] [-r rate] [-w ch,len,vstart,vend] [-w ch,len,vstart,vend] devicename\n\
    -a: perform MM32XAT A2D autocalibration sequence. Must target A2D devicename.\n\
    -r rate: waveform output rate in Hz\n\
    -w chan,len,vstart,vend: output a sawtooth waveform of given length on a channel, within the voltage range\n\
           Multiple -w waveforms may be specified in the same command for different channels\n\
    Note: specifying -a, -r, or vmax!=vmin may interfere with other applications using the A2D device\n\
    ";
    return 1;
}

int DMD_MMAT_test::parseRunstring(int argc, char * argv[])
{
    extern char *optarg; /* set by getopt() */
    extern int optind;   /* "  "     "     */
    int opt_char;        /* option character */

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
                ist >> output.vstart >> comma;
                if (comma != ',' || ist.fail()) return usage(argv[0]);
                ist >> output.vend;
                if (ist.fail()) return usage(argv[0]);
                if (fabs(output.vend - output.vstart) < 0.001)
                    output.constant = true;
                else
                    output.constant = false;
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

    // Autocalibrate A2D if instructed
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

    // Stop D2A waveform generator
    if ((res = ::ioctl(fd,DMMAT_STOP)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_STOP,)",errno);
    }

    // Get D2A setup config informations
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

    // Display D2A config info
    for (int i = 0; i < nchan; i++) {
        cout << "D2A conversion for chan= " << i << 
            ": vmin=" << conv.vmin[i] << ", vmax=" << conv.vmax[i] <<
            ", cmin=" << conv.cmin[i] << ", cmax=" << conv.cmax[i] << endl;
    }

    // Set the output rate in [Hz].
    struct D2A_Config cfg;
    cfg.waveformRate = (int)rint(_rate);
    if ((res = ::ioctl(fd,DMMAT_D2A_SET_CONFIG, &cfg)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_SET_CONFIG,)",errno);
    }

    // Get the Output Configurations, initialize outputs struct
    struct DMMAT_D2A_Outputs outputs;
    res = ::ioctl(fd,DMMAT_D2A_GET,&outputs);
    if (res < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_GET,)",errno);
    }
    outputs.nout = DMMAT_D2A_OUTPUTS_PER_BRD;

    // Generate waveforms
    //   Channel outputs not specified will remain unchanged
    //   Unspecified waveforms will be stopped
    bool start_waveform_generator = false;

    for (unsigned int iwave = 0; iwave < _waveforms.size(); iwave++) {
        int chan = _waveforms[iwave].chan;
        int wlen = _waveforms[iwave].len;

        float outscale = 1.0*(conv.cmax[chan] - conv.cmin[chan]) / (conv.vmax[chan] - conv.vmin[chan]);
        float dv = (_waveforms[iwave].vend - _waveforms[iwave].vstart) / wlen;

        cerr << "Waveform, Ch = " << chan << endl;
        cerr << "wlen = " << wlen << endl;
        cerr << "vend = " << _waveforms[iwave].vend << endl;
        cerr << "vstart = " << _waveforms[iwave].vstart << endl;
        cerr << "dc/dv = " << outscale << endl;
        cerr << "dv/dt = " << dv << endl;

        if (_waveforms[iwave].constant == false)
        {
            D2A_WaveformWrapper wave(chan,wlen);
            D2A_Waveform *wavep = wave.c_ptr();

            // create a simple ramp from vstart to vend
            for(int ip = 0; ip < wlen; ip++){
                float v = _waveforms[iwave].vstart + ip * dv;
                if (v < conv.vmin[chan]) v = conv.vmin[chan];
                if (v > conv.vmax[chan]) v = conv.vmax[chan];
                wavep->point[ip] = (int)rint((v - conv.vmin[chan]) * outscale + conv.cmin[chan]);
            }

            if ((res = ::ioctl(fd,DMMAT_ADD_WAVEFORM, wavep)) < 0) {
                ::close(fd);
                throw n_u::IOException(_deviceName,"ioctl(,DMMAT_ADD_WAVEFORM,)",errno);
            }

            outputs.counts[chan] = _waveforms[iwave].vstart; // Begin with start ramp value
            outputs.active[chan] = 1; // Mark channel active
            start_waveform_generator = true;
        }
        else // Constant waveform
        {
            int code = (int)rint((_waveforms[iwave].vstart - conv.vmin[chan]) * outscale + conv.cmin[chan]);
            outputs.active[chan] = 1;
            outputs.counts[chan] = code;
            cerr << "DAC const code = " << code << endl;
        }
    }

    // Output constant voltages on active channels
    if ((res = ::ioctl(fd,DMMAT_D2A_SET, &outputs)) < 0) {
        ::close(fd);
        throw n_u::IOException(_deviceName,"ioctl(,DMMAT_D2A_SET,)",errno);
    }

    // Start D2A waveform generator if requested
    if (start_waveform_generator == true)
    {	
        if ((res = ::ioctl(fd,DMMAT_START)) < 0) {
            ::close(fd);
            throw n_u::IOException(_deviceName,"ioctl(,DMMAT_START,)",errno);
        }

        cerr << "Waveforms Running, [ctrl-C] to stop" << endl;
        ::pause();
    }    

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
