/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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
/*

    Extract samples from a list of sensors from an archive.

*/

#include <nidas/dynld/RawSampleInputStream.h>
#include <nidas/dynld/RawSampleOutputStream.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/core/FileSet.h>
#include <nidas/core/Bzip2FileSet.h>
#include <nidas/core/Socket.h>
#include <nidas/core/NidasApp.h>
#include <nidas/core/BadSampleFilter.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/auto_ptr.h>

#include <memory>
#include <csignal>
#include <climits>

#include <iomanip>

#include <unistd.h>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

using nidas::util::Logger;
using nidas::util::LogScheme;
using nidas::util::LogConfig;

using nidas::util::Socket;
using nidas::util::SocketAddress;

using nidas::util::IOException;
using nidas::util::EOFException;

namespace nu = nidas::util;


class SensorExtract: public HeaderSource
{
public:

    SensorExtract();

    int parseRunstring(int argc, char** argv);

    int run();

    void sendHeader(dsm_time_t, SampleOutput*);

    /**
     * for debugging.
     */
    void logHeader();

private:

    list<string> inputFileNames;
    std::unique_ptr<SocketAddress> sockAddr;
    SampleInputHeader header;
    NidasApp app;
    BadSampleFilterArg FilterArg;
};

SensorExtract::SensorExtract():
    inputFileNames(),
    sockAddr(),
    header(),
    app("sensor_extract"),
    FilterArg()
{
}

int SensorExtract::parseRunstring(int argc, char** argv)
{
    app.enableArguments(app.Help | app.InputFiles | app.OutputFiles |
                        app.Version | app.SampleRanges | FilterArg |
                        app.StartTime | app.EndTime |
                        app.loggingArgs());

    // it might be unusual to use sensor_extract on a socket input, but at one
    // point it was explicitly added so sensor_extract could be used in
    // testing as an archiver. however, since file input is much more likely
    // to be the intention, do not set a default input.

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        cout << "Usage: " << app.getName()
             << " [options] {input-spec} {output-spec}\n"
             << app.usage();
        return 1;
    }
    app.parseInputs(args);
    if (!app.inputsProvided())
    {
        throw NidasAppException("Input is missing.");
    }
    app.validateOutput();
    SampleMatcher& matcher = app.sampleMatcher();
    matcher.setStartTime(app.getStartTime());
    matcher.setEndTime(app.getEndTime());
    return 0;
}

void SensorExtract::sendHeader(dsm_time_t, SampleOutput* out)
{
    logHeader();
    header.write(out);
}

void SensorExtract::logHeader()
{
    ILOG(("") << "Writing header to output:");
    ILOG(("") << "ArchiveVersion:" << header.getArchiveVersion());
    ILOG(("") << "SoftwareVersion:" << header.getSoftwareVersion());
    ILOG(("") << "ProjectName:" << header.getProjectName());
    ILOG(("") << "SystemName:" << header.getSystemName());
    ILOG(("") << "ConfigName:" << header.getConfigName());
    ILOG(("") << "ConfigVersion:" << header.getConfigVersion());
}

int SensorExtract::run()
{
    app.setupSignals();
    bool outOK = true;
    try {

        // create the output FileSet, with this instance as the HeaderSource
        nidas::core::FileSet* outSet = 0;
        outSet = FileSet::createFileSet(app.outputFileName());
        outSet->setFileLengthSecs(app.outputFileLength());
        SampleOutputStream outStream(outSet);
        outStream.setHeaderSource(this);

        // create the input channel, from either a FileSet or a socket 
        IOChannel* iochan = 0;
        if (app.dataFileNames().size() > 0)
        {
            nidas::core::FileSet* fset =
                nidas::core::FileSet::getFileSet(app.dataFileNames());
            iochan = fset->connect();
        }
        else
        {
            // retries on socket connect allow for this to be started before
            // the server is ready, such as when used as an archiver.
            nu::Socket* sock = 0;
            while (!sock && !app.interrupted())
            {
                try {
                    sock = new nu::Socket(*app.socketAddress());
                }
                catch(const IOException& e) {
                    WLOG(("%s: retrying in 10 seconds...", e.what()));
                    sleep(10);
                }
            }
            iochan = new nidas::core::Socket(sock);
        }

        // RawSampleInputStream owns the iochan ptr.
        RawSampleInputStream input(iochan);
        input.setBadSampleFilter(FilterArg.getFilter());

        input.readInputHeader();
        // save header for later writing to output
        header = input.getInputHeader();

        // just keep reading samples and writing them to output, using the
        // SampleMatcher to select samples and reassign ids.
        try {
            while (outOK && !app.interrupted())
            {
                Sample* samp = input.readSample();
                if (app.interrupted())
                    break;

                if (app.sampleMatcher().match_with_reassign(samp))
                {
                    outOK = outStream.receive(samp);
                }
                samp->freeReference();
            }
        }
        catch (EOFException& ioe) {
            cerr << ioe.what() << endl;
        }

        outStream.flush();
        outStream.close();
    }
    catch (IOException& ioe)
    {
        cerr << ioe.what() << endl;
        return 1;
    }
    return !outOK;
}


int main(int argc, char** argv)
{
    LogConfig lc;
    lc.level = nidas::util::LOGGER_INFO;
    Logger::getInstance()->setScheme
          (LogScheme("sensor_extract").addConfig (lc));

    SensorExtract merge;
    int res;
    try {
        if ((res = merge.parseRunstring(argc,argv)) != 0)
            return res;
        return merge.run();
    }
    catch (std::exception& e) {
        cerr << e.what() << endl;
    }
    return 1;
}
