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
 * program is to extract PMS2D data, repackage it into ADS2 format and
 * write it to a new file with a new XML header.
 */

#include <nidas/core/FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/util/UTime.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>

#include <nidas/dynld/raf/TwoD64_USB.h>
#include <nidas/dynld/raf/TwoD32_USB.h>

#include <fstream>
#include <memory> // auto_ptr<>


static const int P2D_DATA = 4096;	// TwoD image buffer size.

static const unsigned char Fast2DsyncStr[] = { 0xAA, 0xAA, 0xAA };

// Old 32bit 2D overload word, MikeS puts the overload in the first slice.
static const unsigned char overLoadSync[] = { 0x55, 0xAA };

static const size_t DefaultMinimumNumberParticlesRequired = 5;


using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;


class Probe
{
public:
  Probe() : resolution(0), hasOverloadCount(0), nDiodes(64), recordCount(0), rejectRecordCount(0)
    {
      memset((void *)diodeCount, 0, sizeof(diodeCount));
      memset((void *)particleCount, 0, sizeof(particleCount));
    }

  // Input info.
  DSMSensor * sensor;

  // Output info.
  size_t resolution;
  float resolutionM;
  short id;

  // File info.
  size_t hasOverloadCount;

  // Number of diodes, 64 for Fast2D, 32 for old probes.
  size_t nDiodes;

  // Total number of records found in file.
  size_t recordCount;

  // Total number of records rejected due to too few sync words.
  size_t rejectRecordCount;

  /* Count of each diode value of 1 for the entire flight, sync words excluded.
   * This diagnostic output helps find which diode is bad when the probe runs away.
   */
  size_t diodeCount[64];	// 64 is max possible diodes.

  /* Count of number of particles per record.
   * This diagnostic output helps find which diode is bad when the probe runs away.
   */
  size_t particleCount[1024];	// 1024 is max possible slices/record.
};


// ADS1 / ADS2 legacy format.  This is what we will repackage the records into.
struct P2d_rec
{
  short id;                             // 'P1','C1','P2','C2', H1, H2
  short hour;
  short minute;
  short second;
  short year;
  short month;
  short day;
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

const n_u::EndianConverter * bigEndian = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);


class Extract2D : public HeaderSource
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

    /**
     * Count number of particles in a record, also report miss-aligned data.
     */
    size_t countParticles(Probe * probe, P2d_rec & record);

    /**
     * Sum occluded diodes along the flight path.  This increments
     * for the whole flight.  Help find stuck bits.
     */
    void computeDiodeCount(Probe * probe, P2d_rec & record);


    static bool interrupted;

    bool outputHeader;

    /// Whether to output diode count histogram.
    bool outputDiodeCount;

    /// Whether to output particle count histogram.
    bool outputParticleCount;

    string xmlFileName;

    list<string> inputFileNames;

    string outputFileName;

    int outputFileLength;

    SampleInputHeader header;

    set<dsm_sample_id_t> includeIds;

    set<dsm_sample_id_t> excludeIds;

    map<dsm_sample_id_t,dsm_sample_id_t> newids;

    size_t minNumberParticlesRequired;
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
/* maybe some day we want this option.
    -x dsmid,sensorid: the dsm id and sensor id of samples to exclude\n\
            more than one -x option can be specified\n\
	    either -s or -x options can be specified, but not both\n\
*/
    cerr << "\
Usage: " << argv0 << " [-x dsmid,sensorid] [-s] [-c] [-n #] output input ... \n\n\
    -s: generate diode count histogram along flight path.\n\
    -c: generate particle count histogram.\n\
    -n #: Minimum number of time slices required per record to\n\
            transfer to output file.\n\
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
	outputHeader(true), outputDiodeCount(false), outputParticleCount(false), outputFileLength(0), minNumberParticlesRequired(DefaultMinimumNumberParticlesRequired)
{
}

