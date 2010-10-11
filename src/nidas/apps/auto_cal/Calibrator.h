#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/util/SocketAddress.h>
#include <nidas/dynld/RawSampleInputStream.h>

#include <list>
#include <map>
#include <string>

#include <QtGui>
#include <QThread>

#include "AutoCalClient.h"

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

class Calibrator : public QThread
{
    Q_OBJECT

public:
    Calibrator( AutoCalClient *acc );

    ~Calibrator();

    inline void setTestVoltage() { testVoltage = true; };

    bool setup() throw();

    static bool isSettingUp;

    void run();

signals:
    void setValue(int progress);

public slots:
    void cancel();

private:
    bool testVoltage;

    bool canceled;

    AutoCalClient* _acc;

    RawSampleInputStream* _sis;

    SamplePipeline* _pipeline;

    map<dsm_sample_id_t, string>dsmLocations;
};

#endif
