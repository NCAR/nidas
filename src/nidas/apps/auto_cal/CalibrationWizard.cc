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

#include "CalibrationWizard.h"
#include "Calibrator.h"
#include "PolyEval.h"

#include <unistd.h>

#include <QCursor>


CalibrationWizard::CalibrationWizard(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizard(parent, Qt::Window), calibrator(calib)
{
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoBackButtonOnLastPage,  true);
    setOption(QWizard::IndependentPages,        true);
    setOption(QWizard::NoCancelButton,          true);

    _SetupPage   = new SetupPage(calib);
    _TestA2DPage = new TestA2DPage(calib, acc);
    _AutoCalPage = new AutoCalPage(calib, acc);

    setPage(Page_Setup,   _SetupPage);
    setPage(Page_TestA2D, _TestA2DPage);
    setPage(Page_AutoCal, _AutoCalPage);

    setStartId(Page_Setup);

#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
// TODO
//  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo.png"));

    setWindowTitle(tr("Auto Calibration Wizard"));

    // setup UNIX signal handler
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGINT);
    sigaddset(&sigset,SIGTERM);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = CalibrationWizard::sigAction;
    sigaction(SIGHUP ,&act,(struct sigaction *)0);
    sigaction(SIGINT ,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);

    // setup sockets to receive UNIX signals
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd))
        qFatal("Couldn't create socketpair");

    _snSignal = new QSocketNotifier(signalFd[1], QSocketNotifier::Read, this);
    connect(_snSignal, SIGNAL(activated(int)), this, SLOT(handleSignal()));
}

CalibrationWizard::~CalibrationWizard()
{
    delete _snSignal;
    delete _AutoCalPage;
    delete _TestA2DPage;
    delete _SetupPage;
}


/* static */
void CalibrationWizard::sigAction(int sig, siginfo_t* siginfo, void* vptr)
{
    cout <<
        "received signal " << strsignal(sig) << '(' << sig << ')' <<
        ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
        ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
        ", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;

    char a = 1;

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        ::write(signalFd[0], &a, sizeof(a));
        break;
    }
}


void CalibrationWizard::handleSignal()
{
    char tmp;
    ::read(signalFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    emit close();
}


/* static */
int CalibrationWizard::signalFd[2]  = {0, 0};


void CalibrationWizard::accept()
{
    calibrator->cancel();
    calibrator->wait();
    QWizard::accept();
}


void CalibrationWizard::closeEvent(QCloseEvent *event)
{
    cout << __PRETTY_FUNCTION__ << endl;
    if (!calibrator) return;

    calibrator->cancel();
    calibrator->wait();
    exit(0);
}


/* ---------------------------------------------------------------------------------------------- */

SetupPage::SetupPage(Calibrator *calib, QWidget *parent)
    : QWizardPage(parent), calibrator(calib)
{
    setTitle(tr("Setup"));

    // TODO
//  setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark.png"));

    topLabel = new QLabel(tr("This will search the NIDAS server and all of its "
                             "connected DSMs for NCAR based A2D cards.\n\nAll "
                             "cards can only operate as configured.\n\nYou can "
                             "either test a card's channels by manually setting "
                             "them, or you can automatically calibrate all of "
                             "the cards:\n"));
    topLabel->setWordWrap(true);

    testa2dRadioButton = new QRadioButton(tr("&Test A2D channels"));
    autocalRadioButton = new QRadioButton(tr("&Auto Calibrate"));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addWidget(testa2dRadioButton);
    layout->addWidget(autocalRadioButton);
    setLayout(layout);
}


int SetupPage::nextId() const
{
    if (autocalRadioButton->isChecked()) {
        return CalibrationWizard::Page_AutoCal;
    }
    else if (testa2dRadioButton->isChecked()) {
        return CalibrationWizard::Page_TestA2D;
    }
    return CalibrationWizard::Page_Setup;
}

/* ---------------------------------------------------------------------------------------------- */

AutoCalPage::AutoCalPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizardPage(parent), dsmId(-1), devId(-1), calibrator(calib), acc(acc)
{
    setTitle(tr("Auto Calibration"));
    setSubTitle(tr("Select a card from the tree to review the results."));
    setFinalPage(true);
}


void AutoCalPage::setVisible(bool visible)
{
    QWizardPage::setVisible(visible);

    if (visible) {
        wizard()->setButtonText(QWizard::CustomButton1, tr("&Save"));
        wizard()->setOption(QWizard::HaveCustomButton1, true);
        connect(wizard(), SIGNAL(customButtonClicked(int)),
                this, SLOT(saveButtonClicked()));
    } else {
        wizard()->setOption(QWizard::HaveCustomButton1, false);
        disconnect(wizard(), SIGNAL(customButtonClicked(int)),
                   this, SLOT(saveButtonClicked()));
    }
}


void AutoCalPage::saveButtonClicked()
{
/* original behavior was to save the card selected on the GUI.
    if (dsmId == devId) {
        QMessageBox::information(0, "notice", "You must select a card to save!");
        return;
    }

    acc->SaveCalFile(dsmId, devId);
*/

    // New default behavior is to save all cards in one fell swoop.
    acc->SaveAllCalFiles();
}


void AutoCalPage::createTree()
{
    cout << "AutoCalPage::createTree" << endl;
    treeView = new QTreeView();

    // extract the tree from AutoCalClient.
    cout << acc->GetTreeModel();
    treeModel = new TreeModel( QString(acc->GetTreeModel().c_str()) );

    // Initialize the QTreeView
    treeView->setModel(treeModel);
    treeView->expandAll();
    treeView->setMinimumWidth(300);
    treeView->resizeColumnToContents(0);
    treeView->resizeColumnToContents(1);
    treeView->resizeColumnToContents(2);

    // The dsmId(s) and devId(s) are hidden in the 3rd column.
//  treeView->hideColumn(2);

    connect(treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                                    this, SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));

    treeView->setCurrentIndex(treeView->rootIndex().child(0,0));
}


