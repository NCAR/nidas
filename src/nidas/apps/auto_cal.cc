/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

// #define _XOPEN_SOURCE        /* glibc2 needs this */

#include <ctime>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <iomanip>

#include <nidas/linux/ncar_a2d.h>

#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>

#include <nidas/core/Socket.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/core/XmlRpcThread.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>

#include <nidas/dynld/RawSampleInputStream.h>

#include <gsl/gsl_statistics_float.h>
#include <gsl/gsl_fit.h>

#include <sys/stat.h>

#define NSAMPS 100

#define DEBUG
#ifdef DEBUG
#include <nidas/core/FileSet.h>
#endif

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;
using namespace XmlRpc;

typedef unsigned char   uchar;

namespace n_u = nidas::util;

string fillStateDesc[] = {"SKIP", "PEND", "EMPTY", "FULL" };

enum stateEnum { DONE, GATHER, DEAD };
string stateEnumDesc[] = {"DONE", "GATHER", "DEAD" };

string ChnSetDesc[] = {
  "- - - - - - - -", "- - - - - - - X", "- - - - - - X -", "- - - - - - X X",
  "- - - - - X - -", "- - - - - X - X", "- - - - - X X -", "- - - - - X X X",
  "- - - - X - - -", "- - - - X - - X", "- - - - X - X -", "- - - - X - X X",
  "- - - - X X - -", "- - - - X X - X", "- - - - X X X -", "- - - - X X X X",
  "- - - X - - - -", "- - - X - - - X", "- - - X - - X -", "- - - X - - X X",
  "- - - X - X - -", "- - - X - X - X", "- - - X - X X -", "- - - X - X X X",
  "- - - X X - - -", "- - - X X - - X", "- - - X X - X -", "- - - X X - X X",
  "- - - X X X - -", "- - - X X X - X", "- - - X X X X -", "- - - X X X X X",
  "- - X - - - - -", "- - X - - - - X", "- - X - - - X -", "- - X - - - X X",
  "- - X - - X - -", "- - X - - X - X", "- - X - - X X -", "- - X - - X X X",
  "- - X - X - - -", "- - X - X - - X", "- - X - X - X -", "- - X - X - X X",
  "- - X - X X - -", "- - X - X X - X", "- - X - X X X -", "- - X - X X X X",
  "- - X X - - - -", "- - X X - - - X", "- - X X - - X -", "- - X X - - X X",
  "- - X X - X - -", "- - X X - X - X", "- - X X - X X -", "- - X X - X X X",
  "- - X X X - - -", "- - X X X - - X", "- - X X X - X -", "- - X X X - X X",
  "- - X X X X - -", "- - X X X X - X", "- - X X X X X -", "- - X X X X X X",
  "- X - - - - - -", "- X - - - - - X", "- X - - - - X -", "- X - - - - X X",
  "- X - - - X - -", "- X - - - X - X", "- X - - - X X -", "- X - - - X X X",
  "- X - - X - - -", "- X - - X - - X", "- X - - X - X -", "- X - - X - X X",
  "- X - - X X - -", "- X - - X X - X", "- X - - X X X -", "- X - - X X X X",
  "- X - X - - - -", "- X - X - - - X", "- X - X - - X -", "- X - X - - X X",
  "- X - X - X - -", "- X - X - X - X", "- X - X - X X -", "- X - X - X X X",
  "- X - X X - - -", "- X - X X - - X", "- X - X X - X -", "- X - X X - X X",
  "- X - X X X - -", "- X - X X X - X", "- X - X X X X -", "- X - X X X X X",
  "- X X - - - - -", "- X X - - - - X", "- X X - - - X -", "- X X - - - X X",
  "- X X - - X - -", "- X X - - X - X", "- X X - - X X -", "- X X - - X X X",
  "- X X - X - - -", "- X X - X - - X", "- X X - X - X -", "- X X - X - X X",
  "- X X - X X - -", "- X X - X X - X", "- X X - X X X -", "- X X - X X X X",
  "- X X X - - - -", "- X X X - - - X", "- X X X - - X -", "- X X X - - X X",
  "- X X X - X - -", "- X X X - X - X", "- X X X - X X -", "- X X X - X X X",
  "- X X X X - - -", "- X X X X - - X", "- X X X X - X -", "- X X X X - X X",
  "- X X X X X - -", "- X X X X X - X", "- X X X X X X -", "- X X X X X X X",
  "X - - - - - - -", "X - - - - - - X", "X - - - - - X -", "X - - - - - X X",
  "X - - - - X - -", "X - - - - X - X", "X - - - - X X -", "X - - - - X X X",
  "X - - - X - - -", "X - - - X - - X", "X - - - X - X -", "X - - - X - X X",
  "X - - - X X - -", "X - - - X X - X", "X - - - X X X -", "X - - - X X X X",
  "X - - X - - - -", "X - - X - - - X", "X - - X - - X -", "X - - X - - X X",
  "X - - X - X - -", "X - - X - X - X", "X - - X - X X -", "X - - X - X X X",
  "X - - X X - - -", "X - - X X - - X", "X - - X X - X -", "X - - X X - X X",
  "X - - X X X - -", "X - - X X X - X", "X - - X X X X -", "X - - X X X X X",
  "X - X - - - - -", "X - X - - - - X", "X - X - - - X -", "X - X - - - X X",
  "X - X - - X - -", "X - X - - X - X", "X - X - - X X -", "X - X - - X X X",
  "X - X - X - - -", "X - X - X - - X", "X - X - X - X -", "X - X - X - X X",
  "X - X - X X - -", "X - X - X X - X", "X - X - X X X -", "X - X - X X X X",
  "X - X X - - - -", "X - X X - - - X", "X - X X - - X -", "X - X X - - X X",
  "X - X X - X - -", "X - X X - X - X", "X - X X - X X -", "X - X X - X X X",
  "X - X X X - - -", "X - X X X - - X", "X - X X X - X -", "X - X X X - X X",
  "X - X X X X - -", "X - X X X X - X", "X - X X X X X -", "X - X X X X X X",
  "X X - - - - - -", "X X - - - - - X", "X X - - - - X -", "X X - - - - X X",
  "X X - - - X - -", "X X - - - X - X", "X X - - - X X -", "X X - - - X X X",
  "X X - - X - - -", "X X - - X - - X", "X X - - X - X -", "X X - - X - X X",
  "X X - - X X - -", "X X - - X X - X", "X X - - X X X -", "X X - - X X X X",
  "X X - X - - - -", "X X - X - - - X", "X X - X - - X -", "X X - X - - X X",
  "X X - X - X - -", "X X - X - X - X", "X X - X - X X -", "X X - X - X X X",
  "X X - X X - - -", "X X - X X - - X", "X X - X X - X -", "X X - X X - X X",
  "X X - X X X - -", "X X - X X X - X", "X X - X X X X -", "X X - X X X X X",
  "X X X - - - - -", "X X X - - - - X", "X X X - - - X -", "X X X - - - X X",
  "X X X - - X - -", "X X X - - X - X", "X X X - - X X -", "X X X - - X X X",
  "X X X - X - - -", "X X X - X - - X", "X X X - X - X -", "X X X - X - X X",
  "X X X - X X - -", "X X X - X X - X", "X X X - X X X -", "X X X - X X X X",
  "X X X X - - - -", "X X X X - - - X", "X X X X - - X -", "X X X X - - X X",
  "X X X X - X - -", "X X X X - X - X", "X X X X - X X -", "X X X X - X X X",
  "X X X X X - - -", "X X X X X - - X", "X X X X X - X -", "X X X X X - X X",
  "X X X X X X - -", "X X X X X X - X", "X X X X X X X -", "X X X X X X X X",
};

