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
#include <list>

#include "Calibrator.h"
#include "TreeModel.h"

using namespace nidas::core;
using namespace std;

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
QT_END_NAMESPACE

class CalibrationWizard : public QWizard
{
    Q_OBJECT

public:
    CalibrationWizard(Calibrator *calibrator, AutoCalClient *acc, QWidget *parent = 0);

    enum { Page_Setup, Page_AutoCal, Page_TestA2D };

private:
    Calibrator *calibrator;

private slots:
    void showHelp();
};


class SetupPage : public QWizardPage
{
    Q_OBJECT

public:
    SetupPage(Calibrator *calibrator, QWidget *parent = 0);

    void initializePage();
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
    void setValue(int progress);

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
    void printButtonClicked();

private:
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
    TestA2DPage(QWidget *parent = 0);

    int nextId() const { return -1; }
};

#endif
