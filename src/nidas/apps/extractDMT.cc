// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
 * Modified version of Gordon's sensor_extract program.  Purpose of this
 * program is to extract DMT CIP/PIP data, repackage it into the PADS format
 * and write it to a new file.  PADS format is similar to RAF format, with no
 * file header.  Each record has a buffer header added with date and time
 * information, which is slightly different than the RAF format.  Endianness
 * will remain as the probe output, generally little endian.  RAF format is
 * big endian.
 */

#include <nidas/dynld/raf/Extract2D.h>
#include <nidas/dynld/raf/PIP_Image.h>

#include <fstream>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>

/**
 * Version of the output OAP/PIP file.  Increment this when changes occur.
 * Change log:
 * 03/25/2011 - version 1:
 *	tas should be tas as int, drop the '* 255 / 125' encoding.
 *	add nDiodes attribute to probe element.
 *	Change PIP in XML to OAP.
 *	add version.  :)
 */
static const int FILE_VERSION = 1;

static const int P2D_DATA = TWOD_BUFFER_SIZE;	// TwoD image buffer size.

// Sync and overload words for Fast2D.
static const unsigned char syncStr[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
static const unsigned char overloadSync[] = { 0x55, 0x55, 0xAA };

static const size_t DefaultMinimumNumberParticlesRequired = 5;


using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;


#include <iomanip>

namespace n_u = nidas::util;


class ExtractDMT : public Extract2D
{
public:

    ExtractDMT();

    virtual int parseRunstring(int argc, char** argv) throw();

    virtual int usage(const char* argv0);

    static int main(int argc, char** argv) throw();

    int run() throw();

private:

    /**
     * Count number of particles in a record, also report miss-aligned data.
     */
    size_t countParticles(Probe * probe, const unsigned char * record);

    /**
     * Sum occluded diodes along the flight path.  This increments for the
     * whole flight.  Help find stuck bits.
     * @returns the number of diodes that had positives in the record.
     */
    size_t computeDiodeCount(Probe * probe, const unsigned char * record);

};


int main(int argc, char** argv)
{
    return ExtractDMT::main(argc,argv);
}


/* static */
int ExtractDMT::usage(const char* argv0)
{
/* maybe some day we want this option.
    -x dsmid,sensorid: the dsm id and sensor id of samples to exclude\n\
            more than one -x option can be specified\n\
	    either -s or -x options can be specified, but not both\n\
*/
    cerr << "\
Usage: " << argv0 << " [-x dsmid,sensorid] [-a] [-s] [-c] [-n #] output input ... \n\n\
    -a: copy all records, ignore all thresholds.\n\
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
int ExtractDMT::main(int argc, char** argv) throw()
{
    setupSignals();

    ExtractDMT extract;

    int res;
    
    if ((res = extract.parseRunstring(argc, argv)) != 0)
        return res;

    return extract.run();
}


ExtractDMT::ExtractDMT()
{
  outputHeader = false;
}

int ExtractDMT::parseRunstring(int argc, char** argv) throw()
{
    extern int optind;      /* "  "     "     */
    extern char *optarg;    /* set by getopt() */
    int opt_char;           /* option character */

    while ((opt_char = getopt(argc, argv, "acsn:")) != -1) {
	switch (opt_char) {
	case 'a':
	    copyAllRecords = true;
            break;
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


class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int ExtractDMT::run() throw()
{
#ifdef OUTPUT_RAW
  ofstream outFilePIP("pip.dat", ofstream::binary | ofstream::trunc);
#endif
    try
    {
        ofstream outFile(outputFileName.c_str(), ofstream::binary | ofstream::trunc);
        if (!outFile.is_open()) {
            throw n_u::IOException("extractPIP","can't open output file ",errno);
        }
        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(inputFileNames);

        // SampleInputStream owns the iochan ptr.
        SampleInputStream input(fset);

        input.readInputHeader();
        header = input.getInputHeader();

        AutoProject aproject;
        map<dsm_sample_id_t, Probe *> probeList;

        if (xmlFileName.length() == 0)
            xmlFileName = header.getConfigName();
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);


        // Scan header for 2D probes.
        struct stat statbuf;
        if (::stat(xmlFileName.c_str(), &statbuf) == 0)
        {
            n_u::auto_ptr<xercesc::DOMDocument> doc(parseXMLConfigFile(xmlFileName));

            Project::getInstance()->fromDOMElement(doc->getDocumentElement());

            DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();

            if (outputHeader)
            {
                outFile << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
			<< "<OAP version=\"" << FILE_VERSION << "\">\n"
                        << " <Source>ncar.ucar.edu</Source>\n"
                        << " <FormatURL>http://www.eol.ucar.edu/raf/Software/OAPfiles.html</FormatURL>\n"
			<< " <Project>" << header.getProjectName() << "</Project>\n"
			<< " <Platform>" << header.getSystemName() << "</Platform>\n";

                int rc, year, month, day, t;
                char flightNumber[80];
                rc = sscanf(	inputFileNames.front().c_str(), "%04d%02d%02d_%06d_%s.ads",
				&year, &month, &day, &t, flightNumber);
                if (rc == 5) {
                    char date[64];
                    sprintf(date, "%02d/%02d/%04d", month, day, year);
                    if (strrchr(flightNumber, '.') )
                        *(strrchr(flightNumber, '.')) = '\0';
                    outFile << " <FlightNumber>" << flightNumber << "</FlightNumber>\n"
				<< " <FlightDate>" << date << "</FlightDate>\n";
                }
            }

            size_t Pcnt = 0, Ccnt = 0;
            for ( ; di.hasNext(); )
            {
                const DSMConfig * dsm = di.next();
                const list<DSMSensor *>& sensors = dsm->getSensors();

                list<DSMSensor *>::const_iterator dsm_it;
                for (dsm_it = sensors.begin(); dsm_it != sensors.end(); ++dsm_it)
                {
                    if ( ! dynamic_cast<raf::PIP_Image *>((*dsm_it)) )
                        continue;

                    cout << "Found DMT PIP sensor = " << (*dsm_it)->getCatalogName()
                         << (*dsm_it)->getSuffix() << endl;

                    if ((*dsm_it)->getCatalogName().size() == 0)
                    {
                        cout << " Sensor " << (*dsm_it)->getName() << 
                           " is not in catalog, unable to recognize CIP vs. PIP, ignoring.\n" <<
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

                    parm = p->sensor->getParameter("SerialNumber");
                    p->serialNumber = parm->getStringValue(0);

                    if ((*dsm_it)->getCatalogName().compare("PIP_IMAGE") == 0)
                    {
                        p->id = htons(0x5038);	// P8, PIP 100um 64 diode probe.
                    }
                    else
                    if ((*dsm_it)->getCatalogName().compare("CIP_IMAGE") == 0)
                    {
                        p->id = htons(0x4338);	// C8, CIP 64 diode probe.
                    }
                    else
                        p->id = 0;

                    if (outputHeader)
                    {
                        outFile << "  <probe id=\"" << ((char *)&p->id)[0]
				<< ((char *)&p->id)[1] << "\""
                                << " type=\"" << (*dsm_it)->getCatalogName() << "\""
                                << " resolution=\"" << p->resolution << "\""
                                << " nDiodes=\"" << p->nDiodes << "\""
                                << " serialnumber=\"" << p->serialNumber << "\""
                                << " suffix=\"" << (*dsm_it)->getSuffix() << "\"/>\n";
                    }
                }
            }
            if (outputHeader)
                outFile << "</OAP>\n";
        }

        if (includeIds.size() == 0)
        {
            std::cerr << "extractPIP, no OAP probes found in the header.\n";
            return 0;
        }

        try {
            for (;;) {

                Sample *samp = input.readSample();
                if (interrupted) break;
                dsm_sample_id_t id = samp->getId();

		if (includeIds.size() > 0) {
		    if (includeIds.find(id) != includeIds.end()) {
                        if (samp->getDataByteLength() == 4096) {
                            const int * dp = (const int *) samp->getConstVoidDataPtr();

                            PADS_rec record;
                            Probe *probe = probeList[id];
//                            record.id = probe->id;
                            setTimeStamp(record, samp);

                            // Decode true airpseed.
//                            float tas = 0.0;
//                            record.tas = htons((short)tas);

//                            record.overld = htons(0);
                            ::memcpy(record.data, dp, P2D_DATA);

#ifdef OUTPUT_RAW
  outFilePIP.write((char *)dp, 4096);
#endif
                            ++probe->recordCount;
                            if (copyAllRecords ||
                                   (countParticles(probe, record.data) >= minNumberParticlesRequired &&
                                    computeDiodeCount(probe, record.data) != 1))
                                    outFile.write((char *)&record, sizeof(record));
                            else
                                    ++probe->rejectRecordCount;
                        }
		    }
		}
                samp->freeReference();
            }
        }
        catch (n_u::EOFException& ioe) {
; //            cerr << ioe.what() << endl;
        }
#ifdef OUTPUT_RAW
  outFilePIP.close();
#endif

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

size_t ExtractDMT::computeDiodeCount(Probe * probe, const unsigned char * record)
{
    size_t nSlices = P2D_DATA / (probe->nDiodes / 8);
    size_t nBytes = P2D_DATA / nSlices;

    // Diode count for this buffer only.
    size_t diodeCnt[64];	// 64 is max possible diodes.
    memset(diodeCnt, 0, sizeof(diodeCnt));

    /* Compute flight long histogram of each bit.  Useful for stuck bit detection.
     */
    for (size_t s = 0; s < nSlices; ++s)
    {
      const unsigned char * slice = &record[s*nBytes];

      /* Do not count timing or overload words.  We only compare 2 bytes and not 3
       * since in PREDICT the probe started having a stuck bit in the sync word.
       */
      if (memcmp(slice, syncStr, 8) == 0)
        continue;

      for (size_t i = 0; i < nBytes; ++i)
      {
        for (size_t j = 0; j < 8; ++j) // 8 bits.
          if (((slice[i] << j) & 0x80) == 0x00) {
            probe->diodeCount[i*nBytes + j]++;
            diodeCnt[i*nBytes + j]++;
          }
      }
    }

    /* Count how many diodes were active in the buffer.  The idea is if only 1
     * diode was active, then we have a stuck bit.
     */
    size_t cnt = 0;
    for (size_t i = 0; i < 64; ++i) {
        if (diodeCnt[i] > 0)
            ++cnt;
    }

    return cnt;
}


size_t ExtractDMT::countParticles(Probe * probe, const unsigned char * record)
{
    size_t totalCnt = 0;
    const unsigned char *p = record;

    for (size_t i = 0; i < 4088; ++i) {
        if (::memcmp(&p[i], syncStr, 8) == 0) {
            ++totalCnt;
        }
    }

    ++probe->particleCount[totalCnt];

    return totalCnt;
}
