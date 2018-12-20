#ifndef _nidas_dynld_raf_extract2d_h_
#define _nidas_dynld_raf_extract2d_h_


#include <nidas/core/Project.h>
#include <nidas/core/FileSet.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/HeaderSource.h>
#include <nidas/util/UTime.h>
#include <nidas/util/util.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>

namespace nidas { namespace dynld { namespace raf {


#define TWOD_BUFFER_SIZE	(4096)

// PADS record format.  This is what we will repackage the records into.
struct PADS_rec
{
  int16_t year;
  int16_t month;
  int16_t day;
  int16_t hour;
  int16_t minute;
  int16_t second;
  int16_t msec;                         // msec of this record
  int16_t wday;                         // overload time, msec
  unsigned char data[TWOD_BUFFER_SIZE]; // image buffer
};
typedef struct PADS_rec PADS_rec;


// ADS record format.  This is what we will repackage the records into.
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
  unsigned char data[TWOD_BUFFER_SIZE]; // image buffer
};
typedef struct P2d_rec P2d_rec;


using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;


class Probe
{
public:
  Probe() :
      sensor(0), resolution(0), resolutionM(0.0), id(0), serialNumber(),
      hasOverloadCount(0), nDiodes(64), recordCount(0), rejectRecordCount(0),
      rejectTooFewParticleCount(0), rejectTooFewDiodesCount(0),
      diodeCount(), particleCount(), totalParticles(0), inDOF(0)
    {
    memset(diodeCount, 0, sizeof(diodeCount));
    memset(particleCount, 0, sizeof(particleCount));
    }

  // Input info.
  DSMSensor * sensor;

  // Output info.
  size_t resolution;
  float resolutionM;
  short id;
  string serialNumber;

  // File info.
  size_t hasOverloadCount;

  // Number of diodes, 64 for Fast2D, 32 for old probes.
  size_t nDiodes;

  // Total number of records found in file.
  size_t recordCount;

  // Total number of records rejected.
  size_t rejectRecordCount;
  size_t rejectTooFewParticleCount;
  size_t rejectTooFewDiodesCount;

  /* Count of each diode value of 1 for the entire flight, sync words excluded.
   * This diagnostic output helps find which diode is bad when the probe runs away.
   */
  size_t diodeCount[64];        // 64 is max possible diodes.

  /* Count of number of particles per record.
   * This diagnostic output helps find which diode is bad when the probe runs away.
   */
  size_t particleCount[512];    // 512 is max possible slices/record.

  size_t totalParticles;
  size_t inDOF;

private:
    Probe(const Probe&);
    Probe& operator=(const Probe&);
};



class Extract2D : public HeaderSource
{
public:

    Extract2D();

    virtual int parseRunstring(int argc, char** argv) throw();

    virtual int usage(const char* argv0);

    virtual int run() throw() = 0;

// static functions
    static void sigAction(int sig, siginfo_t* siginfo, void*);

    static void setupSignals();

    /**
     * @throws nidas::util::IOException
     **/
    void sendHeader(dsm_time_t,SampleOutput* out);

protected:

    /**
     * Count number of particles in a record, also report miss-aligned data.
     */
    virtual size_t countParticles(Probe * probe, const unsigned char * record) = 0;

    /**
     * Sum occluded diodes along the flight path.  This increments for the
     * whole flight.  Help find stuck bits.
     * @returns the number of diodes that had positives in the record.
     */
    virtual size_t computeDiodeCount(Probe * probe, const unsigned char * record) = 0;

    /**
     * Decode Sample time tag and place into outgoing record.
     */
    void setTimeStamp(P2d_rec & record, Sample *samp);
    void setTimeStamp(PADS_rec & record, Sample *samp);

    static bool interrupted;

    bool outputHeader;

    /// Whether to output diode count histogram.
    bool outputDiodeCount;

    /// Whether to output particle count histogram.
    bool outputParticleCount;

    /// Copy 100% of 2D records from source file to output file, no filtering.
    bool copyAllRecords;

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

}}}
#endif