void AutoCalPage::selectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
    if (selected.indexes().count() == 0)
        return;

    QModelIndex index = selected.indexes().first();
    QModelIndex parent = index.parent();

    if (parent == QModelIndex()) {
        dsmId = devId;
        return;
    }
    QModelIndex devIdx = index.sibling(index.row(), 2);
    devId = treeModel->data(devIdx, Qt::DisplayRole).toInt();

    QModelIndex dsmIdx = parent.sibling(parent.row(), 2);
    dsmId = treeModel->data(dsmIdx, Qt::DisplayRole).toInt();

    for (int chn = 0; chn < numA2DChannels; chn++) {
        VarName[chn]->setText( QString( acc->GetVarName(dsmId, devId, chn).c_str() ) );

        OldTimeStamp[chn]->setText( QString( acc->GetOldTimeStamp(dsmId, devId, chn).c_str() ) );
        NewTimeStamp[chn]->setText( QString( acc->GetNewTimeStamp(dsmId, devId, chn).c_str() ) );

        OldTemperature[chn]->setText( QString::number( acc->GetOldTemperature(dsmId, devId, chn) ) );
        NewTemperature[chn]->setText( QString::number( acc->GetNewTemperature(dsmId, devId, chn) ) );

        OldIntcp[chn]->setText( QString::number( acc->GetOldIntcp(dsmId, devId, chn) ) );
        NewIntcp[chn]->setText( QString::number( acc->GetNewIntcp(dsmId, devId, chn) ) );

        OldSlope[chn]->setText( QString::number( acc->GetOldSlope(dsmId, devId, chn) ) );
        NewSlope[chn]->setText( QString::number( acc->GetNewSlope(dsmId, devId, chn) ) );
    }
}


