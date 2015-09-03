/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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
/*

    App to query and set options on a Diamond emerald serial IO card.
    Interacts via ioctls with the emerald kernel module.
*/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h> // perror()
#include <errno.h>
#include <nidas/linux/diamond/emerald.h>

#include <string>
#include <iostream>
#include <sstream>

using namespace std;

class EmeraldDIO
{
public:
    EmeraldDIO();

    int parseRunstring(int argc, char** argv);

    int usage(const char* argv0);

    int doRequest();

private:
    bool _input;

    string _devName;

    int _fd;

    int _outputVal;
};

EmeraldDIO::EmeraldDIO() : _input(false),_devName(),_fd(-1),_outputVal(-9999)
{
}

int EmeraldDIO::parseRunstring(int argc,char** argv)
{
    extern int optind;
    int opt_char;

    while ((opt_char = getopt(argc, argv, "i")) != -1) {
        switch (opt_char) {
        case 'i':
          _input = true;
          break;
        case '?':
          usage(argv[0]);
          break;
        }
    }

    if (optind == argc) return usage(argv[0]);
    _devName = argv[optind++];

    if (! _input && optind < argc) {
        istringstream ist(argv[optind]);
        ist >> _outputVal;
        if (ist.fail()) {
            cerr << "Cannot parse val: " << argv[optind] << endl;
            usage(argv[0]);
        }
        optind++;
    }
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int EmeraldDIO::usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " [-i] device [val]\n\n\
-i: set the port to be an input\n\
device: device name, for example: /dev/ttyDn\n\
val: 0 or 1, set the digital output port to low(0) or high(1)\n\
    If val is not specified, then return the current value\n" << endl;
    return 1;
}

int EmeraldDIO::doRequest()
{
    int opts = O_RDWR;
    if (_input) opts = O_RDONLY;

    _fd = ::open(_devName.c_str(),opts);
    if (_fd < 0) {
        perror(_devName.c_str());
        return -1;
    }

    int res = 0;

    if (_input) {
        int output;
        if (ioctl(_fd,EMERALD_IOCGDIOOUT,&output) < 0) {
            perror(_devName.c_str());
            return -1;
        }
        int val = 0;
        if (output && ioctl(_fd,EMERALD_IOCSDIOOUT,val) < 0) {
              perror(_devName.c_str());
              return -1;
        }
        if (ioctl(_fd,EMERALD_IOCGDIO,&res) < 0) {
            perror(_devName.c_str());
            return -1;
        }
        cout << res << endl;
    }
    else {
        int output;
        if (ioctl(_fd,EMERALD_IOCGDIOOUT,&output) < 0) {
            perror(_devName.c_str());
            return -1;
        }
        int val = 1;
        if (!output && ioctl(_fd,EMERALD_IOCSDIOOUT,val) < 0) {
            perror(_devName.c_str());
            return -1;
        }
        if (_outputVal < -9000) {
            if (ioctl(_fd,EMERALD_IOCGDIO,&res) < 0) {
                perror(_devName.c_str());
                return -1;
            }
            cout << res << endl;
        }
        else {
            if (ioctl(_fd,EMERALD_IOCSDIO,_outputVal) < 0) {
                perror(_devName.c_str());
                return -1;
            }
        }
    }
    ::close(_fd);
    return res;
}

int main(int argc, char** argv)
{

    EmeraldDIO dio;

    int res;

    res = dio.parseRunstring(argc, argv);
    if (res) return res;

    res = dio.doRequest();
    return res;
}
