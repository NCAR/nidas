// design taken from 'examples/dialogs/licensewizard'

#include "CalibrationWizard.h"
#include "Calibrator.h"


CalibrationWizard::CalibrationWizard(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizard(parent), calibrator(calib)
{
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoBackButtonOnLastPage,  true);
    setOption(QWizard::IndependentPages,        true);
    setOption(QWizard::NoCancelButton,          true);

    setPage(Page_Setup,   new SetupPage(calib) );
    setPage(Page_TestA2D, new TestA2DPage(calib, acc) );
    setPage(Page_AutoCal, new AutoCalPage(calib, acc) );

    setStartId(Page_Setup);

#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
// TODO
//  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo.png"));

    setWindowTitle(tr("Auto Calibration Wizard"));
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


void SetupPage::initializePage()
{
    cout << "SetupPage::initializePage" << endl;
    if (calibrator->setup())
        exit(1);
}


int SetupPage::nextId() const
{
    if (autocalRadioButton->isChecked())
        return CalibrationWizard::Page_AutoCal;

    else if (testa2dRadioButton->isChecked())
        return CalibrationWizard::Page_TestA2D;

    else
        return CalibrationWizard::Page_Setup;
}

/* ---------------------------------------------------------------------------------------------- */

AutoCalPage::AutoCalPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizardPage(parent), calibrator(calib), acc(acc)
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
    if (dsmId == devId) {
        QMessageBox::information(0, "notice", "You must select a card to save!");
        return;
    }
    acc->SaveCalFile(dsmId, devId);
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

        OldTemperature[chn]->setText( QString( acc->GetOldTemperature(dsmId, devId, chn).c_str() ) );
        NewTemperature[chn]->setText( QString( acc->GetNewTemperature(dsmId, devId, chn).c_str() ) );

        OldIntcp[chn]->setText( QString( acc->GetOldIntcp(dsmId, devId, chn).c_str() ) );
        NewIntcp[chn]->setText( QString( acc->GetNewIntcp(dsmId, devId, chn).c_str() ) );

        OldSlope[chn]->setText( QString( acc->GetOldSlope(dsmId, devId, chn).c_str() ) );
        NewSlope[chn]->setText( QString( acc->GetNewSlope(dsmId, devId, chn).c_str() ) );
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

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

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
            calibrator,   SLOT(canceled()) );

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
    : QWizardPage(parent), calibrator(calib), acc(acc)
{
    setTitle(tr("Test A2Ds"));
    setSubTitle(tr("Select a card from the tree to list channels."));
    setFinalPage(true);
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


//void TestA2DPage::dispMesVolt(int channel, float val)
void TestA2DPage::dispMesVolt()
{
    QString temp;
    for (int chn = 0; chn < numA2DChannels; chn++) {
        if ( acc->calActv[0][dsmId][devId][chn] == SKIP ) continue;
        temp.sprintf("%7.4f", acc->testData[dsmId][devId][chn]);
        MesVolt[chn]->setText( temp );
    }
}


void TestA2DPage::updateSelection()
{
    ncar_a2d_setup setup = acc->GetA2dSetup(dsmId, devId);

    for (int chn = 0; chn < numA2DChannels; chn++) {
        VarName[chn]->setText( QString( acc->GetVarName(dsmId, devId, chn).c_str() ) );
        MesVolt[chn]->setText("");

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
    layout->addWidget( MesVoltTitle,   0, 2);
    layout->addWidget( SetVoltTitle,   0, 3, 1, 6);

    layout->setColumnMinimumWidth(0, 20);

    list<int> voltageLevels = acc->GetVoltageLevels();

    for (int chn = 0; chn < numA2DChannels; chn++) {
        Channel[chn] = new QLabel( QString("%1:").arg(chn) );
        Channel[chn]->setStyleSheet("QLabel { min-width: 30px ; max-width: 30px }");
        layout->addWidget(Channel[chn], chn+1, 0);

        VarName[chn] = new QLabel;
        VarName[chn]->setStyleSheet("QLabel { min-width: 100px }");
        layout->addWidget(VarName[chn], chn+1, 1);

        MesVolt[chn] = new QLabel;
        MesVolt[chn]->setStyleSheet("QLabel { min-width: 50px ; max-width: 50px }");
        layout->addWidget(MesVolt[chn], chn+1, 2);

        // group buttons by channel
        vLevels[chn] = new QButtonGroup();

        vLvlBtn[-99][chn] = new QPushButton("off");
        vLvlBtn[  0][chn] = new QPushButton("0v");
        vLvlBtn[  1][chn] = new QPushButton("1v");
        vLvlBtn[  5][chn] = new QPushButton("5v");
        vLvlBtn[ 10][chn] = new QPushButton("10v");
        vLvlBtn[-10][chn] = new QPushButton("-10v");

        list<int>::iterator l;
        int column = 3;
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

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    createTree();
    createGrid();

    connect(this, SIGNAL(TestVoltage(int, int)),
            acc,    SLOT(TestVoltage(int, int)));

    connect(acc,  SIGNAL(updateSelection()),
            this,   SLOT(updateSelection()));

    connect(acc,  SIGNAL(dispMesVolt()),
            this,   SLOT(dispMesVolt()));

    mainLayout = new QHBoxLayout;
    mainLayout->addWidget(treeView);
    mainLayout->addWidget(gridGroupBox);

    setLayout(mainLayout);

    calibrator->setTestVoltage();
    acc->setTestVoltage(-1, -1);

    calibrator->start();  // see Calibrator::run
}
