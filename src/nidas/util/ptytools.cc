// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2018, Copyright University Corporation for Atmospheric Research
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

#include "ptytools.h"
#include "Logger.h"

#ifdef GPP_2_95_2
#include <strstream>
#else
#include <sstream>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>

using namespace std;

namespace nidas { namespace util {

bool isapty(int fd)
{
    bool retval = false;
    std::string linkName;
    char buf[PATH_MAX];

    memset(buf, 0, PATH_MAX*sizeof(char));

    ostringstream procStr("/proc/self/fd/");
    procStr << fd;
    long int procStrLen = readlink(procStr.str().c_str(), buf, (size_t)PATH_MAX);
    // get the file name, then call checkIfPty(name)
    linkName.append(buf, procStrLen);

    if (procStrLen) {
        DLOG(("Termios::checkIfPty(fd): found link: ") << linkName);
        retval = isapty(linkName);
    }
    else {
        DLOG(("Termios::checkIfPty(int): no link found"));
    }

    return retval;
}

bool isapty(const std::string& devName)
{
    bool retval = false;
    std::string root = devName;
    struct stat devStat;

    // Start by checking if the device name is a link and get it's root
    int error = lstat(devName.c_str(), &devStat);
    if (!error && S_ISLNK(devStat.st_mode)) {
        DLOG(("Termios::checkIfPty(name): %s is a symlink", devName.c_str()));
        char* realName = realpath(devName.c_str(), 0);
        if (realName) {
            root = std::string((const char*)realName, strlen(realName));
            free(realName);
            DLOG(("Termios::checkIfPty(name): symlink target: ") << root);

        }
        // Then check if the device name is in /dev/pts/? or /dev/ptm?
        if (root.find("/dev/pts/") || devName.find("/dev/ptm")) {
            retval = true;
        }
    }
    return retval;
}

}} //namespace nidas { namespace util {
