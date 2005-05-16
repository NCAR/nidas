/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <RawSampleInputStream.h>
#include <DSMEngine.h>

#ifdef NEEDED
// #include <Sample.h>
#include <dsm_sample.h>
#include <atdUtil/EOFException.h>
#endif

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class DumpClient: public SampleClient 
{
public:

    typedef enum format { DEFAULT, ASCII, HEX, SIGNED_SHORT, FLOAT } format_t;

    DumpClient(dsm_sample_id_t,format_t,ostream&);

    virtual ~DumpClient() {}

    bool receive(const Sample* samp) throw();

    void printHeader();

private:
    dsm_sample_id_t sampleId;
    format_t format;
    ostream& ostr;

};

class Runstring {
public:
    Runstring(int argc, char** argv);

    static void usage(const char* argv0);
    bool process;
    string xmlFileName;
    string dataFileName;
    dsm_sample_id_t sampleId;
    DumpClient::format_t format;
};

Runstring::Runstring(int argc, char** argv): process(false),sampleId(0),
	format(DumpClient::DEFAULT)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

										
    while ((opt_char = getopt(argc, argv, "Ad:FHps:Sx:")) != -1) {
	switch (opt_char) {
	case 'A':
	    format = DumpClient::ASCII;
	    break;
	case 'd':
	    sampleId = SET_DSM_ID(sampleId,atoi(optarg));
	    break;
	case 'F':
	    format = DumpClient::FLOAT;
	    break;
	case 'H':
	    format = DumpClient::HEX;
	    break;
	case 'p':
	    process = true;
	    break;
	case 's':
	    sampleId = SET_SHORT_ID(sampleId,atoi(optarg));
	    break;
	case 'S':
	    format = DumpClient::SIGNED_SHORT;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (format == DumpClient::DEFAULT)
    	format = (process ? DumpClient::FLOAT : DumpClient::HEX);

    if (optind == argc - 1) dataFileName = string(argv[optind++]);
    if (dataFileName.length() == 0) usage(argv[0]);
    if (xmlFileName.length() == 0) usage(argv[0]);

    if (sampleId < 0) usage(argv[0]);
    if (optind != argc) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p] -d dsmid -s sampleId -x xml_file data_file [-A | -H | -S]\n\
  -p: process (optional). Pass samples to sensor process method\n\
  -x xml_file (optional). Name of XML file (required with -p option)\n\
  -A: ASCII output (typical for samples from a serial sensor)\n\
  -F: floating point output (default for processed output)\n\
  -H: hex output (default for raw output)\n\
  -S: signed short output (typical for samples from an A2D)\n\
  data_file (required). Name of sample file.\n\
" << endl;
    exit(1);
}

DumpClient::DumpClient(dsm_sample_id_t id,format_t fmt,ostream &outstr):
	sampleId(id),format(fmt),ostr(outstr)
{
}

void DumpClient::printHeader()
{
    cout << "|--- date time -------|  bytes" << endl;
}

bool DumpClient::receive(const Sample* samp) throw()
{
    dsm_time_t tt = samp->getTimeTag();

    dsm_sample_id_t sampid = samp->getId();
    if (sampid != sampleId) return false;

    struct tm tm;
    char cstr[64];
    time_t ut = tt / 1000;
    gmtime_r(&ut,&tm);
    int msec = tt % 1000;
    strftime(cstr,sizeof(cstr),"%Y %m %d %H:%M:%S",&tm);
    ostr << cstr << '.' << setw(3) << setfill('0') << msec << ' ';
    ostr << setw(7) << setfill(' ') << samp->getDataByteLength() << ' ';

    switch(format) {
    case ASCII:
	{
	string dstr((const char*)samp->getConstVoidDataPtr(),
		samp->getDataByteLength());
        ostr << dstr << endl;
	}
        break;
    case HEX:
        {
	const unsigned char* cp =
		(const unsigned char*) samp->getConstVoidDataPtr();
	ostr << setfill('0');
	for (unsigned int i = 0; i < samp->getDataByteLength(); i++)
	    ostr << hex << setw(2) << (unsigned int)cp[i] << dec << ' ';
	ostr << endl;
	}
        break;
    case SIGNED_SHORT:
	{
	const short* sp =
		(const short*) samp->getConstVoidDataPtr();
	ostr << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/2; i++)
	    ostr << setw(6) << sp[i] << ' ';
	ostr << endl;
	}
        break;
    case FLOAT:
	{
	const float* fp =
		(const float*) samp->getConstVoidDataPtr();
	ostr << setprecision(4) << setfill(' ');
	for (unsigned int i = 0; i < samp->getDataByteLength()/4; i++)
	    ostr << setw(10) << fp[i] << ' ';
	ostr << endl;
	}
        break;
    case DEFAULT:
        break;
    }
    return true;
}

int main(int argc, char** argv)
{

    Runstring rstr(argc,argv);

    dsm::FileSet* fset = new dsm::FileSet();

#ifdef USE_FILESET_TIME_CAPABILITY
    struct tm tm;
    strptime("2005 04 05 00:00:00","%Y %m %d %H:%M:%S",&tm);
    time_t start = timegm(&tm);

    strptime("2005 04 06 00:00:00","%Y %m %d %H:%M:%S",&tm);
    time_t end = timegm(&tm);

    fset->setDir("/tmp/RICO/hiaper");
    fset->setFileName("radome_%Y%m%d_%H%M%S.dat");
    fset->setStartTime(start);
    fset->setEndTime(end);
#else
    fset->setFileName(rstr.dataFileName);
#endif

    RawSampleInputStream sis(fset);
    sis.init();

    auto_ptr<Project> project;
    list<DSMSensor*> allsensors;

    if (rstr.xmlFileName.length() > 0) {
	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(rstr.xmlFileName));

        project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	const list<Site*>& sitelist = project->getSites();
	list<Site*>::const_iterator ai;
	for (ai = sitelist.begin(); ai != sitelist.end(); ++ai) {
	    Site* site = *ai;
	    const list<DSMConfig*>& dsms = site->getDSMConfigs();
	    list<DSMConfig*>::const_iterator di;
	    for (di = dsms.begin(); di != dsms.end(); ++di) {
		DSMConfig* dsm = *di;
		const list<DSMSensor*>& sensors = dsm->getSensors();
		allsensors.insert(allsensors.end(),sensors.begin(),sensors.end());
	    }
	}
    }

    DumpClient dumper(rstr.sampleId,rstr.format,cout);

    if (rstr.process) {
	list<DSMSensor*>::const_iterator si;
	for (si = allsensors.begin(); si != allsensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->init();
	    sis.addProcessedSampleClient(&dumper,sensor);
	}
    }
    else sis.addSampleClient(&dumper);

    dumper.printHeader();

    try {
	for (;;) {
	    sis.readSamples();
	}
    }
    catch (atdUtil::EOFException& eof) {
        cerr << eof.what() << endl;
    }
    catch (atdUtil::IOException& ioe) {
        cerr << ioe.what() << endl;
    }

}


