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
// design taken from 'examples/dialogs/licensewizard'

#ifndef CalibrationWizard_H
#define CalibrationWizard_H

#include <QtGui>
#include <QWizard>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QTreeView>
#include <QGridLayout>
#include <QButtonGroup>
#include <QProgressDialog>

#include <nidas/core/DSMSensor.h>
#include <map>
#include <list>

#include "Calibrator.h"
#include "TreeModel.h"

namespace n_u = nidas::util;

using namespace nidas::core;
using namespace std;

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QRadioButton;
QT_END_NAMESPACE

class SetupPage;
class TestA2DPage;
class AutoCalPage;

/**
 * GUI logic for auto calibration tool.  Creates either AutoCalPage or TestA2DPage
 * based on user input.
 */
class CalibrationWizard : public QWizard
{
    Q_OBJECT

public:
    CalibrationWizard(Calibrator *calibrator, AutoCalClient *acc, QWidget *parent = 0);

    ~CalibrationWizard();

    enum { Page_Setup, Page_AutoCal, Page_TestA2D };

public slots:
    // Qt signal handler.
    void handleSignal();

signals:
    void dialogClosed();

protected:
    void accept();

    void closeEvent(QCloseEvent *event);

private:
    AutoCalClient *acc;

    Calibrator *calibrator;

    // Unix signal handler.
    static void sigAction(int sig, siginfo_t* siginfo, void* vptr)
    { _instance->cleanup(sig, siginfo, vptr); }

    void cleanup(int sig, siginfo_t* siginfo, void* vptr);

    static CalibrationWizard *_instance;

    static int signalFd[2];

    SetupPage*   _SetupPage;
    TestA2DPage* _TestA2DPage;
    AutoCalPage* _AutoCalPage;

    QSocketNotifier *_snSignal;
};


class SetupPage : public QWizardPage
{
    Q_OBJECT

public:
    SetupPage(Calibrator *calibrator, QWidget *parent = 0);

    int nextId() const;

private:
    Calibrator *calibrator;

    QLabel *topLabel;
    QRadioButton *testa2dRadioButton;
    QRadioButton *autocalRadioButton;
};


class AutoCalPage : public QWizardPage
{
    Q_OBJECT

public:
    AutoCalPage(Calibrator *calibrator, AutoCalClient *acc, QWidget *parent = 0);

    void initializePage();
    int nextId() const { return -1; }
    void setVisible(bool visible);

    QProgressDialog* qPD;

public slots:
    void errMessage(const QString& message);
    void setValue(int progress);
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
    void saveButtonClicked();

private:
    int dsmId;
    int devId;

    Calibrator *calibrator;

    AutoCalClient *acc;

    void createTree();
    void createGrid();

    QTreeView *treeView;
    TreeModel *treeModel;
    QGroupBox *gridGroupBox;

    QVBoxLayout *treeLayout;
    QButtonGroup *buttonGroup;
    QHBoxLayout *mainLayout;

    enum { numA2DChannels = 8 };

    QLabel *ChannelTitle;
    QLabel *VarNameTitle;
    QLabel *TimeStampTitle;
    QLabel *TemperatureTitle;
    QLabel *IntcpTitle;
    QLabel *SlopeTitle;

    QLabel *Channel[numA2DChannels];
    QLabel *VarName[numA2DChannels];

    QLabel *OldTimeStamp[numA2DChannels];
    QLabel *OldTemperature[numA2DChannels];
    QLabel *OldIntcp[numA2DChannels];
    QLabel *OldSlope[numA2DChannels];

    QLabel *NewTimeStamp[numA2DChannels];
    QLabel *NewTemperature[numA2DChannels];
    QLabel *NewIntcp[numA2DChannels];
    QLabel *NewSlope[numA2DChannels];
};


class TestA2DPage : public QWizardPage
{
    Q_OBJECT

public:
    TestA2DPage(Calibrator *calibrator, AutoCalClient *acc, QWidget *parent = 0);
    ~TestA2DPage();

    void initializePage();
    int nextId() const { return -1; }

signals:
    void TestVoltage(int channel, int level);

public slots:
    void dispVolts();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void TestVoltage();
    void updateSelection();

private:
    int dsmId;
    int devId;

    Calibrator *calibrator;

    AutoCalClient *acc;

    void createTree();
    void createGrid();

    QTreeView *treeView;
    TreeModel *treeModel;
    QGroupBox *gridGroupBox;

    QVBoxLayout *treeLayout;
    QButtonGroup *buttonGroup;
    QHBoxLayout *mainLayout;

    enum { numA2DChannels = 8 };

    QLabel *ChannelTitle;
    QLabel *VarNameTitle;
    QLabel *RawVoltTitle;
    QLabel *MesVoltTitle;
    QLabel *SetVoltTitle;

    QLabel *Channel[numA2DChannels];
    QLabel *VarName[numA2DChannels];
    QLabel *RawVolt[numA2DChannels];
    QLabel *MesVolt[numA2DChannels];
    QHBoxLayout *SetVolt[numA2DChannels];

    map<int, map< int, QPushButton* > > vLvlBtn;

    QButtonGroup* vLevels[numA2DChannels];
};

#endif