int Extract2D::parseRunstring(int argc, char** argv) throw()
{
    extern int optind;       /* "  "     "     */
    extern char *optarg;       /* set by getopt() */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "csn:")) != -1) {
	switch (opt_char) {
	case 'c':
	    outputParticleCount = true;
            break;
	case 'n':
	    minNumberParticlesRequired = atoi(optarg);
            break;
	case 's':
	    outputDiodeCount = true;
            break;
/*
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
*/
	case '?':
	    return usage(argv[0]);
	}
    }

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

        nidas::core::FileSet * fset = new nidas::core::FileSet();

        list<string>::const_iterator fi = inputFileNames.begin();
        for (; fi != inputFileNames.end(); ++fi)
            fset->addFileName(*fi);

        // SampleInputStream owns the iochan ptr.
        SampleInputStream input(fset);

        input.readInputHeader();
        header = input.getInputHeader();

        auto_ptr<Project> project;
        map<dsm_sample_id_t, Probe *> probeList;

        if (xmlFileName.length() == 0)
            xmlFileName = header.getConfigName();
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);


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

                    cout << "Found 2D sensor = " << (*dsm_it)->getCatalogName()
                         << (*dsm_it)->getSuffix() << endl;

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
                    p->resolutionM = p->resolution * 1.0e-6;

                    if ((*dsm_it)->getCatalogName().compare("Fast2DC") == 0)
                    {
                        if (p->resolution == 10)	// 10um.
                            p->id = htons(0x4336);	// C6, Fast 10um 64 diode probe.
                        else
                            p->id = htons(0x4334);	// C4, Fast 25um 64 diode probe.
                    }
                    else
                        p->nDiodes = 32;	// Old probe, should only be 2DP on ADS3.

                    if ((*dsm_it)->getCatalogName().compare("TwoDC") == 0)
                        p->id = htons(0x4331 + Ccnt++);

                    if ((*dsm_it)->getCatalogName().compare("TwoDP") == 0)
                    {
                        p->id = htons(0x5031 + Pcnt++);
                        // Undo hard fixed divide by 10 in USB white box for 2DP only
                        p->resolutionM /= 10;
                    }

                    if (outputHeader)
                    {
                        outFile << "  <probe id=\"" << ((char *)&p->id)[0]
				<< ((char *)&p->id)[1] << "\""
                                << " type=\"" << (*dsm_it)->getCatalogName() << "\""
                                << " resolution=\"" << p->resolution << "\""
                                << " suffix=\"" << (*dsm_it)->getSuffix() << "\"/>\n";
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

        try {
            for (;;) {

                Sample * samp = input.readSample();
                if (interrupted) break;
                dsm_sample_id_t id = samp->getId();

		if (includeIds.size() > 0) {
		    if (includeIds.find(id) != includeIds.end()) {
                        if (samp->getDataByteLength() == 4104) {
                            const int * dp = (const int *) samp->getConstVoidDataPtr();
                            int stype = bigEndian->int32Value(*dp++);

                            if (stype != TWOD_SOR_TYPE) {  
                                unsigned char * cp = (unsigned char *)dp;
                                unsigned short * sp = (unsigned short *)dp;

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
                                record.year = htons(t.tm_year) + 1900;
                                record.month = htons(t.tm_mon) + 1;
                                record.day = htons(t.tm_mday);
                                record.msec = htons(msecs);

                                // Decode true airpseed.
                                float tas = 0.0;
                                if (stype == TWOD_IMG_TYPE) {
                                    tas = (1.0e6 / (1.0 - ((float)cp[0] / 255))) * probe->resolutionM;
                                }
                                if (stype == TWOD_IMGv2_TYPE) {
//                                    tas = 1.0e11 / (511 - (float)bigEndian->uint16Value(sp[0])) * 511 / 25000 / 2 * probe->resolutionM;
                                    tas = 1.0e11 / (511 - (float)sp[0]) * 511 / 25000 / 2 * probe->resolutionM;
                                }

                                // Encode true airspeed to the ADS1 / ADS2 format for
                                // backwards compatability.
                                record.tas = htons( ((short)tas * (255 / 125) + 1) );

                                record.overld = htons(0);
                                ::memcpy(record.data, dp, P2D_DATA);

                                // For old 2D probes, not Fast 2DC.
                                if (::memcmp(record.data, overLoadSync, 2) == 0)
                                {
                                    unsigned long * lp = (unsigned long *)record.data;
                                    record.overld = htons((ntohl(*lp) & 0x0000ffff) / 2000);
                                    probe->hasOverloadCount++;
                                }

                                ++probe->recordCount;
				if (countParticles(probe, record) > minNumberParticlesRequired)
                                    outFile.write((char *)&record, sizeof(record));
                                else
                                    ++probe->rejectRecordCount;

                                if (outputDiodeCount)
                                    computeDiodeCount(probe, record);
                            }
                        }
		    }
		}
                samp->freeReference();
            }
        }
        catch (n_u::EOFException& ioe) {
; //            cerr << ioe.what() << endl;
        }

        outFile.close();

        // Output some statistics.
        map<dsm_sample_id_t, Probe *>::iterator mit;
        for (mit = probeList.begin(); mit != probeList.end(); ++mit)
        {
            Probe * probe = (*mit).second;
            cout << endl << probe->sensor->getCatalogName()
                 << probe->sensor->getSuffix()
                 << " statistics." << endl;

            cout.width(10);
            cout << probe->recordCount << " "
                 << " total records." << endl;

            if (probe->rejectRecordCount > 0) {
                cout.width(10);
                cout << probe->rejectRecordCount << " "
                 << " records were rejected due to fewer than "
                 << minNumberParticlesRequired
                 << " particles per record." << endl;
            }

            if (probe->sensor->getCatalogName().compare("Fast2DC")) {
                cout.width(10);
                cout << probe->hasOverloadCount
                 << " records had an overload word @ spot zero, or "
                 << probe->hasOverloadCount * 100 / probe->recordCount
                 << "%." << endl;
            }

            if (outputDiodeCount) {
                for (size_t i = 0; i < probe->nDiodes; ++i) {
                    cout << "    Bin ";
                    cout.width(2);
                    cout << i << " count = ";
                    cout.width(10);
                    cout << probe->diodeCount[i] << endl;
                }
            }

            if (outputParticleCount) {
                for (size_t i = 0; i < 1024; ++i) {
                  if (probe->particleCount[i] > 0) {
                    cout.width(10);
                    cout << probe->particleCount[i];
                    cout << i << " records have ";
                    cout << "    nParticles = ";
                    cout.width(4);
                }
              }
            }

            delete probe;
        }
    }
    catch (n_u::IOException& ioe)
    {
        cerr << ioe.what() << endl;
	return 1;
    }
    return 0;
}