class CalibrationClient: public SampleClient
{
public:

    bool Setup( list<DSMSensor*> allSensors );

    enum stateEnum SetNextCalVoltage(int grouped);

    bool receive(const Sample* samp) throw();

    bool Gathered();

    void DisplayResults();

private:

    dsm_time_t lastTimeStamp;

    struct sA2dSampleInfo {
        uint dsmId;
        uint devId;
        bool isaTemperatureId;
        map<uint, uint> channel;                       // indexed by varId
    };
    map<dsm_sample_id_t, sA2dSampleInfo> sampleInfo;   // indexed by sampId

    map<uint, map<uint, dsm_sample_id_t > > temperatureId;

    map<uint, string> dsmNames;                        // indexed by dsmId
    map<uint, string> devNames;                        // indexed by devId

    enum fillState {SKIP, PEND, EMPTY, FULL };

    typedef map<uint, enum fillState>  channel_a_type; // indexed by channel
    typedef map<uint, channel_a_type>  device_a_type;  // indexed by devId
    typedef map<uint, device_a_type>   dsm_a_type;     // indexed by dsmId
    typedef map<uint, dsm_a_type>      level_a_type;   // indexed by level

    //  calActv[nLevels][nDSMs][nDevices][nChannels]
    level_a_type calActv;

    typedef vector<float>             data_d_type;

    typedef map<uint, data_d_type>     level_d_type;   // indexed by level
    typedef map<uint, level_d_type>    channel_d_type; // indexed by channel
    typedef map<uint, channel_d_type> device_d_type;   // indexed by devId
    typedef map<uint, device_d_type>  dsm_d_type;      // indexed by dsmId

    //  calData[nDSMs][nDevices][nChannels][nLevels]
    dsm_d_type calData;

    //  Gains[nDSMs][nDevices][nChannels]
    map<uint, map<uint, map<uint, int> > > Gains;

    //  Ofsts[nDSMs][nDevices][nChannels]
    map<uint, map<uint, map<uint, int> > > Ofsts;

    struct CalFileInfo {
        string name;
        string timeFormat;
    };
    map<uint, map<uint, CalFileInfo > > calFileInfo;   // indexed by dsmId, devId

    map<uint, map<uint, dsm_time_t > > timeStamp;      // indexed by dsmId, devId

    //  temperatureData[nDSMs][nDevices]
    map<uint, map<uint, data_d_type > > temperatureData;

    int  VltLvl;  // active voltage level

    level_a_type::iterator    iLevel;
    dsm_a_type::iterator      iDsm;
    device_a_type::iterator   iDevice;
    channel_a_type::iterator  iChannel;
};