void AutoCalPage::createGrid()
{
    gridGroupBox = new QGroupBox(tr("Auto Cal Results"));

    QGridLayout *layout = new QGridLayout;

    ChannelTitle     = new QLabel( QString( "CHN" ) );
    VarNameTitle     = new QLabel( QString( "VARNAME" ) );
    TimeStampTitle   = new QLabel( QString( "TIME" ) );
    IntcpTitle       = new QLabel( QString( "INTCP" ) );
    SlopeTitle       = new QLabel( QString( "SLOPE" ) );

    TemperatureTitle = new QLabel( QString( "TEMP" ) );

    QFont font;
    #if defined(Q_WS_X11)
    font.setFamily("Monospace");
    #else
    font.setFamily("Courier New");
    #endif
    font.setPointSize(9);
    setFont(font);

    layout->addWidget( ChannelTitle,   0, 0);
    layout->addWidget( VarNameTitle,   0, 1);
    layout->addWidget( TimeStampTitle, 0, 2);
    layout->addWidget( IntcpTitle,     0, 3);
    layout->addWidget( SlopeTitle,     0, 4);

    layout->setColumnMinimumWidth( 2, 174);

//  layout->addWidget( TemperatureTitle, 0, 2);

    for (int chn = 0; chn < numA2DChannels; chn++) {
        Channel[chn] = new QLabel( QString("%1:").arg(chn) );
        layout->addWidget(Channel[chn], chn*3+2, 0, 2, 1);

        VarName[chn] = new QLabel;
        layout->addWidget(VarName[chn], chn*3+2, 1, 2, 1);

//      layout->setRowMinimumHeight(chn*2, 30);

        OldTimeStamp[chn]   = new QLabel;
        OldTemperature[chn] = new QLabel;
        OldIntcp[chn]       = new QLabel;
        OldSlope[chn]       = new QLabel;

        NewTimeStamp[chn]   = new QLabel;
        NewTemperature[chn] = new QLabel;
        NewIntcp[chn]       = new QLabel;
        NewSlope[chn]       = new QLabel;

        // add a "blank line" between channels
        layout->addWidget(new QLabel, (chn*3)+1, 0, 1, 5);

        layout->addWidget(OldTimeStamp[chn],   (chn*3)+2, 2);
        layout->addWidget(OldIntcp[chn],       (chn*3)+2, 3);
        layout->addWidget(OldSlope[chn],       (chn*3)+2, 4);

        layout->addWidget(NewTimeStamp[chn],   (chn*3)+3, 2);
        layout->addWidget(NewIntcp[chn],       (chn*3)+3, 3);
        layout->addWidget(NewSlope[chn],       (chn*3)+3, 4);
    }
//  layout->addWidget(OldTemperature[chn], (chn*3)+2, 2);
//  layout->addWidget(NewTemperature[chn], (chn*3)+3, 2);

//  gridGroupBox->setMinimumSize(450, 600); // width, height
    gridGroupBox->setLayout(layout);
}


void AutoCalPage::initializePage()
{
    cout << "AutoCalPage::initializePage" << endl;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (calibrator->setup("acserver")) return;
    QApplication::restoreOverrideCursor();

    createTree();
    createGrid();

    mainLayout = new QHBoxLayout;
    mainLayout->addWidget(treeView);
    mainLayout->addWidget(gridGroupBox);

    setLayout(mainLayout);

    qPD = new QProgressDialog(this);
    qPD->setRange(0, acc->maxProgress() );
    qPD->setWindowTitle(tr("Auto Calibrating..."));
    qPD->setWindowModality(Qt::WindowModal);

    // This connection spans across threads so it is a
    // Qt::QueuedConnection by default.
    // (http://doc.qt.nokia.com/4.6/threads-mandelbrot.html)
    connect(acc,  SIGNAL(errMessage(const QString&)),
            this,   SLOT(errMessage(const QString&)));

    connect(calibrator, SIGNAL(setValue(int)),
            qPD,          SLOT(setValue(int)) );

    connect(qPD,        SIGNAL(canceled()),
            calibrator,   SLOT(cancel()) );

    calibrator->start();  // see Calibrator::run
}


void AutoCalPage::errMessage(const QString& message)
{
    QMessageBox::warning(this, "error", message);
}


void AutoCalPage::setValue(int progress)
{
    qPD->setValue(progress);
};

/* ---------------------------------------------------------------------------------------------- */

TestA2DPage::TestA2DPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizardPage(parent), dsmId(-1), devId(-1), calibrator(calib), acc(acc)
{
    setTitle(tr("Test A2Ds"));
    setSubTitle(tr("Select a card from the tree to list channels."));
    setFinalPage(true);
    setMinimumWidth(880);
}


TestA2DPage::~TestA2DPage()
{
    cout << "TestA2DPage::~TestA2DPage()" << endl;
    list<int> voltageLevels = acc->GetVoltageLevels();
    list<int>::iterator l;

    for (int chn = 0; chn < numA2DChannels; chn++)
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
            delete vLvlBtn[*l][chn];
}


void TestA2DPage::createTree()
{
    cout << "TestA2DPage::createTree" << endl;
    treeView = new QTreeView();

    // extract the tree from AutoCalClient.
    cout << acc->GetTreeModel();
    treeModel = new TreeModel( QString(acc->GetTreeModel().c_str()) );

    // Initialize the QTreeView
    treeView->setModel(treeModel);
    treeView->expandAll();
    treeView->setMinimumWidth(300);
    treeView->resizeColumnToContents(0);
    treeView->resizeColumnToContents(1);
    treeView->resizeColumnToContents(2);

    // The dsmId(s) and devId(s) are hidden in the 3rd column.
//  treeView->hideColumn(2);

    connect(treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                                    this, SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));

    treeView->setCurrentIndex(treeView->rootIndex().child(0,0));
}


