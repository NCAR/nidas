/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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
#include <QString>

#include "AutoCalClient.h"

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

/**
 * @class Calibrator
 * Thread to collect data from dsm_server via AutoCalClient for
 * both auto cal and diagnostic modes.
 */
class Calibrator : public QThread
{
    Q_OBJECT

public:
    Calibrator( AutoCalClient *acc );

    ~Calibrator();

    inline void setTestVoltage() { testVoltage = true; };

    bool setup(QString host) throw();

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