bool CalibrationClient::Setup(list<DSMSensor*> allSensors)
{
    cout << endl << "CalibrationClient::Setup()" << endl;

    map<string, list <int> > voltageLevels;
    list <int> volts;

    volts.push_back(0);
    volts.push_back(1);
    volts.push_back(5);
    voltageLevels["4F"] = volts;
    voltageLevels["2T"] = volts;

    volts.push_back(10);
    voltageLevels["2F"] = volts;

    volts.push_back(-10);
    voltageLevels["1T"] = volts;

    // set up
    list<list<DSMSensor*>::iterator> pruneSen;
    list<DSMSensor*>::iterator si;

    for (si = allSensors.begin(); si != allSensors.end(); si++) {
        DSMSensor* sensor = *si;
        string dsmName = sensor->getDSMName();
        string devName = sensor->getDeviceName();
        uint dsmId = sensor->getDSMId();
        uint devId = sensor->getSensorId();
        cout << "AAA " << dsmName << ":" << devName;
        cout << " " << dsmId << ":" << devId << endl;
    }
    for (si = allSensors.begin(); si != allSensors.end(); si++) {
        DSMSensor* sensor = *si;

        string dsmName = sensor->getDSMName();
        string devName = sensor->getDeviceName();

        uint dsmId = sensor->getDSMId();
        uint devId = sensor->getSensorId();

        cout << "  " << dsmName << ":" << devName << endl;

        // extract the A2D board serial number
        CalFile *cf = sensor->getCalFile();
        if (cf) {
            calFileInfo[dsmId][devId].name       = cf->getFile();
            cout << "  calFileInfo[" << dsmId << "][" << devId << "].name = ";
            cout << calFileInfo[dsmId][devId].name << endl;

            float data[60];
            n_u::UTime tlast((time_t)0);

            for (;;) {
                n_u::UTime t = cf->readTime();
                if (cf->eof()) break;
                int n = cf->readData(data,sizeof data/sizeof data[0]);
                cout << t.format(true,"%Y %m %d %H:%M:%S.%3f %Z ");
                for (int i = 0; i < n; i++) cout << data[i] << ' ';
                cout << endl;
                if (t < tlast) cout << "backwards time at " <<
                    t.format(true,"%Y %m %d %H:%M:%S.%3f %Z ") <<
                    " nline=" << cf->getLineNumber() << endl;
                tlast = t;
            }
            cout << "  re-opening CalFile" << endl;
            if (cf->eof()) {
                cf->close();
                cf->open();
            }
            calFileInfo[dsmId][devId].timeFormat = cf->getDateTimeFormat();
            cout << "  calFileInfo[" << dsmId << "][" << devId << "].timeFormat = ";
            cout << calFileInfo[dsmId][devId].timeFormat << endl;
        }
        else {
            cout << "  CalFile not set!" << endl;
            cout << "  PRUNING: " << dsmName << ":" << devName << endl;
            pruneSen.push_back(si);
            continue;
        }
#ifndef DEBUG
        // establish an xmlrpc connection to this DSM
        XmlRpcClient dsm_xmlrpc_client(dsmName.c_str(),
                                       DSM_XMLRPC_PORT_TCP, "/RPC2");

        // fetch the current setup from the card itself
        XmlRpcValue get_params, get_result;
        get_params["device"] = devName;
        cout << "  get_params: " << get_params.toXml() << endl;
        ncar_a2d_setup setup;

        if (dsm_xmlrpc_client.execute("GetA2dSetup", get_params, get_result)) {
            if (dsm_xmlrpc_client.isFault()) {
                cout << "  xmlrpc client fault: " << get_result["faultString"] << endl;
                cout << "  PRUNING: " << dsmName << ":" << devName << endl;
                pruneSen.push_back(si);
                dsm_xmlrpc_client.close();
                continue;
            }
            dsm_xmlrpc_client.close();
            for (uint i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                setup.gain[i]   = get_result["gain"][i];
                setup.offset[i] = get_result["offset"][i];
                setup.calset[i] = get_result["calset"][i];
                cout << "setup.gain["  << i << "]   = " << setup.gain[i] << endl;
                cout << "setup.offset["  << i << "] = " << setup.offset[i] << endl;
                cout << "setup.calset["  << i << "] = " << setup.calset[i] << endl;
            }
            setup.vcal = get_result["vcal"];
            cout << "setup.vcal = " << setup.vcal << endl;
#ifdef DONT_IGNORE_ACTIVE_CARDS
            if (setup.vcal != -99) {
                // TODO ensure that a -99 is reported back by the driver when nothing is active.
                cout << "  A calibration voltage is active here.  Cannot auto calibrate this." << endl;
                cout << "  PRUNING: " << dsmName << ":" << devName << endl;
                pruneSen.push_back(si);
                continue;
            }
#endif
        }
        else {
            cout << "  1 COVERED " << "xmlrpc client NOT responding" << endl;
            cout << "  PRUNING: " << dsmName << ":" << devName << endl;
            pruneSen.push_back(si);
            continue;
        }
        cout << "  2 COVERED " << "get_result: " << get_result.toXml() << endl;
#endif
        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag * tag = ti.next();
            dsm_sample_id_t sampId = tag->getId();

            bool goodSample = true;
            uint varId = 0;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable * var = vi.next();

                int chan = var->getA2dChannel();
                if (chan < 0) {
                    temperatureId[dsmId][devId] = sampId;
                    continue;
                }
                uint channel = chan;

                int gain=1, offset=0;

                const Parameter * parm;
                parm = var->getParameter("gain");
                if (parm)
                    gain = (int)parm->getNumericValue(0);

                parm = var->getParameter("bipolar");
                if (parm)
                    offset = (int)(parm->getNumericValue(0));
#ifndef DEBUG
                // compare with what is currently configured
                if ( (setup.gain[channel] != gain) || (setup.offset[channel] != !offset) ) {
                    cout << "can not calibrate channel " << channel << " because it is running as: "
                         << setup.gain[channel] << (setup.offset[channel] ? "T" : "F")
                         << " but configured as: "
                         << gain << (offset ? "T" : "F") << endl;
                         << "(you need to reboot this DSM)" << endl
                         << "ignoring: " << dsmName << ":" << devName;

                    goodSample = false;
                    pruneSen.push_back(si);
                    break;
                }
#endif
                // channel is available
                ostringstream go;
                go << gain << (offset ? "T" : "F");

                sampleInfo[sampId].channel[varId++] = channel;

                Gains[dsmId][devId][channel] = gain;
                Ofsts[dsmId][devId][channel] = offset;

                list<int>::iterator l;
                for ( l =  voltageLevels[go.str()].begin(); l != voltageLevels[go.str()].end(); l++) {

                    calActv[*l][dsmId][devId][channel] = PEND;
                    calData[dsmId][devId][channel][*l].reserve( NSAMPS * sizeof(float) );

                    cout << sampId;
                    cout << " CcalActv[" << *l << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[*l][dsmId][devId][channel] ] << endl;
                }
            }
            if (goodSample) {
                sampleInfo[sampId].dsmId = dsmId;
                sampleInfo[sampId].devId = devId;
                sampleInfo[sampId].isaTemperatureId = false;
                sampleInfo[temperatureId[dsmId][devId]].isaTemperatureId = true;
                temperatureData[dsmId][devId].reserve( NSAMPS * sizeof(float) );
            }
        }
        dsmNames[dsmId] = dsmName;
        devNames[devId] = devName;
        timeStamp[dsmId][devId] = 0;
    }
    lastTimeStamp = 0;

    cout << "Prune out non-responsive or miss configured cards." << endl;
    // Prune out non-responsive or miss configured cards.
    pruneSen.unique();
    list<list<DSMSensor*>::iterator>::iterator iSen;
    for (iSen = pruneSen.begin(); iSen != pruneSen.end(); iSen++)
    {
        DSMSensor* sensor = **iSen;

        string dsmName = sensor->getDSMName();
        string devName = sensor->getDeviceName();

        uint dsmId = sensor->getDSMId();
        uint devId = sensor->getSensorId();

        cout << "PRUNING " << dsmName << ":" << devName;
        cout << " " << dsmId << ":" << devId << endl;

        allSensors.erase( *iSen );
    }
    for (si = allSensors.begin(); si != allSensors.end(); si++) {
        DSMSensor* sensor = *si; 
        string dsmName = sensor->getDSMName();
        string devName = sensor->getDeviceName(); 
        uint dsmId = sensor->getDSMId();
        uint devId = sensor->getSensorId(); 
        cout << "BBB " << dsmName << ":" << devName;
        cout << " " << dsmId << ":" << devId << endl;
    }

    cout << "...................................................................\n";

    // for each level
    for (iLevel  = calActv.begin();
         iLevel != calActv.end(); iLevel++) {

        int level        =   iLevel->first;
        dsm_a_type* Dsms = &(iLevel->second);
        cout << "" << level << endl;

        // for each DSM
        for (iDsm  = Dsms->begin();
             iDsm != Dsms->end(); iDsm++) {

            uint dsmId             =   iDsm->first;
            device_a_type* Devices = &(iDsm->second);
            cout << "  " << dsmId << endl;

            // for each device
            for (iDevice  = Devices->begin();
                 iDevice != Devices->end(); iDevice++) {

                uint devId               =   iDevice->first;
                channel_a_type* Channels = &(iDevice->second);
                cout << "    " << devId << endl;

                // for each channel
                for (iChannel  = Channels->begin();
                     iChannel != Channels->end(); iChannel++) {

                    uint  channel = iChannel->first;
                    cout << "      " << channel << endl;

                    cout << "BcalActv[" << level << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[level][dsmId][devId][channel] ] << endl;
                }
            }
        }
    }

    cout << "...................................................................\n";

    map<dsm_sample_id_t,struct sA2dSampleInfo>::iterator iSI;
    map<uint,uint>::iterator iC;
    for ( iSI  = sampleInfo.begin();
          iSI != sampleInfo.end(); iSI++ )
    {
        struct sA2dSampleInfo *SI = &(iSI->second);

        cout << iSI->first << " ";
        cout << "SI->dsmId " << SI->dsmId << " ";
        cout << "SI->devId " << SI->devId << " ";
        cout << "SI->isaTemperatureId " << SI->isaTemperatureId << " ";
        cout << "SI->channel ";
        for ( iC  = SI->channel.begin();
              iC != SI->channel.end(); iC++ )
            cout << " " << iC->second;
        cout << endl;
    }

    cout << "...................................................................\n";

    // start the Voltage level index
    iLevel = calActv.begin();
    return 0;
}


