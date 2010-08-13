// design taken from 'examples/dialogs/licensewizard'

#include "CalibrationWizard.h"
#include "Calibrator.h"


CalibrationWizard::CalibrationWizard(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : calibrator(calib), QWizard(parent)
{
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoBackButtonOnLastPage,  true);
    setOption(QWizard::IndependentPages,        true);
    setOption(QWizard::NoCancelButton,          true);

    setPage(Page_Setup,   new SetupPage(calib) );
    setPage(Page_TestA2D, new TestA2DPage);
    setPage(Page_AutoCal, new AutoCalPage(calib, acc) );

    setStartId(Page_Setup);

#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
// TODO
//  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo.png"));

    setWindowTitle(tr("Auto Calibration Wizard"));
}


SetupPage::SetupPage(Calibrator *calib, QWidget *parent)
    : calibrator(calib), QWizardPage(parent)
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

//  autocalRadioButton->setChecked(true);  // BUG - this causes 'calibrator->setup()' to run prematurely.

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addWidget(testa2dRadioButton);
    layout->addWidget(autocalRadioButton);
    setLayout(layout);
}


int SetupPage::nextId() const
{
    cout << "SetupPage::nextId" << endl;   /// Grrr this gets called before any buttons are pressed!
    if (autocalRadioButton->isChecked()) {
        cout << "calibrator->setup();" << endl;
        if (calibrator->setup())
            exit(1);
        return CalibrationWizard::Page_AutoCal;
    } else {
        return CalibrationWizard::Page_TestA2D;
    }
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
    if (dsmId == devId) return;
    acc->SaveCalFile(dsmId, devId);
}


AutoCalPage::AutoCalPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : calibrator(calib), acc(acc), QWizardPage(parent)
{
    cout << "AutoCalPage::AutoCalPage" << endl;
    setTitle(tr("Auto Calibration"));
    setSubTitle(tr("Select a card from the tree to review the results."));
    setFinalPage(true);
}


void AutoCalPage::createTree()
{
    cout << "AutoCalPage::createTree" << endl;
//  treeGroupBox = new QGroupBox(tr("Analog Card Selection"));
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

//  treeGroupBox->setLayout(treeView);
}


void AutoCalPage::selectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
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
}


void AutoCalPage::createGrid()
{
    gridGroupBox = new QGroupBox(tr("Auto Cal Results"));

    QGridLayout *layout = new QGridLayout();

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
    QApplication::processEvents();

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
    QApplication::processEvents();
};


TestA2DPage::TestA2DPage(QWidget *parent)
    : QWizardPage(parent)
{
    cout << "TestA2DPage::TestA2DPage" << endl;
    setTitle(tr("Test A2Ds"));
    setSubTitle(tr("Select a channel from the tree to set it."));

    QGridLayout *layout = new QGridLayout;
    setLayout(layout);
}