void Extract2D::computeDiodeCount(Probe * probe, P2d_rec & record)
{
    size_t nSlices = P2D_DATA / (probe->nDiodes / 8);
    size_t nBytes = P2D_DATA / nSlices;

    /* Compute flight long histogram of each bit.  Useful for stuck bit detection.
     */
    for (size_t s = 0; s < nSlices; ++s)
    {
      unsigned char * slice = &record.data[s*nBytes];
      if (memcmp(slice, Fast2DsyncStr, 3) == 0 || (probe->nDiodes == 32 && slice[0] == 0x55))
        continue;

      for (size_t i = 0; i < nBytes; ++i)
      {
        for (size_t j = 0; j < 8; ++j) // 8 bits.
          if (((slice[i] << j) & 0x80) == 0x00)
            probe->diodeCount[i*nBytes + j]++;
      }
    }
}


size_t Extract2D::countParticles(Probe * probe, P2d_rec & record)
{
    size_t totalCnt = 0, missCnt = 0;
    unsigned char * p = record.data;

    if (probe->nDiodes == 32)
        for (size_t i = 0; i < 4095; ++i, ++p) {
            if (*p == 0x55) {
                ++totalCnt;
                if ((i % 4) != 0)
                    ++missCnt;
            }
        }

    if (probe->nDiodes == 64)
        for (size_t i = 0; i < 4093; ++i, ++p) {
            if (::memcmp(p, Fast2DsyncStr, 3) == 0) {
                ++totalCnt;
                if ((i % 8) != 0)
                    ++missCnt;
            }
        }

    ++probe->particleCount[totalCnt];

    if (missCnt > 0)
    {
        char msg[200];
        sprintf(msg,
		"Miss-aligned data, %02d:%02d:%02d.%03d, rec #%zd, total sync=%d, missAligned count=%d",
		ntohs(record.hour), ntohs(record.minute), ntohs(record.second),
		ntohs(record.msec), probe->recordCount, totalCnt, missCnt);
        cout << msg << endl;
    }

    return totalCnt;
}
