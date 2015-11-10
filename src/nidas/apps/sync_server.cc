// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include <nidas/dynld/raf/SyncServer.h>
#include <nidas/core/NidasApp.h>
#include <nidas/util/Logger.h>

using nidas::core::NidasApp;
using nidas::core::NidasAppException;

#include <unistd.h>
#include <getopt.h>

#include <iostream>
#include <sstream>

namespace n_u = nidas::util;

using nidas::dynld::raf::SyncServer;


int usage(const std::string& argv0)
{
    std::cerr << "\
Usage: " << argv0 << " [-l sorterSecs] [-p port] [nidas-app-options] raw_data_file ...\n\
    -l sorterSecs: length of sample sorter, in fractional seconds\n\
        default=" << (float)SyncServer::SORTER_LENGTH_SECS << "\n\
    -p port: sync record output socket port number: default="
              << SyncServer::DEFAULT_PORT << "\n\
    raw_data_file: names of one or more raw data files, separated by spaces\n"
              << std::endl;
    
    NidasApp& app = *NidasApp::getApplicationInstance();
    std::cerr << "NIDAS options:\n" << app.usage();
    return 1;
}


int parseRunstring(SyncServer& sync, std::vector<std::string>& args)
{
    NidasApp& app = *NidasApp::getApplicationInstance();
    app.parseArguments(args);

    std::list<std::string> dataFileNames;

    unsigned int i = 1;
    while (i < args.size())
    {
        std::string arg = args[i];
        std::string optarg;
        if (i+1 < args.size())
            optarg = args[i+1];

        if (arg == "-l" && !optarg.empty())
        {
            std::istringstream ist(optarg);
            float sorter_secs;
            ist >> sorter_secs;
            if (ist.fail()) 
                return usage(args[0]);
            sync.setSorterLengthSeconds(sorter_secs);
            ++i;
        }
        else if (arg == "-p" && !optarg.empty())
        {
            int port;
            std::istringstream ist(optarg);
            ist >> port;
            if (ist.fail()) 
                sync.resetAddress(new n_u::UnixSocketAddress(optarg));
            else
                sync.resetAddress(new n_u::Inet4SocketAddress(port));
            ++i;
        }
        else if (arg[0] == '-')
        {
	    return usage(args[0]);
	}
        else
        {
            dataFileNames.push_back(arg);
        }
        ++i;
    }

    if (app.xmlHeaderFile().length())
        sync.setXMLFileName(app.xmlHeaderFile());
    if (dataFileNames.size() == 0)
        return usage(args[0]);
    sync.setDataFileNames(dataFileNames);
    return 0;
}


SyncServer* signal_target = 0;

void
interrupt_sync_server()
{
    if (signal_target)
    {
        signal_target->interrupt();
    }
}


void setupSignals(SyncServer& sync)
{
    signal_target = &sync;
    NidasApp::setupSignals(interrupt_sync_server);
}


int main(int argc, char** argv)
{
    NidasApp app("sync_server");
    app.enableArguments(app.LogLevel | app.XmlHeaderFile);
    // Because -l is overloaded for sorter seconds.
    app.requireLongFlag(app.LogLevel);
    app.setApplicationInstance();

    SyncServer sync;
    setupSignals(sync);

    std::vector<std::string> args(argv, argv+argc);

    int res;
    try {
        if ((res = parseRunstring(sync, args)) != 0)
            return res;
    }
    catch (NidasAppException& appx)
    {
        std::cerr << appx.what() << std::endl;
        return 1;
    }

    try {
        sync.init();
        int result = sync.run();

        // Destroy the project created and initialized by the SyncServer.
        nidas::core::Project::destroyInstance();
        nidas::core::XMLImplementation::terminate();

        return result;
    }
    catch(const n_u::Exception&e ) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

}
