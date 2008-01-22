/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/extract2d.cc $

    Extract samples from a list of sensors from an archive.

 ********************************************************************
*/

/*
 * Modified version of Gordon's sensor_extract program.  Purpose of this
 * program is to extract PMS2D data, reformat it into ADS2 format and
 * write it to a new file.
 */

#include <nidas/dynld/FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>

#include <nidas/dynld/raf/TwoD64_USB.h>
#include <nidas/dynld/raf/TwoD32_USB.h>

#include <fstream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

class Probe
{
public:
  Probe() : resolution(0), hasOverloadCount(0), recordCount(0) { }

  // Input info.
  DSMSensor * sensor;

  // Output info.
  size_t resolution;
  float frequency;
  short id;

  // File info.
  size_t hasOverloadCount;
  size_t recordCount;
};


static const int P2D_DATA = 4096;	// TwoD image buffer size.

struct P2d_rec
{
  short id;                             // 'P1','C1','P2','C2', H1, H2
  short hour;
  short minute;
  short second;
  short spare1;
  short spare2;
  short spare3;
  short tas;                            // true air speed
  short msec;                           // msec of this record
  short overld;                         // overload time, msec
  unsigned char data[P2D_DATA];         // image buffer
};
typedef struct P2d_rec P2d_rec;


#include <csignal>
#include <climits>

#include <iomanip>

// hack for arm-linux-gcc from Arcom which doesn't define LLONG_MAX
// #ifndef LLONG_MAX
// #   define LLONG_MAX    9223372036854775807LL
// #   define LLONG_MIN    (-LLONG_MAX - 1LL)
// #endif

namespace n_u = nidas::util;

class Extract2D: public HeaderSource
{
public:

    Extract2D();

    int parseRunstring(int argc, char** argv) throw();

    int run() throw();

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

    static int main(int argc, char** argv) throw();

    static int usage(const char* argv0);

    void sendHeader(dsm_time_t thead,SampleOutput* out)
        throw(n_u::IOException);

private:

    static bool interrupted;

    bool outputHeader;

    string xmlFileName;

    list<string> inputFileNames;

    string outputFileName;

    int outputFileLength;

    SampleInputHeader header;

    set<dsm_sample_id_t> includeIds;

    set<dsm_sample_id_t> excludeIds;

    map<dsm_sample_id_t,dsm_sample_id_t> newids;

};

int main(int argc, char** argv)
{
    return Extract2D::main(argc,argv);
}


/* static */
bool Extract2D::interrupted = false;

/* static */
void Extract2D::sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr <<
    	"received signal " << strsignal(sig) << '(' << sig << ')' <<
	", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
                                                                                
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            Extract2D::interrupted = true;
    break;
    }
}

/* static */
void Extract2D::setupSignals()
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
    act.sa_sigaction = Extract2D::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}

/* static */
int Extract2D::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-s dsmid,sensorid[,newdsmid,newsensorid]] [-s dsmid,sensorid ...]\n\
	[-x dsmid,sensorid] [-l output_file_length] output input ... \n\n\
    -s dsmid,sensorid: the dsm id and sensor id of samples to extract\n\
            more than one -s option can be specified\n\
	    newdsm,newsensor: change id to newdsmid,newsensorid\n\
    -x dsmid,sensorid: the dsm id and sensor id of samples to exclude\n\
            more than one -x option can be specified\n\
	    either -s or -x options can be specified, but not both\n\
    -l output_file_length: length of output files, in seconds\n\
    output: output file name or file name format\n\
    input ...: one or more input file name or file name formats\n\
" << endl;
    return 1;
}

/* static */
int Extract2D::main(int argc, char** argv) throw()
{
    setupSignals();

    Extract2D merge;

    int res;
    
    if ((res = merge.parseRunstring(argc,argv)) != 0) return res;

    return merge.run();
}


Extract2D::Extract2D():
	outputHeader(true), outputFileLength(0)
{
}