// This function shall be called repeatedly by until
// it returns DONE.  It is a re-entrant function that avances to
// the next calibration voltage level.
//
// This funcion iterates across the levels, dsmNames, devNames, and Channels.
//
enum stateEnum CalibrationClient::SetNextCalVoltage(int grouped)
{
    cout << "CalibrationClient::SetNextCalVoltage(" << grouped << ")" << endl;
    enum stateEnum state = GATHER;

    if (iLevel == calActv.end() ) {
        cout << "THATS ALL FOLKS!" << endl;
        iLevel = calActv.begin();
        state = DONE;
    }

    bool alive       = false;
    int level        =   iLevel->first;
    dsm_a_type* Dsms = &(iLevel->second);
    cout << "SNCV " << level << endl;
    VltLvl = level;

    // for each DSM
    for (iDsm  = Dsms->begin();
         iDsm != Dsms->end(); iDsm++) {

        uint dsmId             =   iDsm->first;
        device_a_type* Devices = &(iDsm->second);
        cout << "  " << dsmId << endl;

        // for each device
        for (iDevice  = Devices->begin();
             iDevice != Devices->end(); iDevice++) {

            uint devId               =   iDevice->first;
            channel_a_type* Channels = &(iDevice->second);
            cout << "    " << devId << endl;

#ifndef DEBUG
            XmlRpcClient dsm_xmlrpc_client(dsmNames[dsmId].c_str(),
                                           DSM_XMLRPC_PORT_TCP, "/RPC2");
#endif

            XmlRpcValue set_params, set_result;
            set_params["device"] = devNames[devId];
            uchar ChnSet = 0;

            if (state == DONE) {
                set_params["state"] = 0;
                set_params["voltage"] = 0;  // ignored
                ChnSet = 0xff;
            } else {
                set_params["state"] = 1;
                set_params["voltage"] = level;

                // for each channel
                for (iChannel  = Channels->begin();
                     iChannel != Channels->end(); iChannel++) {

                    uint  channel = iChannel->first;

                    ChnSet |= (1 << channel);
                    calActv[level][dsmId][devId][channel] = EMPTY;

                    cout << "      ";
                    cout << "ScalActv[" << level << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[level][dsmId][devId][channel] ] << endl;
                }
            }
            cout << "    ";
            cout << "XMLRPC ChnSet:    " << ChnSetDesc[ChnSet] << endl;
            set_params["calset"] = ChnSet;

#ifndef DEBUG
            cout << " set_params: " << set_params.toXml() << endl;

            // Instruct card to generate a calibration voltage.
            // DSMEngineIntf::TestVoltage::execute
            if (dsm_xmlrpc_client.execute("TestVoltage", set_params, set_result)) {
                if (dsm_xmlrpc_client.isFault()) {
                    cout << "xmlrpc client fault: " << set_result["faultString"] << endl;

                    // skip other cards owned by this DSM
                    dsm_xmlrpc_client.close();
                    break;
                }
            }
            else {
                cout << "xmlrpc client NOT responding" << endl;

                // skip other cards owned by this DSM
                dsm_xmlrpc_client.close();
                break;
            }
            dsm_xmlrpc_client.close();
            cout << "set_result: " << set_result.toXml() << endl;
#endif
            alive = true;
        }
    }
    struct timeval tv;
    ::gettimeofday(&tv,0);
    lastTimeStamp = (dsm_time_t)tv.tv_sec * USECS_PER_SEC + tv.tv_usec;

    if (!alive) return DEAD;

    // re-entrant for each level
    iLevel++;

    cout << "...................................................................\n";

    return state;
}