void TestA2DPage::dispVolts()
{
    if (devId == -1) return;
    if (dsmId == -1) return;
    if (dsmId == devId) return;

    QString raw, mes;
    for (int chn = 0; chn < numA2DChannels; chn++) {
        if ( acc->calActv[0][dsmId][devId][chn] == SKIP ) continue;

        // obtain current set of calibration coefficients for this channel
        std::vector<double> _cals;
        _cals.push_back( acc->GetOldIntcp(dsmId, devId, chn) );
        _cals.push_back( acc->GetOldSlope(dsmId, devId, chn) );

        // apply the coefficients to the raw measured values
        float voltage = acc->testData[dsmId][devId][chn];
        float applied = numeric::PolyEval(_cals, voltage);
        raw.sprintf("%7.4f", voltage);
        mes.sprintf("%7.4f", applied);
        RawVolt[chn]->setText( raw );
        MesVolt[chn]->setText( mes );
    }
}


void TestA2DPage::updateSelection()
{
    ncar_a2d_setup setup = acc->GetA2dSetup(dsmId, devId);

    for (int chn = 0; chn < numA2DChannels; chn++) {
        VarName[chn]->setText( QString( acc->GetVarName(dsmId, devId, chn).c_str() ) );
        RawVolt[chn]->setText("");
        MesVolt[chn]->setText("");

        VarName[chn]->setHidden(false);
        RawVolt[chn]->setHidden(false);
        MesVolt[chn]->setHidden(false);

        list<int> voltageLevels;
        list<int>::iterator l;

        // hide and uncheck all voltage selection buttons for this channel
        voltageLevels = acc->GetVoltageLevels();
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++) {
            vLvlBtn[*l][chn]->setHidden(true);
            vLvlBtn[*l][chn]->setDown(false);
//          cout << "TestA2DPage::updateSelection vLvlBtn[" << *l << "][" << chn << "]->setDown(false);" << endl;
        }
        voltageLevels.clear();

        // show available voltage selection buttons for this channel
        voltageLevels = acc->GetVoltageLevels(dsmId, devId, chn);
        if (!voltageLevels.empty()) {
            for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
                vLvlBtn[*l][chn]->setHidden(false);

            RawVolt[chn]->setText("---");
            MesVolt[chn]->setText("---");
            vLvlBtn[-99][chn]->setHidden(false);

            // check all active channels to use the same calibration voltage
            if (setup.calset[chn])
                vLvlBtn[setup.vcal][chn]->setDown(true);
            else
                vLvlBtn[-99][chn]->setDown(true);
        }
    }
}


void TestA2DPage::selectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
    cout << "TestA2DPage::selectionChanged" << endl;
    if (selected.indexes().count() > 0) {

        QModelIndex index = selected.indexes().first();
        QModelIndex parent = index.parent();

        QModelIndex devIdx = index.sibling(index.row(), 2);
        devId = treeModel->data(devIdx, Qt::DisplayRole).toInt();

        QModelIndex dsmIdx = parent.sibling(parent.row(), 2);
        dsmId = treeModel->data(dsmIdx, Qt::DisplayRole).toInt();

        if (parent == QModelIndex()) {

            // DSM selected not card... hide the displayed table contents.
            list<int> voltageLevels;
            list<int>::iterator l;
            voltageLevels = acc->GetVoltageLevels();
            for (int chn = 0; chn < numA2DChannels; chn++) {
                VarName[chn]->setHidden(true);
                RawVolt[chn]->setHidden(true);
                MesVolt[chn]->setHidden(true);
                for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
                    vLvlBtn[*l][chn]->setHidden(true);
            }
            dsmId = devId;
            return;
        }
        acc->setTestVoltage(dsmId, devId);
        updateSelection();
    }
}


