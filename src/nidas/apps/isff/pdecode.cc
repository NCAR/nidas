/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/Project.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/UnixIOChannel.h>
#include <nidas/dynld/FileSet.h>
#include <nidas/dynld/SampleArchiver.h>
#include <nidas/dynld/AsciiOutput.h>
#include <nidas/dynld/isff/NetcdfRPCOutput.h>
#include <nidas/dynld/isff/NetcdfRPCChannel.h>
#include <nidas/dynld/isff/GOESOutput.h>
#include <nidas/dynld/isff/PacketInputStream.h>

#include <memory>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

class PacketDecode
{
public:

    PacketDecode();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    // void getPacketSampleTags();

private:

    list<string> packetFileNames;

    string xmlFileName;

    string netcdfServer;

    // list<SampleTag*> sampleTags;

    bool doAscii;

};

int main(int argc, char** argv)
{
    return PacketDecode::main(argc,argv);
}


/* static */
int PacketDecode::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " -x xml_file [packet_file] ...\n\
    -a : (optional) output ASCII samples \n\
    -h : print this help\n\
    -N nc_server_host: (optional), send data to system running nc_server\n\
    -x xml_file: nidas XML configuration file\n\
    packet_file: name of one or more GOES NESDIS packet files.\n\
    	\'-\' means  read from stdin, which is the default\n\
" << endl;
    return 1;
}

/* static */
int PacketDecode::main(int argc, char** argv) throw()
{
    PacketDecode decoder;

    int res;
    
    if ((res = decoder.parseRunstring(argc,argv)) != 0) return res;

    return decoder.run();
}

PacketDecode::PacketDecode(): doAscii(false)
{
}

int PacketDecode::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "ahN:x:")) != -1) {
	switch (opt_char) {
	case 'a':
	    doAscii = true;
	    break;
	case 'h':
	    return usage(argv[0]);
	case 'N':
	    netcdfServer = optarg;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }

    if (xmlFileName.length() == 0) return usage(argv[0]);

    for ( ; optind < argc; optind++)
        packetFileNames.push_back(argv[optind]);

    if (packetFileNames.size() == 0) packetFileNames.push_back("-");
    return 0;
}

int PacketDecode::run() throw()
{

    try {
	xmlFileName = Project::expandEnvVars(xmlFileName);

	auto_ptr<Project> project;
	XMLParser parser;

	cerr << "parsing: " << xmlFileName << endl;
	auto_ptr<xercesc::DOMDocument> doc(parser.parse(xmlFileName));

	project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	nidas::dynld::FileSet* fset = new nidas::dynld::FileSet();
	list<string>::const_iterator fi;
	for (fi = packetFileNames.begin(); fi != packetFileNames.end(); ++fi)
	    fset->addFileName(*fi);

	// PacketInputStream owns the fset ptr.
	PacketInputStream input(fset);

	input.init();

	SampleArchiver arch;

	arch.connect(&input);

	NetcdfRPCOutput* netcdfOutput = 0;

	if (netcdfServer.length() > 0) {
	    // getPacketSampleTags();
	    NetcdfRPCChannel* netcdfChannel = new NetcdfRPCChannel;
	    netcdfChannel->setServer(netcdfServer);
	    netcdfChannel->setDirectory("${ISFF}/projects/${PROJECT}/netcdf");
	    netcdfChannel->setFileNameFormat("isff_%Y%m%d.nc");
	    netcdfChannel->setCDLFileName("${ISFF}/projects/${PROJECT}/ISFF/config/isff.cdl");
	    const list<const SampleTag*>& tags = input.getSampleTags();
	    list<const SampleTag*>::const_iterator si = tags.begin();
	    for ( ; si != tags.end(); ++si)
		netcdfChannel->addSampleTag(*si);

            netcdfChannel->connect();

	    NetcdfRPCOutput* netcdfOutput = new NetcdfRPCOutput(netcdfChannel);
            arch.connect(netcdfOutput);
	}

	AsciiOutput* asciiOutput = 0;
	if (doAscii) {
            // ascii to stdout, file descriptor 1
	    asciiOutput = new AsciiOutput(new UnixIOChannel("stdout",1));
            arch.connect(asciiOutput);
	}

	try {
	    for (;;) {
		// if (interrupted) break;
		input.readSamples();
	    }
	}
	catch (n_u::EOFException& e) {
	    cerr << "EOF received: flushing buffers" << endl;
	    input.flush();
	    arch.disconnect(&input);
	    input.close();

	    if (asciiOutput) arch.disconnect(asciiOutput);
	    delete asciiOutput;

	    if (netcdfOutput) arch.disconnect(netcdfOutput);
	    delete netcdfOutput;

	    throw e;
	}
	catch (n_u::IOException& e) {
	    input.close();

	    if (asciiOutput) arch.disconnect(asciiOutput);
	    delete asciiOutput;

	    if (netcdfOutput) arch.disconnect(netcdfOutput);
	    delete netcdfOutput;

	    throw e;
	}
    }
    catch (n_u::EOFException& eof) {
        cerr << eof.what() << endl;
	return 1;
    }
    catch (n_u::IOException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (n_u::InvalidParameterException& ioe) {
        cerr << ioe.what() << endl;
	return 1;
    }
    catch (XMLException& e) {
        cerr << e.what() << endl;
	return 1;
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
	return 1;
    }
    return 0;
}