#define TDELAY 10 // time delay after setting a new voltage (seconds)

bool CalibrationClient::receive(const Sample* samp) throw()
{
    dsm_time_t currTimeStamp;

    struct timeval tv;
    ::gettimeofday(&tv,0);
    currTimeStamp = (dsm_time_t)tv.tv_sec * USECS_PER_SEC + tv.tv_usec;

#ifndef DEBUG
    if (currTimeStamp < lastTimeStamp + TDELAY * USECS_PER_SEC)
        return false;
#endif

    dsm_sample_id_t sampId = samp->getId();
    uint dsmId             = sampleInfo[sampId].dsmId;
    uint devId             = sampleInfo[sampId].devId;

    if (dsmId == 0) { cout << "dsmId == 0\n"; return false; }
    if (devId == 0) { cout << "devId == 0\n"; return false; }

    // timetag first data value received
    if (timeStamp[dsmId][devId] == 0)
        timeStamp[dsmId][devId] = currTimeStamp;

    cout << n_u::UTime(currTimeStamp).format(true,"%Y %m %d %H:%M:%S.%4f") << endl;
    cout << " CalibrationClient::receive " << sampId << " [" << VltLvl << "][" << dsmId << "][" << devId << "]" << endl;

    const float* fp =
            (const float*) samp->getConstVoidDataPtr();

    // store the card's onboard temperatureData
    // There is only one variable in this sample.
    if (sampleInfo[sampId].isaTemperatureId ) {

        // stop gathering after NSAMPS received
        if (temperatureData[dsmId][devId].size() > NSAMPS-1)
            return true;

        cout << "RtemperatureData[" << dsmId << "][" << devId << "].size() = ";
        cout << temperatureData[dsmId][devId].size();

        temperatureData[dsmId][devId].push_back(fp[0]);

        cout << " " << temperatureData[dsmId][devId].size() << endl;
        return true;
    }
    bool channelFound = false;
    // store the card's generated calibration
    // There are one or more variables in this sample.
    // varId
    for (uint varId = 0; varId < samp->getDataByteLength()/sizeof(float); varId++) {

        uint channel = sampleInfo[sampId].channel[varId];

        // ignore samples that are not currently being calibrated
        if ( calActv[VltLvl][dsmId][devId][channel] == EMPTY ) {
            channelFound = true;

#ifdef DEBUG
            calData[dsmId][devId][channel][VltLvl].push_back((double)VltLvl + ((channel+1) * 0.1) );
#else
            calData[dsmId][devId][channel][VltLvl].push_back(fp[varId]);
#endif

            cout << n_u::UTime(currTimeStamp).format(true,"%Y %m %d %H:%M:%S.%4f") << " ";
            cout << " sampId: " << sampId;
            cout << " value: " << setw(10) << fp[varId];
            cout << " RcalData[" << dsmId << "][" << devId << "][" << channel << "][" << VltLvl << "].size() = ";
            cout << calData[dsmId][devId][channel][VltLvl].size() << endl;

            // stop gathering after NSAMPS received
            if (calData[dsmId][devId][channel][VltLvl].size() > NSAMPS-1) {
                calActv[VltLvl][dsmId][devId][channel] = FULL;
                continue;
            }
        }
    }
    if ( !channelFound )
        return false;

    return true;
}


