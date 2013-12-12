#ifndef AUTOCALCLIENT_H
#define AUTOCALCLIENT_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleClient.h>
#include <nidas/linux/ncar_a2d.h>

#include <map>
#include <list>
#include <vector>
#include <string>

#include <QObject>

#define NUM_NCAR_A2D_CHANNELS         8       // Number of A/D's per card
#define NSAMPS 100
//#define SIMULATE

using namespace nidas::core;
using namespace std;

enum stateEnum { GATHER, DONE, DEAD };

enum fillState { SKIP, PEND, EMPTY, FULL };

/**
 * @class AutoCalClient
 * Hodge podge class.  Generates QtTree of DSMs for the Wizard, collects
 * data from dsm_server, progress bar, stores and displays results from all
 * analog cards.
 */
class AutoCalClient: public QObject, public SampleClient
{
    Q_OBJECT

public:

    AutoCalClient();

    /// Implementation of SampleClient::flush().
    void flush() throw() {}

    /// Reassemble a nidas dsmId and sensorId back into id.
    dsm_sample_id_t id(unsigned int dsmId, unsigned int devId);

    void setTestVoltage(int dsmId, int devId);

    bool readCalFile(DSMSensor* sensor);

    ncar_a2d_setup GetA2dSetup(int dsmId, int devId);

    bool Setup(DSMSensor* sensor);

    void createQtTreeModel( map<dsm_sample_id_t, string>dsmLocations );

    enum stateEnum SetNextCalVoltage(enum stateEnum state);

    bool receive(const Sample* samp) throw();

    bool Gathered();

    void DisplayResults();

    int maxProgress() { return nLevels * NSAMPS + 1; };

    string GetTreeModel() { return QTreeModel.str(); };

    // Save all cards at once.
    void SaveAllCalFiles();

    // Save an individual analog card.
    void SaveCalFile(uint dsmId, uint devId);

    list<int> GetVoltageLevels();

    list<int> GetVoltageLevels(uint dsmId, uint devId, uint chn);

    string GetVarName(uint dsmId, uint devId, uint chn);

    string GetOldTimeStamp(uint dsmId, uint devId, uint chn);
    string GetNewTimeStamp(uint dsmId, uint devId, uint chn);

    float GetOldTemperature(uint dsmId, uint devId, uint chn);
    float GetNewTemperature(uint dsmId, uint devId, uint chn);

    float GetOldIntcp(uint dsmId, uint devId, uint chn);
    float GetNewIntcp(uint dsmId, uint devId, uint chn);

    float GetOldSlope(uint dsmId, uint devId, uint chn);
    float GetNewSlope(uint dsmId, uint devId, uint chn);

    unsigned int nLevels;

    int progress;

    typedef map<uint, enum fillState>  channel_a_type; // indexed by chn
    typedef map<uint, channel_a_type>  device_a_type;  // indexed by devId
    typedef map<uint, device_a_type>   dsm_a_type;     // indexed by dsmId
    typedef map< int, dsm_a_type>      level_a_type;   // indexed by level

    /// calActv[level][dsmId][devId][chn]
    level_a_type calActv;

    /// testData[dsmId][devId][chn]
    map<uint, map<uint, map<uint, float > > > testData;

signals:
    void dispVolts();
    void errMessage(const QString& message);
    void updateSelection();

public slots:
    void TestVoltage(int channel, int level);

private:
    string ChnSetDesc(unsigned int val);

    bool testVoltage;
    int tvDsmId;
    int tvDevId;

    ostringstream QTreeModel;
    ostringstream QStrBuf;

    dsm_time_t lastTimeStamp;

    /// voltageLevels["GB"]   indexed by "1T", "2F", "2T", or "4F"
    map<string, list <int> > voltageLevels;

    struct sA2dSampleInfo {
        uint dsmId;
        uint devId;
        uint rate;
        bool isaTemperatureId;
        map<uint, uint> channel;                       // indexed by varId
    };
    map<dsm_sample_id_t, sA2dSampleInfo> sampleInfo;   // indexed by sampId

    map<uint, map<uint, dsm_sample_id_t > > temperatureId;

    map<uint, string> dsmNames;                        // indexed by dsmId
    map<uint, string> devNames;                        // indexed by devId
    map< int, uint>   slowestRate;                     // indexed by level

    typedef vector<float>             data_d_type;

    typedef map< int, data_d_type>     level_d_type;   // indexed by level
    typedef map<uint, level_d_type>    channel_d_type; // indexed by chn
    typedef map<uint, channel_d_type> device_d_type;   // indexed by devId
    typedef map<uint, device_d_type>  dsm_d_type;      // indexed by dsmId

    /// calData[dsmId][devId][chn][level]
    dsm_d_type calData;

    /// index to active voltage level
    int idxVltLvl;

    /// active voltage level
    int VltLvl;

    /// isNAN[dsmId][devId][chn][level]
    map<uint, map<uint, map<uint, map<uint, bool> > > > isNAN;

    /// calFilePath[dsmId][devId]
    map<uint, map<uint, string > > calFilePath;

    /// calFileName[dsmId][devId]
    map<uint, map<uint, string > > calFileName;

    /// calFileSaved[dsmId][devId]
    map<uint, map<uint, bool > > calFileSaved;

    /// calFileResults[dsmId][devId]
    map<uint, map<uint, string > > calFileResults;

    /// resultTemperature[dsmId][devId]
    map<uint, map<uint, float > > resultTemperature;

    /// temperatureData[dsmId][devId]
    map<uint, map<uint, data_d_type > > temperatureData;

    /// VarNames[dsmId][devId][chn]
    map<uint, map<uint, map<uint, string> > > VarNames;

    /// Gains[dsmId][devId][chn]
    map<uint, map<uint, map<uint, int> > > Gains;

    /// Bplrs[dsmId][devId][chn]
    map<uint, map<uint, map<uint, int> > > Bplrs;

    /// timeStamp[dsmId][devId][chn]
    map<uint, map<uint, map<uint, dsm_time_t> > > timeStamp;

    /// calFileTime[dsmId][devId][gain][bplr]
    map<uint, map<uint, map<uint, map<uint, dsm_time_t > > > > calFileTime;

    /// calFileIntcp[dsmId][devId][chn][gain][bplr]
    map<uint, map<uint, map<uint, map<uint, map<uint, float > > > > > calFileIntcp;

    /// calFileSlope[dsmId][devId][chn][gain][bplr]
    map<uint, map<uint, map<uint, map<uint, map<uint, float > > > > > calFileSlope;

    /// resultIntcp[dsmId][devId][chn][gain][bplr]
    map<uint, map<uint, map<uint, map<uint, map<uint, float > > > > > resultIntcp;

    /// resultSlope[dsmId][devId][chn][gain][bplr]
    map<uint, map<uint, map<uint, map<uint, map<uint, float > > > > > resultSlope;

    level_a_type::iterator    iLevel;
    dsm_a_type::iterator      iDsm;
    device_a_type::iterator   iDevice;
    channel_a_type::iterator  iChannel;
};

#endif
