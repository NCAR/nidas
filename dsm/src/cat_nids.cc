/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-11 07:45:00 -0600 (Tue, 11 Oct 2005) $

    $LastChangedRevision: 3051 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/src/data_stats.cc $
 ********************************************************************

*/

#define _LARGEFILE64_SOURCE

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>



#include <FileSet.h>
#include <UnixIOChannel.h>
#include <SampleInput.h>
#include <SampleOutput.h>
#include <atdUtil/EOFException.h>

using namespace dsm;
using namespace std;

/**
 * Concatenate two or more input Sample archive files into
 * one output file.
 */
class Concater
{
public:

    Concater();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    int parseRunstring(int argc, char** argv);

    int run() throw();

private:
    list<string> inNames;

    string outName;

};

Concater::Concater()
{
}

/* static */
int Concater::main(int argc, char** argv) throw()
{
    Concater cat;

    int result;
    if ((result = cat.parseRunstring(argc,argv))) return result;

    return cat.run();
}

/* static */
int Concater::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-o output_file] input_file ...\n\
    -o output_file: Name of output file, which must not exist.\n\
	If no -o option, or output_file is '-', output will be to stdout.\n\
    input_file: one or more input files\n" << endl;
    return 1;
}

int Concater::parseRunstring(int argc, char**argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "o:")) != -1) {
	switch (opt_char) {
	case 'o':
	    outName = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    for (; optind < argc; optind++)
	inNames.push_back(argv[optind]);

    if (inNames.size() == 0) return usage(argv[0]);

    return 0;
}

int Concater::run() throw()
{

    int result = 0;
    try {

	int outfd;
	if (outName.length() == 0 || !outName.compare("-"))
	    outfd = 1;	// stdout
	else if ((outfd = ::open64(outName.c_str(),
	    O_WRONLY | O_CREAT | O_EXCL,0444)) < 0)
	    throw atdUtil::IOException(outName,"open",errno);

	IOChannel* outchan = new UnixIOChannel(outName,outfd);
	SampleOutputStream outStream;
	outStream.connected(outchan);

	outStream.init();

	FileSet* fset = new dsm::FileSet();

	list<string>::const_iterator fi;
	for (fi = inNames.begin(); fi != inNames.end(); ++fi)
	    fset->addFileName(*fi);

	SampleInputStream sis(fset);
	sis.init();
	sis.readHeader();

	SampleInputHeader header = sis.getHeader();
	header.write(outStream.getIOStream());

	sis.addSampleClient(&outStream);

	try {
	    for (;;) sis.readSamples();
	}
	catch (atdUtil::EOFException& eof) {
	    outStream.finish();
	    outStream.close();
	}
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
	result = 1;
    }
    catch (atdUtil::Exception& ioe) {
        cerr << ioe.what() << endl;
	result = 1;
    }
    return result;
}

int main(int argc, char** argv)
{
    return Concater::main(argc,argv);
}
