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

#include "Version.h"
#include <nidas/Revision.h>
#include <nidas/Config.h>
#include <sstream>
#include <cstring>

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

namespace
{
    const char* _version = REPO_REVISION;
}

#include "nidas/cpp_symbols.h"

namespace nidas {
namespace core {

const char* Version::getSoftwareVersion()
{
    return _version;
}

const char* Version::getArchiveVersion()
{
    return "1";
}

const char* Version::getChangelogURL()
{
    return "https://ncar.github.io/nidas/CHANGELOG.html";
}

std::string
Version::
getCompilerDefinitions()
{
    std::ostringstream out;
#ifdef __VERSION__
    out << "Compiler: " << __VERSION__ << "\n";
#endif
    for (auto& p: cpp_symbols)
    {
        // the value is the symbol name if undefined
        out << p.first;
        if (strcmp(p.first, p.second) == 0)
            out << " (undefined)";
        else if (p.second[0])
            out << " (defined=" << p.second << ")";
        else
            out << " (defined)";
        out << "\n";
    }
    return out.str();
}


} // namespace core
} // namespace nidas