// This funcion checks to see if enough data was gathered.
//
bool CalibrationClient::Gathered()
{
    bool isGathered = false;

    map<dsm_sample_id_t,struct sA2dSampleInfo>::iterator iSI;
    for ( iSI  = sampleInfo.begin();
          iSI != sampleInfo.end(); iSI++ )
    {
        struct sA2dSampleInfo *SI = &(iSI->second);
        if ( SI->isaTemperatureId ) continue;

        map<uint,uint>::iterator iC;
        for ( iC  = SI->channel.begin();
              iC != SI->channel.end(); iC++ )
        {
            uint channel = iC->second;
            enum fillState fillstate = calActv[VltLvl][SI->dsmId][SI->devId][channel];

//          cout << "GcalActv[" << VltLvl << "][" << SI->dsmId << "][" << SI->devId << "][" << channel << "] = ";
//          cout << fillStateDesc[ fillstate ] << endl;

            if ( fillstate == FULL )
                isGathered = true;
            else if ( fillstate == EMPTY )
                return false;
        }
    }
    if (isGathered)
        cout << "CalibrationClient::Gathered" << endl;

    return isGathered;
}


void CalibrationClient::DisplayResults()
{
    cout << "CalibrationClient::DisplayResults" << endl;

    // for each level
    for (iLevel  = calActv.begin();
         iLevel != calActv.end(); iLevel++) {

        int level        =   iLevel->first;
        dsm_a_type* Dsms = &(iLevel->second);
        cout << "" << level << endl;

        // for each DSM
        for (iDsm  = Dsms->begin();
             iDsm != Dsms->end(); iDsm++) {

            uint dsmId             =   iDsm->first;
            device_a_type* Devices = &(iDsm->second);
            cout << "  " << dsmId << endl;

            // for each device
            for (iDevice  = Devices->begin();
                 iDevice != Devices->end(); iDevice++) {

                uint devId               =   iDevice->first;
                channel_a_type* Channels = &(iDevice->second);
                cout << "    " << devId << endl;

                // for each channel
                for (iChannel  = Channels->begin();
                     iChannel != Channels->end(); iChannel++) {

                    uint  channel = iChannel->first;
                    cout << "      " << channel << endl;

                    cout << "DcalActv[" << level << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[level][dsmId][devId][channel] ] << endl;
                }
            }
        }
    }

    cout << "...................................................................\n";

    typedef struct { int gain; int ofst; } GO_type;
    GO_type GO[] = {{1,1},{2,0},{2,1},{4,0}};

    dsm_d_type::iterator     iiDsm;
    device_d_type::iterator  iiDevice;
    channel_d_type::iterator iiChannel;
    level_d_type::iterator   iiLevel;
    data_d_type::iterator    iiData;

    vector<float> voltageMin;
    vector<float> voltageMax;

    // for each DSM
    for (iiDsm  = calData.begin();
         iiDsm != calData.end(); iiDsm++) {

        uint dsmId             =   iiDsm->first;
        device_d_type* Devices = &(iiDsm->second);

        // for each device
        for (iiDevice  = Devices->begin();
             iiDevice != Devices->end(); iiDevice++) {

            uint devId               =   iiDevice->first;
            channel_d_type* Channels = &(iiDevice->second);

            map<uint, double> c0;  // indexed by channel
            map<uint, double> c1;  // indexed by channel

            // for each channel
            for (iiChannel  = Channels->begin();
                 iiChannel != Channels->end(); iiChannel++) {

                uint channel          =   iiChannel->first;
                level_d_type* Levels = &(iiChannel->second);

                double aVoltageLevel, aVoltageMean, aVoltageWeight;
                double aVoltageMin, aVoltageMax;
                vector<double> voltageMean;
                vector<double> voltageLevel;
                vector<double> voltageWeight;

                // for each voltage level
                // NOTE these levels could be from for any (gain, offset) range.
                for (iiLevel  = Levels->begin();
                     iiLevel != Levels->end(); iiLevel++) {

                    int level         =   iiLevel->first;
                    data_d_type* Data = &(iiLevel->second);
                    size_t nPts = Data->size();
                    cout << "nPts:   " << nPts << endl;

                    // create a vector from the voltage levels
                    aVoltageLevel = static_cast<double>(level);
                    voltageLevel.push_back( aVoltageLevel );

                    // create a vector from the computed voltage min
                    aVoltageMin = gsl_stats_float_min(
                      &(*Data)[0], 1, nPts);
                    voltageMin.push_back( aVoltageMin );

                    // create a vector from the computed voltage max
                    aVoltageMax = gsl_stats_float_max(
                      &(*Data)[0], 1, nPts);
                    voltageMax.push_back( aVoltageMax );

                    // create a vector from the computed voltage means
                    aVoltageMean = gsl_stats_float_mean(
                      &(*Data)[0], 1, nPts);
                    voltageMean.push_back( aVoltageMean );

                    // create a vector from the computed voltage weights
                    aVoltageWeight = gsl_stats_float_variance(
                      &(*Data)[0], 1, nPts);
                    aVoltageWeight = (aVoltageWeight == 0.0) ? 1.0 : (1.0 / aVoltageWeight);
                    voltageWeight.push_back( aVoltageWeight );

                    cout << "   aVoltageLevel: "  << setprecision(7) << setw(12) << aVoltageLevel;
                    cout << " | aVoltageMin: "    << setprecision(7) << setw(12) << aVoltageMin;
                    cout << " | aVoltageMax: "    << setprecision(7) << setw(12) << aVoltageMax;
                    cout << " | aVoltageMean: "   << setprecision(7) << setw(12) << aVoltageMean;
                    cout << " | aVoltageWeight: " << setprecision(7) << setw(12) << aVoltageWeight;
                    cout << endl;
                    cout << "calData[" << dsmId << "][" << devId << "][" << channel << "][" << level << "]" << endl;
                    for (iiData  = Data->begin(); iiData != Data->end(); iiData++)
                        cout << setprecision(7) << setw(12) << *iiData;
                    cout << endl;
                }
                size_t nPts = voltageLevel.size();
                double cov00, cov01, cov11, chisq;

                vector<double>::iterator iVM = voltageMean.begin();
                cout << "channel: " << channel << endl;
                for ( ; iVM != voltageMean.end(); iVM++ )
                    cout << "iVM: " << *iVM << endl;
                cout << "voltageLevel.size(): " << nPts << endl;

                // compute weighted linear fit to the data
                gsl_fit_wlinear (&voltageMean[0], 1,
                                 &voltageWeight[0], 1,
                                 &voltageLevel[0], 1,
                                 nPts,
                                 &c0[channel], &c1[channel], &cov00, &cov01, &cov11, &chisq);
            }
            // compute temperature mean
            double temperatureMean =
              gsl_stats_float_mean(&(temperatureData[dsmId][devId][0]), 1,
                                     temperatureData[dsmId][devId].size());

            // record results to the device's CalFile
            ostringstream ostr;
            ostr << "#Ntemperature: "<< temperatureData[dsmId][devId].size() << endl;
            ostr << "# temperature: "<< temperatureMean << endl;
            ostr << "#  Date              Gain  Bipolar";
            //                          123456123456789
            //       2009 11 06 16:13:52
            for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++)
                //       1234      5     678901212345      6     789012";
                ostr << "  CH" << ix << "-offset   CH" << ix << "-slope";
            ostr << endl;

            // for each (gain, offset) range
            for (uint iGO=0; iGO<4; iGO++) {

                // find out if a channel was calibrated at this range
                bool found = false;
                for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++) {
                    if ( ( Channels->find(ix) != Channels->end() ) &&
                         ( Gains[dsmId][devId][ix] == GO[iGO].gain ) &&
                         ( Ofsts[dsmId][devId][ix] == GO[iGO].ofst ) ) {
                        found = true;
                        break;
                    }
                }
                // display calibrations that were performed at this range
                if ( found ) {
                    ostr << n_u::UTime(timeStamp[dsmId][devId]).format(true,"%Y %m %d %H:%M:%S");
                    ostr << setw(6) << dec << GO[iGO].gain;
                    ostr << setw(9) << dec << GO[iGO].ofst;

                    for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++) {
                        if ( ( Channels->find(ix) != Channels->end() ) &&
                             ( Gains[dsmId][devId][ix] == GO[iGO].gain ) &&
                             ( Ofsts[dsmId][devId][ix] == GO[iGO].ofst ) )

                            ostr << setprecision(7) << setw(12) << c0[ix]
                                 << setprecision(7) << setw(12) << c1[ix];
                        else
                            ostr << "         ---         ---";
//                          ostr << setprecision(7) << setw(12) << 0.0
//                               << setprecision(7) << setw(12) << 1.0;
                    }
                    ostr << endl;
                }
            }
            // TODO provide the user the option to review the results before storing them
            cout << "calFileInfo[" << dsmId << "][" << devId << "].name = ";
            cout << calFileInfo[dsmId][devId].name << endl;

            cout << ostr.str() << endl;
        }
    }
    // DEBUG show totals for Min and Max
    cout << "voltageMin.size() = " << voltageMin.size() << endl;
    cout << "voltageMax.size() = " << voltageMax.size() << endl;
    double allVoltageMin = gsl_stats_float_min(
      &(voltageMin[0]), 1, voltageMin.size());
    double allVoltageMax = gsl_stats_float_max(
      &(voltageMax[0]), 1, voltageMax.size());
    cout << "allVoltageMin = " << allVoltageMin << endl;
    cout << "allVoltageMax = " << allVoltageMax << endl;
}