int Extract2D::parseRunstring(int argc, char** argv) throw()
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
/*
    while ((opt_char = getopt(argc, argv, "l:s:x:")) != -1) {
	switch (opt_char) {
	case 'l':
	    outputFileLength = atoi(optarg);
	    break;
        case 's':
            {
                unsigned long dsmid;
                unsigned long sensorid;
                unsigned long newdsmid;
                unsigned long newsensorid;
                int i;
                i = sscanf(optarg,"%ld,%ld,%ld,%ld",
                    &dsmid,&sensorid,&newdsmid,&newsensorid);
                if (i < 2) return usage(argv[0]);
                dsm_sample_id_t id = 0;
                id = SET_DSM_ID(id,dsmid);
                id = SET_SHORT_ID(id,sensorid);
                includeIds.insert(id);
                if (i < 3) newdsmid = dsmid;
                if (i < 4) newsensorid = sensorid;
                dsm_sample_id_t newid = 0;
                newid = SET_DSM_ID(newid,newdsmid);
                newid = SET_SHORT_ID(newid,newsensorid);
                newids[id] = newid;
            }
            break;
        case 'x':
            {
                unsigned long dsmid;
                unsigned long sensorid;
                int i;
                i = sscanf(optarg,"%ld,%ld",&dsmid,&sensorid);
                if (i < 2) return usage(argv[0]);
                dsm_sample_id_t id = 0;
                id = SET_DSM_ID(id,dsmid);
                id = SET_SHORT_ID(id,sensorid);
                excludeIds.insert(id);
            }
            break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (includeIds.size() + excludeIds.size() == 0) return usage(argv[0]);
    if (includeIds.size() > 0 &&  excludeIds.size() > 0) return usage(argv[0]);
*/
    if (optind < argc) outputFileName = argv[optind++];
    for ( ;optind < argc; )
        inputFileNames.push_back(argv[optind++]);
    if (inputFileNames.size() == 0) return usage(argv[0]);
    return 0;
}


void Extract2D::sendHeader(dsm_time_t thead,SampleOutput* out)
    throw(n_u::IOException)
{
    header.write(out);
}

