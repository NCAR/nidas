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

    Extract samples from a list of sensors from an archive.

*/

/*
 * Modified version of Gordon's sensor_extract program.  Purpose of this
 * program is to extract OAP/PMS2D data, repackage it into ADS2 format and write
 * it to a new file with an XML header.
 */

#include <nidas/dynld/raf/Extract2D.h>

#include <nidas/dynld/raf/TwoD64_USB.h>
#include <nidas/dynld/raf/TwoD32_USB.h>

#include <fstream>
#include <sys/stat.h>

#include <unistd.h>
#include <getopt.h>

/**
 * Version of the output OAP/PMS2D file.  Increment this when changes occur.
 * Change log:
 * 03/25/2011 - version 1:
 *	tas should be tas as int, drop the '* 255 / 125' encoding.
 *	add nDiodes attribute to probe element.
 *	Change PMS2D in XML to OAP.
 *	add version.  :)
 */
static const int FILE_VERSION = 1;

static const int P2D_DATA = 4096;	// TwoD image buffer size.

// Sync and overload words for Fast2D.
static const unsigned char Fast2DsyncStr[] = { 0xAA, 0xAA, 0xAA };
static const unsigned char FastOverloadSync[] = { 0x55, 0x55, 0xAA };

// Old 32bit 2D overload word, MikeS puts the overload in the first slice.
static const unsigned char overLoadSync[] = { 0x55, 0xAA };


using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;


#include <iomanip>

namespace n_u = nidas::util;

const n_u::EndianConverter * bigEndian = n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);


class ExtractFast2D : public Extract2D
{
public:

    ExtractFast2D() { };

    int run() throw();

    static int main(int argc, char** argv) throw();

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
    return ExtractFast2D::main(argc,argv);
}


/* static */
int ExtractFast2D::main(int argc, char** argv) throw()
{
    setupSignals();

    ExtractFast2D extract;

    int res;

    if ((res = extract.parseRunstring(argc, argv)) != 0)
        return res;

    return extract.run();
}


class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