class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};


class Calibrator
{
public:
    Calibrator();

    int parseRunstring(int argc, char** argv);

    int run() throw();

    static int main(int argc, char** argv);

    static int usage(const char* argv0);

    static void sigAction(int sig, siginfo_t* siginfo, void* vptr);

    static void setupSignals();

private:

    bool unCalibrated;

    bool _grouped;

    list<DSMSensor*> allSensors;

    static const int DEFAULT_PORT = 30000;

    static bool interrupted;

    auto_ptr<n_u::SocketAddress> sockAddr;
};


Calibrator::Calibrator():
   unCalibrated(false),
   _grouped(true)
{
}


int Calibrator::parseRunstring(int argc, char** argv)
{
//  extern char *optarg;       /* set by getopt() */
//  extern int optind;         /* "  "     "      */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "Cs")) != -1) {
        switch (opt_char) {
        case 'C':
            unCalibrated = true;
            break;
        case 's':
            _grouped = false;
            break;
        case '?':
            return usage(argv[0]);
        }
    }
    return 0;
}


int Calibrator::run() throw()
{
    cout << "Calibrator::run()\n";

    try {
        AutoProject project;

        IOChannel* iochan = 0;

#ifdef DEBUG
        list<string> dataFileNames;
        dataFileNames.push_back("/home/data/20091106_161332_ff04.ads");
        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(dataFileNames);
        iochan = fset->connect();
        cout << "DEBUG!  using " << dataFileNames.front() << endl;
#else
        // real time operation
        n_u::Inet4Address addr = n_u::Inet4Address::getByName("localhost");
        sockAddr.reset(new n_u::Inet4SocketAddress(addr,DEFAULT_PORT));
        n_u::Socket* sock = new n_u::Socket(*sockAddr.get());
        iochan = new nidas::core::Socket(sock);
        cout << "Calibrator::run() connected to dsm_server\n";
#endif

        RawSampleInputStream sis(iochan);
        sis.setMaxSampleLength(32768);
        sis.readInputHeader();

        cout << "Calibrator::run() RawSampleStream now owns the iochan ptr.\n";

        const SampleInputHeader& header = sis.getInputHeader();

        cout << "Calibrator::run() setup SampleInputHeader\n";

        string xmlFileName = header.getConfigName();
        xmlFileName = n_u::Process::expandEnvVars(xmlFileName);

        struct stat statbuf;
        if ( ::stat(xmlFileName.c_str(),&statbuf) )
            return 1;

        cout << "Calibrator::run() found xml config file\n";
        auto_ptr<xercesc::DOMDocument> doc(
          DSMEngine::parseXMLConfigFile(xmlFileName));

        Project::getInstance()->fromDOMElement(doc->getDocumentElement());

        DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
        for ( ; di.hasNext(); ) {
            const DSMConfig* dsm = di.next();

            SensorIterator si = dsm->getSensorIterator();
            for ( ; si.hasNext(); ) {
                DSMSensor *sensor = si.next();
                if (!sensor->getClassName().compare("raf.DSMAnalogSensor")) {
                    allSensors.push_back(sensor);
                }
            }
        }
        cout << "Calibrator::run() extracted analog sensors\n";

        CalibrationClient calibration;

        if ( calibration.Setup(allSensors) ) return 1;

        cout << "Calibrator::run() set up CalibrationClient\n";

        SamplePipeline pipeline;

        pipeline.setRealTime(true);
        pipeline.setProcSorterLength(0);

        list<DSMSensor*>::const_iterator si;
        for (si = allSensors.begin(); si != allSensors.end(); ++si) {
            DSMSensor* sensor = *si;
            if (unCalibrated)
                sensor->setCalFile(0);
            sensor->init();

            //  1. inform the SampleInputStream of what SampleTags to expect
            sis.addSampleTag(sensor->getRawSampleTag());
        }

        // 2. connect the pipeline to the SampleInputStream.
        pipeline.connect(&sis);

        // 3. connect the client to the pipeline
        pipeline.getProcessedSampleSource()->addSampleClient(&calibration);

        enum stateEnum state = GATHER;
        try {
            cout << "MAINLOOP initial state: " << stateEnumDesc[state] << endl;
            while ( (state = calibration.SetNextCalVoltage(_grouped)) != DONE ) {
                cout << "MAINLOOP new state: " << stateEnumDesc[state] << endl;

                if (state == DEAD) {
                    cout << "MAINLOOP --- SOMETHING DIED ---" << endl;
                    break;
                }
                while ( !calibration.Gathered() )
                    sis.readSamples();

                if (interrupted) {
                    cout << "MAINLOOP --- INTERRUPTED ---" << endl;
                    break;
                }
            }
            if (state == DONE)
                calibration.DisplayResults();
        }
        catch (n_u::EOFException& e) {
            cout << "CATCH FLUSHING" << endl;
            cout << e.what() << endl;
            sis.flush();
            sis.close();
        }
        catch (n_u::IOException& e) {
            pipeline.getProcessedSampleSource()->removeSampleClient(&calibration);
            pipeline.disconnect(&sis);
            sis.close();
            throw(e);
        }
        pipeline.getProcessedSampleSource()->removeSampleClient(&calibration);
        pipeline.disconnect(&sis);
        sis.close();
    }
    catch (n_u::Exception& e) {
        cout << e.what() << endl;
        return 1;
    }
    return 0;
}


/* static */
int Calibrator::main(int argc, char** argv)
{
    setupSignals();

    Calibrator calibrator;
    int res;
    if ((res = calibrator.parseRunstring(argc,argv))) return res;

    return calibrator.run();
}


/* static */
int Calibrator::usage(const char* argv0)
{
    cout << "\
Usage: " << argv0 << "[-s] [inputURL] ...\n\
    -s seperate: Ungroup the channels while the test voltage is\n\
                 being generated.   Default is grouped.\n\
    inputURL: data input. One of the following:\n\
        sock:host[:port]          (Default port is " << DEFAULT_PORT << ")\n\
        unix:sockpath             unix socket name\n\
        The default URL is sock:localhost\n" << endl;
    return 1;
}


/* static */
void Calibrator::sigAction(int sig, siginfo_t* siginfo, void* vptr)
{
    cout <<
        "received signal " << strsignal(sig) << '(' << sig << ')' <<
        ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
        ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
        ", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
            Calibrator::interrupted = true;
    break;
    }
}


/* static */
void Calibrator::setupSignals()
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
    act.sa_sigaction = Calibrator::sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);
}


/* static */
bool Calibrator::interrupted = false;


int main(int argc, char** argv)
{
    return Calibrator::main(argc,argv);
}
