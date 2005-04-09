/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <FileSet.h>
#include <RawSampleInputStream.h>
// #include <Sample.h>
#include <dsm_sample.h>
#include <SampleStreamDater.h>
#include <DSMEngine.h>
#include <atdUtil/EOFException.h>

#include <set>
#include <map>
#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class Runstring {
public:
    Runstring(int argc, char** argv);
    static void usage(const char* argv0);
    bool process;
    string xmlFileName;
    string dataFileName;
};

Runstring::Runstring(int argc, char** argv): process(false)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
										
    while ((opt_char = getopt(argc, argv, "px:")) != -1) {
	switch (opt_char) {
	case 'p':
	    process = true;
	    break;
	case 'x':
	    xmlFileName = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }
    if (optind == argc - 1) dataFileName = string(argv[optind++]);
    if (optind != argc || dataFileName.length() == 0) usage(argv[0]);
}

void Runstring::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p] [-x xml_file] data_file\n\
  -p: process (optional). Pass samples to sensor process method\n\
  -x xml_file (optional). Name of XML file (required with -p option)\n\
  data_file (required). Name of sample file.\n\
" << endl;
    exit(1);
}

class CounterClient: public SampleClient 
{
public:

    virtual ~CounterClient() {}

    bool receive(const Sample* samp) throw();

    void printResults();


private:
    set<dsm_sample_id_t> sampids;
    map<dsm_sample_id_t,dsm_sys_time_t> t1s;
    map<dsm_sample_id_t,dsm_sys_time_t> t2s;
    map<dsm_sample_id_t,unsigned long> nsamps;

    SampleStreamDater dater;

};

bool CounterClient::receive(const Sample* samp) throw()
{
    samp = dater(samp);

    if (dater.getStatus() != dater.OK) return false;
    dsm_sys_time_t sampt = dater.getTime();

    dsm_sample_id_t sampid = samp->getId();
    sampids.insert(sampid);

    map<dsm_sample_id_t,dsm_sys_time_t>::iterator t1i =
	t1s.find(sampid);
    if (t1i == t1s.end())
	t1s.insert(
	    make_pair<dsm_sample_id_t,dsm_sys_time_t>(sampid,sampt));
    t2s[sampid] = sampt;
    nsamps[sampid]++;
    // cerr << samp->getId() << " " << samp->getTimeTag() << endl;
    return true;
}

void CounterClient::printResults()
{
    struct tm tm;
    char tstr[64];
    set<dsm_sample_id_t>::iterator si;
    cout << "sampid    nsamps |----- start -----|  |---- end ---|    rate" << endl;
    for (si = sampids.begin(); si != sampids.end(); ++si) {
	dsm_sample_id_t id = *si;
	time_t ut = t1s[id] / 1000;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%Y %m %d %H:%M:%S",&tm);
	string t1str(tstr);
	ut = t2s[id] / 1000;
	gmtime_r(&ut,&tm);
	strftime(tstr,sizeof(tstr),"%m %d %H:%M:%S",&tm);
	string t2str(tstr);
        cout << setw(6) << id << ' ' << setw(9) << nsamps[id] << ' ' <<
	    t1str << "  " << t2str << ' ' << 
	    fixed << setw(7) << setprecision(2) <<
	    float(nsamps[id]) / ((t2s[id]-t1s[id]) / 1000.) << endl;
    }
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

    CounterClient counter;
    auto_ptr<Project> project;

    if (rstr.process && rstr.xmlFileName.length() > 0) {
	auto_ptr<xercesc::DOMDocument> doc(
		DSMEngine::parseXMLConfigFile(rstr.xmlFileName));

        project = auto_ptr<Project>(Project::getInstance());
	project->fromDOMElement(doc->getDocumentElement());

	const list<Aircraft*>& aclist = project->getAircraft();

	if (aclist.size() == 0)
	    throw atdUtil::InvalidParameterException("project","aircraft",
		    "no aircraft tag in XML config");

	if (aclist.size() > 1)
	    throw atdUtil::InvalidParameterException("project","aircraft",
		    "multiple aircraft tags in XML config");
	Aircraft* aircraft = aclist.front();

	const list<DSMConfig*>& dsms = aircraft->getDSMConfigs();
	list<DSMConfig*>::const_iterator di;
	for (di = dsms.begin(); di != dsms.end(); ++di) {
	    DSMConfig* dsm = *di;
	    const list<DSMSensor*>& sensors = dsm->getSensors();
	    list<DSMSensor*>::const_iterator si;
	    for (si = sensors.begin(); si != sensors.end(); ++si) {
		DSMSensor* sensor = *si;
	        sis.addSensor(sensor);
		sensor->addSampleClient(&counter);
	    }
	    
	}
    }
    else sis.addSampleClient(&counter);


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

    counter.printResults();
}