int ExtractFast2D::run() throw()
{
    try
    {
        ofstream outFile(outputFileName.c_str(), ofstream::binary | ofstream::trunc);
        if (!outFile.is_open()) {
            throw n_u::IOException("extract2d","can't open output file ",errno);
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
//cout << "Sensor = " << (*dsm_it)->getCatalogName() << (*dsm_it)->getSuffix() << endl;
                    if ( ! dynamic_cast<raf::TwoD_USB *>((*dsm_it)) &&
//                         (*dsm_it)->getCatalogName().compare(0, 5, "TWODS") != 0 &&
                         ! dynamic_cast<raf::TwoD_USB *>((*dsm_it)) )
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

                    parm = p->sensor->getParameter("SerialNumber");
                    p->serialNumber = parm->getStringValue(0);

                    if ((*dsm_it)->getCatalogName().find("_v2") != std::string::npos)
                    {
                        p->clockFreq = 33.333;  // 33 Mhz clock for the second version
                        p->waveLength = 660;    // New laser is 660
                    }

                    parm = p->sensor->getParameter("ClockFrequency");
                    if (parm != 0) p->clockFreq = (size_t)parm->getNumericValue(0);

                    parm = p->sensor->getParameter("LaserWaveLength");
                    if (parm != 0) p->waveLength = (float)parm->getNumericValue(0);


                    if ((*dsm_it)->getCatalogName().compare(0, 7, "Fast2DP") == 0)
                    {
                        p->id = htons(0x5034);	// P4, Fast 200um 64 diode probe.
                    }
                    else
                    if ((*dsm_it)->getCatalogName().compare(0, 7, "Fast2DC") == 0)
                    {
                        if (p->resolution == 10)	// 10um.
                            p->id = htons(0x4336);	// C6, Fast 10um 64 diode probe.
                        else
                            p->id = htons(0x4334 + Ccnt++);	// C4, Fast 25um 64 diode probe.
                    }
                    else
                        p->nDiodes = 32;	// Old probe, should only be 2DP on ADS3.

                    if ((*dsm_it)->getCatalogName().compare("TWODS") == 0)
                    {
                        p->id = htons(0x5331);
                        p->nDiodes = 128;
                        p->clockFreq = 20;      // 20 Mhz clock
                        p->waveLength = 785;    // nano meters
                    }

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
                                << " nDiodes=\"" << p->nDiodes << "\""
                                << " clockFreq=\"" << p->clockFreq << "\""
                                << " laserWaveLength=\"" << p->waveLength << "\""
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
            std::cerr << "extract2d, no OAP probes found in the header.\n";
            return 0;
        }

        try {
            for (;;) {

                Sample *samp = input.readSample();
                if (interrupted) break;
                dsm_sample_id_t id = samp->getId();

		if (includeIds.size() > 0) {
		    if (includeIds.find(id) != includeIds.end()) {
//cout << "length=" << samp->getDataByteLength() << "\n";
                        if (samp->getDataByteLength() == 4104) {
                            const int * dp = (const int *) samp->getConstVoidDataPtr();
                            int stype = bigEndian->int32Value(*dp++);

                            P2d_rec record;
                            Probe *probe = probeList[id];
                            record.id = probe->id;
                            setTimeStamp(record, samp);

                            // Decode true airpseed.
                            float tas = 0.0;
                            if (stype == TWOD_IMG_TYPE) {
                                unsigned char *cp = (unsigned char *)dp;
                                tas = (1.0e6 / (1.0 - ((float)cp[0] / 255))) * probe->resolutionM;
                            }
                            if (stype == TWOD_IMGv2_TYPE) {
                                Tap2D *t2d = (Tap2D *)dp;
                            // Note: TASToTap2D() has a PotFudgeFactor which multiplies by 1.01.
                            //        Seems we should multiply by 0.99...
                                tas = 1.0e11 / (511.0 - (float)t2d->ntap) * 511.0 / 25000.0 / 2.0 * probe->resolutionM;
                                if (t2d->div10 == 1)
                                    tas /= 10.0;
                            }
                            if (stype == TWOD_IMGv3_TYPE) {
                                unsigned short *sp = (unsigned short *)dp;
                                tas = (float)*sp / 10.0;
                            }

                            // Encode true airspeed to the ADS1 / ADS2 format for
                            // backwards compatability.
                            record.tas = htons((short)tas);

                            record.overld = htons(0);
                            dp++;      // skip over tas field
                            ::memcpy(record.data, dp, P2D_DATA);

                            // For old 2D probes, not Fast 2DC.
                            if (::memcmp(record.data, overLoadSync, 2) == 0)
                            {
                                unsigned long * lp = (unsigned long *)record.data;
                                record.overld = htons((ntohl(*lp) & 0x0000ffff) / 2000);
                                probe->hasOverloadCount++;
                            }

                            ++probe->recordCount;
                            size_t partCnt = countParticles(probe, (const unsigned char *)&record);
                            size_t diodeCnt = computeDiodeCount(probe, record.data);
                            if (partCnt < minNumberParticlesRequired)
                                ++probe->rejectTooFewParticleCount;
                            if (diodeCnt < probe->nDiodes / 2)
                                ++probe->rejectTooFewDiodesCount;

                            /* Output record if:
                             *   1) copying all records (-a).
                             *   2) we meet minimum number particles in the record (default 5)
                             *   3) at least 50% of the diodes had a count in them
                             */
                            if (copyAllRecords ||
                               (partCnt >= minNumberParticlesRequired && diodeCnt >= probe->nDiodes / 2))
                                outFile.write((char *)&record, sizeof(record));
                            else
                                ++probe->rejectRecordCount;
                        }
                        if (samp->getDataByteLength() == 4121) {    // SPEC compressed data.
                            const int * dp = (const int *) samp->getConstVoidDataPtr();
                            P2d_rec record;
                            Probe *probe = probeList[id];

                            record.id = probe->id;
                            setTimeStamp(record, samp);

                            ++probe->recordCount;
                            outFile.write((char *)dp, 4121);
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
                cout << probe->rejectRecordCount
                 << " records were rejected." << endl;

                cout.width(10);
                cout << probe->rejectTooFewParticleCount
                 << " were rejected due to fewer than "
                 << minNumberParticlesRequired
                 << " particles per record." << endl;

                cout.width(10);
                cout << probe->rejectTooFewDiodesCount
                 << " were rejected due to fewer than "
                 << "50%"
                 << " diodes being triggered per record." << endl;
            }

            if (!copyAllRecords) {
                cout.width(10);
                cout << probe->inDOF << " / " << probe->totalParticles
                 << " total particles, " << fixed << setw(4) << setprecision(1)
                 << (float)probe->inDOF / probe->totalParticles * 100.0
                 << "\% in DOF.\n";
            }

            if (probe->sensor->getCatalogName().compare(0, 7, "Fast2DC")) {
                cout.width(10);
                cout << probe->hasOverloadCount
                 << " records had an overload word @ spot zero, or "
                 << probe->hasOverloadCount * 100 / probe->recordCount
                 << "%." << endl;
            }

            cout << endl;

            if (outputDiodeCount) {
                unsigned long sum = 0;
                for (size_t i = 0; i < probe->nDiodes; ++i) {
                    sum += probe->diodeCount[i];
                    cout << "    Diode ";
                    cout.width(2);
                    cout << i << " count = ";
                    cout.width(10);
                    cout << probe->diodeCount[i] << endl;
                }
                cout << "           Average = "; cout.width(10);
                cout << sum / probe->nDiodes << endl << endl;
            }

            if (outputParticleCount) {
                unsigned long sum = 0;
                unsigned long sumP = 0;
                for (size_t i = 0; i < 512; ++i) {
                    if (probe->particleCount[i] > 0) {
                        sum += probe->particleCount[i];
                        sumP += (probe->particleCount[i] * i);
                        cout.width(10);
                        cout << probe->particleCount[i];
                        cout << " records have " << i;
                        cout << " particles." << endl;
                    }
                }
                cout << "Average particles per record = ";
                cout << sumP / sum << endl << endl;
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

size_t ExtractFast2D::computeDiodeCount(Probe * probe, const unsigned char * record)
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
      if (memcmp(slice, Fast2DsyncStr, 2) == 0 ||
          memcmp(slice, FastOverloadSync, 2) == 0 || (probe->nDiodes == 32 && slice[0] == 0x55))
        continue;

      for (size_t i = 0; i < nBytes; ++i)
      {
        for (size_t j = 0; j < 8; ++j) // 8 bits.
          if (((slice[i] << j) & 0x80) == 0x00) {
            probe->diodeCount[i*8 + j]++;
            diodeCnt[i*8 + j]++;
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


size_t ExtractFast2D::countParticles(Probe * probe, const unsigned char * record)
{
    size_t totalCnt = 0, dofCnt = 0, missCnt = 0;
    const P2d_rec *rec = (const P2d_rec *)record;
    const unsigned char *p = rec->data;

    if (probe->nDiodes == 32)
        for (size_t i = 0; i < 4095; ++i, ++p) {
            if (*p == 0x55) {
                ++totalCnt;
                if ((i % 4) != 0)
                    ++missCnt;
            }
        }

    if (probe->nDiodes == 64) {
        unsigned char dof_flag_mask = 0x01;
        if (probe->sensor->getCatalogName().find("_v2") != std::string::npos)
            dof_flag_mask = 0x10;

        for (size_t i = 0; i < 4093; ++i) {
            /* We only compare 2 bytes and not 3 since in PREDICT the probe started
             * having a stuck bit in the third byte of the sync word.
             */
            if (::memcmp(&p[i], Fast2DsyncStr, 2) == 0) {
                ++totalCnt;
                if ((p[i+2] & dof_flag_mask) == 0)
                    ++dofCnt;
                if ((i % 8) != 0)
                    ++missCnt;
                i += 7;	// Skip rest of particle.
            }
            else
            if (::memcmp(&p[i], FastOverloadSync, 2) == 0) {
                i += 7;	// Skip rest of particle.
            }
        }
    }

    ++probe->particleCount[totalCnt];
    probe->totalParticles += totalCnt;
    probe->inDOF += dofCnt;

    if (missCnt > 1)
    {
        char msg[200];
        sprintf(msg,
		" miss-aligned data, %02d:%02d:%02d.%03d, rec #%zd, total sync=%zd, missAligned count=%zd",
		ntohs(rec->hour), ntohs(rec->minute), ntohs(rec->second),
		ntohs(rec->msec), probe->recordCount, totalCnt, missCnt);
        cout << probe->sensor->getCatalogName() << probe->sensor->getSuffix() << msg << endl;
    }

    return totalCnt;
}