int Extract2D::run() throw()
{
    try
    {
        ofstream outFile(outputFileName.c_str(), ofstream::binary | ofstream::trunc);
        if (!outFile.is_open()) {
            throw n_u::IOException("extract2d","can't open output file ",errno);
        }


        FileSet * fset = new nidas::dynld::FileSet();
        int recordCnt, hasOverld;

        list<string>::const_iterator fi = inputFileNames.begin();
        for (; fi != inputFileNames.end(); ++fi)
            fset->addFileName(*fi);

        // SampleInputStream owns the iochan ptr.
        SampleInputStream input(fset);
        input.init();

        input.readHeader();
        header = input.getHeader();

        auto_ptr<Project> project;
        map<dsm_sample_id_t, Probe *> probeList;

        if (xmlFileName.length() == 0)
            xmlFileName = header.getConfigName();
        xmlFileName = Project::expandEnvVars(xmlFileName);


        // Scan header for 2D probes.
        struct stat statbuf;
        if (::stat(xmlFileName.c_str(), &statbuf) == 0)
        {
            auto_ptr<xercesc::DOMDocument> doc(DSMEngine::parseXMLConfigFile(xmlFileName));

            project = auto_ptr<Project>(Project::getInstance());
            project->fromDOMElement(doc->getDocumentElement());

            DSMConfigIterator di = project->getDSMConfigIterator();

            if (outputHeader)
            {
                outFile << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
			<< "<PMS2D>\n"
                        << " <Source>ncar.ucar.edu</Source>\n"
			<< " <Project>" << header.getProjectName() << "</Project>\n"
			<< " <Platform>" << header.getSystemName() << "</Platform>\n";
            }

            for ( ; di.hasNext(); )
            {
                const DSMConfig * dsm = di.next();
                const list<DSMSensor *>& sensors = dsm->getSensors();

                size_t Pcnt = 0, Ccnt = 0;

                list<DSMSensor *>::const_iterator dsm_it;
                for (dsm_it = sensors.begin(); dsm_it != sensors.end(); ++dsm_it)
                {
                    if ( ! dynamic_cast<raf::TwoD_USB *>((*dsm_it)) )
                        continue;

                    cout << "Found 2D sensor = " << (*dsm_it)->getCatalogName() << std::endl;

                    if ((*dsm_it)->getCatalogName().size() == 0)
                    {
                        cout << " Sensor " << (*dsm_it)->getName() << 
                           " is not in catalog, unable to recognize 2DC vs. 2DP, ignoring.\n" <<
                           " Fix by moving this entry from the DSM area up to the sensor catalog.\n";
                        continue;
                    }

                    includeIds.insert((*dsm_it)->getId());

                    Probe * p = new Probe;
                    p->sensor = (*dsm_it);
                    probeList[(*dsm_it)->getId()] = p;

                    const Parameter * parm = p->sensor->getParameter("RESOLUTION");
                    p->resolution = (size_t)parm->getNumericValue(0);
                    p->frequency = p->resolution * 1.0e-6;

                    if ((*dsm_it)->getCatalogName().compare("Fast2DC") == 0)
                        p->id = htons(0x4334);	// C4

                    if ((*dsm_it)->getCatalogName().compare("TwoDC") == 0)
                        p->id = htons(0x4331 + Ccnt++);

                    if ((*dsm_it)->getCatalogName().compare("TwoDP") == 0)
                    {
                        p->id = htons(0x5031 + Pcnt++);
                        // Undo hard fixed divide by 10 in USB white box for 2DP only
                        p->frequency /= 10;
                    }

                    if (outputHeader)
                    {
                        outFile << "  <probe id=\"" << ((char *)&p->id)[0]
				<< ((char *)&p->id)[1] << "\""
                                << " resolution=\"" << p->resolution << "\"/>\n";
                    }
                }
            }
            if (outputHeader)
                outFile << "</PMS2D>\n";
        }

        if (includeIds.size() == 0)
        {
            std::cerr << "extract2d, no PMS2D probes found in the header.\n";
            return 0;
        }

        recordCnt = hasOverld = 0;
        try {
            for (;;) {

                Sample * samp = input.readSample();
                if (interrupted) break;
                dsm_sample_id_t id = samp->getId();

		if (includeIds.size() > 0) {
		    if (includeIds.find(id) != includeIds.end()) {
                        if (samp->getDataByteLength() == 4104) {
                            const int * dp = (const int *) samp->getConstVoidDataPtr();
                            int stype = *dp++;

                            // stype=0 is a image, stype=1 is SOR
                            if (stype == 0) {  
                                unsigned char * cp = (unsigned char *)dp;

                                dp++;      // skip over tas field
                                struct tm t;
                                int msecs;

                                dsm_time_t tt = samp->getTimeTag();
                                n_u::UTime samp_time(tt);
                                samp_time.toTm(true, &t, &msecs);
                                msecs /= 1000;

                                P2d_rec record;
                                Probe * probe = probeList[id];
                                record.id = probe->id;
                                record.hour = htons(t.tm_hour);
                                record.minute = htons(t.tm_min);
                                record.second = htons(t.tm_sec);
                                record.msec = htons(msecs);

                                float tas = (1.0e6 / (1.0 - ((float)cp[0] / 255))) * probe->frequency;
                                record.tas = htons( ((short)tas * (255 / 125) + 1) );

                                record.overld = htons(0);
                                ::memcpy(record.data, dp, P2D_DATA);

                                // For old 2D probes, not Fast 2DC.
                                if (record.data[0] & 0x55aa0000 == 0x55aa0000)
                                {
                                    record.overld = htons((record.data[0] & 0x0000ffff) / 2000);
                                    ++hasOverld;
                                }

                                ++recordCnt;
                                outFile.write((char *)&record, sizeof(record));
                            }
                        }
		    }
		}
                samp->freeReference();
            }
        }
        catch (n_u::EOFException& ioe) {
            cerr << ioe.what() << endl;
        }

        outFile.close();
        cout << recordCnt << " 2D records.  " << hasOverld <<
		" records had an overload word @ spot zero, or " <<
		hasOverld * 100 / recordCnt << "%." <<endl;
    }
    catch (n_u::IOException& ioe)
    {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}