void TestA2DPage::createGrid()
{
    gridGroupBox = new QGroupBox(tr("Set internal voltages here"));

    QGridLayout *layout = new QGridLayout;

    ChannelTitle     = new QLabel( QString( "CHN" ) );
    VarNameTitle     = new QLabel( QString( "VARNAME" ) );
    RawVoltTitle     = new QLabel( QString( "RAWVOLT" ) );
    MesVoltTitle     = new QLabel( QString( "MESVOLT" ) );
    SetVoltTitle     = new QLabel( QString( "SETVOLT" ) );

    QFont font;
    #if defined(Q_WS_X11)
    font.setFamily("Monospace");
    #else
    font.setFamily("Courier New");
    #endif
    font.setPointSize(9);
    setFont(font);

    layout->addWidget( ChannelTitle,   0, 0);
    layout->addWidget( VarNameTitle,   0, 1);
    layout->addWidget( RawVoltTitle,   0, 2);
    layout->addWidget( MesVoltTitle,   0, 3);
    layout->addWidget( SetVoltTitle,   0, 4, 1, 6);

    layout->setColumnMinimumWidth(0, 20);

    list<int> voltageLevels = acc->GetVoltageLevels();

    for (int chn = 0; chn < numA2DChannels; chn++) {
        Channel[chn] = new QLabel( QString("%1:").arg(chn) );
        Channel[chn]->setStyleSheet("QLabel { min-width: 30px ; max-width: 30px }");
        layout->addWidget(Channel[chn], chn+1, 0);

        VarName[chn] = new QLabel;
        VarName[chn]->setStyleSheet("QLabel { min-width: 100px }");
        layout->addWidget(VarName[chn], chn+1, 1);

        RawVolt[chn] = new QLabel;
        RawVolt[chn]->setStyleSheet("QLabel { min-width: 50px ; max-width: 50px }");
        layout->addWidget(RawVolt[chn], chn+1, 2);

        MesVolt[chn] = new QLabel;
        MesVolt[chn]->setStyleSheet("QLabel { min-width: 50px ; max-width: 50px }");
        layout->addWidget(MesVolt[chn], chn+1, 3);

        // group buttons by channel
        vLevels[chn] = new QButtonGroup();

        vLvlBtn[-99][chn] = new QPushButton("off");
        vLvlBtn[  0][chn] = new QPushButton("0v");
        vLvlBtn[  1][chn] = new QPushButton("1v");
        vLvlBtn[  5][chn] = new QPushButton("5v");
        vLvlBtn[ 10][chn] = new QPushButton("10v");
        vLvlBtn[-10][chn] = new QPushButton("-10v");

        list<int>::iterator l;
        int column = 4;
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++) {

            layout->addWidget(vLvlBtn[*l][chn], chn+1, column++);

            vLvlBtn[*l][chn]->setStyleSheet("QPushButton { min-width: 30px ; max-width: 30px }");
            vLvlBtn[*l][chn]->setHidden(true);
            vLevels[chn]->addButton(vLvlBtn[*l][chn]);

            connect(vLvlBtn[*l][chn],  SIGNAL(clicked()),
                    this,              SLOT(TestVoltage()));
        }
    }
//  gridGroupBox->setMinimumSize(450, 600); // width, height
    gridGroupBox->setStyleSheet(" QGroupBox { min-width: 450px }");
    gridGroupBox->setLayout(layout);
}


void TestA2DPage::TestVoltage()
{
    list<int> voltageLevels = acc->GetVoltageLevels();
    list<int>::iterator l;

    for (int chn = 0; chn < numA2DChannels; chn++)
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)

            if((QPushButton *) sender() == vLvlBtn[*l][chn]) {
                acc->setTestVoltage(dsmId, devId);
                emit TestVoltage(chn, *l);
                return;
            }
}


void TestA2DPage::initializePage()
{
    cout << "TestA2DPage::initializePage" << endl;

    calibrator->setTestVoltage();
    acc->setTestVoltage(-1, -1);

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (calibrator->setup("acserver")) return;
    QApplication::restoreOverrideCursor();

    createTree();
    createGrid();

    connect(this, SIGNAL(TestVoltage(int, int)),
            acc,    SLOT(TestVoltage(int, int)));

    connect(acc,  SIGNAL(updateSelection()),
            this,   SLOT(updateSelection()));

    connect(acc,  SIGNAL(dispVolts()),
            this,   SLOT(dispVolts()));

    connect(calibrator, SIGNAL(setValue(int)),
            this,         SLOT(paint()) );

    mainLayout = new QHBoxLayout;
    mainLayout->addWidget(treeView);
    mainLayout->addWidget(gridGroupBox);

    setLayout(mainLayout);

    calibrator->start();  // see Calibrator::run
}
