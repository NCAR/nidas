 /*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision: $

    $LastChangedBy:  $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/config/configwindow.cc $
 ********************************************************************
*/

#include <QtGui>
#include <ctime>
//#include <QFileDialog>
//#include <QMenu>
//#include <QAction>


#include "configwindow.h"
#include "CancelProcessingException.h"

using namespace nidas::core;
using namespace nidas::util;

ConfigWindow::ConfigWindow() : numA2DChannels(8)
{
    SiteTabs = new QTabWidget();

    QAction * openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setStatusTip(tr("Open a new configuration file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(getFile()));

    QAction * saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    saveAct->setStatusTip(tr("Save a configuration file"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));

    QAction * saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(tr("Ctrl+A"));
    saveAsAct->setStatusTip(tr("Save configuration as a new file"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAsFile()));

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addAction(exitAct);

    //ConfigWindow::numA2DChannels = 8;
}

QString ConfigWindow::getFile()
{
    QString filename;
    std::string _dir("/"), _project;
    char * _tmpStr;
    QString _caption;
    QString _winTitle("Configview:  ");

    _tmpStr = getenv("PROJ_DIR");
    if (_tmpStr)
       _dir.append(_tmpStr);
    else
       _caption.append("No $PROJ_DIR. ");

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("PROJECT");
        if (_tmpStr)
        {
            _dir.append("/");
            _dir.append(_tmpStr);
            _project.append(_tmpStr);
        }
        else
            _caption.append("No $PROJECT.");
    }

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("AIRCRAFT");
        if (_tmpStr)
        {
            _dir.append("/");
            _dir.append(_tmpStr);
            _dir.append("/nidas");
        }
        else
            _caption.append(" No $AIRCRAFT.");
    }

    _caption.append(" Choose a file...");

    filename = QFileDialog::getOpenFileName(
                0,
                _caption,
                QString::fromStdString(_dir),
                "Config Files (*.xml)");

    if (filename.isNull() || filename.isEmpty()) {
        cerr << "filename null/empty ; not opening" << endl;
        _winTitle.append("(no file selected)");
        setWindowTitle(_winTitle);
        }
    else {
        doc = new Document();
        doc->setFilename(filename.toStdString());
      try {
        doc->parseFile();
        if (buildProjectWidget(doc)) {
            _winTitle.append(filename);
            setWindowTitle(_winTitle);  
            }
      }
      catch (const nidas::core::XMLException& e) {
        cerr << e.what() << endl;
        QMessageBox::information( 0,
               QString::fromStdString("XML Parsing Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
      }
      catch (const n_u::InvalidParameterException& e) {
        cerr << e.what() << endl;
        QMessageBox::information( 0,
               QString::fromStdString("Invalid Parameter Parsing Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
      }
      catch (const n_u::IOException& e) {
        cerr << e.what() << endl;
        QMessageBox::information( 0,
               QString::fromStdString("I/O Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
      }
      catch (const CancelProcessingException & cpe) {
        // stop processing, show blank window
      }

      }

    resize(1000, 600);
    show();
    return filename;
}



QString ConfigWindow::saveFile()
{
cerr << "saveFile called" << endl;
doc->writeDocument();
return(NULL);
}


QString ConfigWindow::saveAsFile()
{
QString qfilename;
QString _caption;

    qfilename = QFileDialog::getSaveFileName(
                0,
                _caption,
                doc->getDirectory(),
                "Config Files (*.xml)");

    cerr << "saveAs dialog returns " << qfilename.toStdString() << endl;

    if (qfilename.isNull() || qfilename.isEmpty()) {
        cerr << "qfilename null/empty ; not saving" << endl;
        return(NULL);
        }

doc->setFilename(qfilename.toStdString().c_str());
doc->writeDocument();
return(NULL);
}



int ConfigWindow::buildProjectWidget(Document *doc)
{
if (!doc) return(0);

    Project * project = doc->getProject();

        QString tmpStr;

        QTabWidget * SiteTabs = new QTabWidget();
        setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));

        for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
            Site * site = si.next();

            QTabWidget *DSMTabs = new QTabWidget();

            for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
                const DSMConfig * dsm = di.next();


                tmpStr.append("DSM: ");
                tmpStr.append(QString::fromStdString(dsm->getLocation()));
                tmpStr.append(", ["); tmpStr.append(QString::fromStdString(dsm->getName()));
                tmpStr.append("]");

                DSMTableWidget *DSMTable = new DSMTableWidget();

                QVBoxLayout *DSMLayout = new QVBoxLayout;
                QLabel *DSMLabel = new QLabel(tmpStr);
                DSMLayout->addWidget(DSMLabel);
                QGroupBox *DSMGroupBox = new QGroupBox("");

                parseOther(dsm, DSMTable);
                parseAnalog(dsm, DSMTable);

                DSMLayout->addWidget(DSMTable);
                DSMGroupBox->setLayout(DSMLayout);
                DSMTabs->addTab(DSMGroupBox, QString::fromStdString(dsm->getLocation()));
                cout << "DSMTable: " << tmpStr.toStdString() << " size hint: "
                     << DSMTable->sizeHint().width()
                     << ", " << DSMTable->sizeHint().height() << endl;
                tmpStr.clear();

            }
            SiteTabs->addTab(DSMTabs, QString::fromStdString(site->getName()));
        }

        setCentralWidget(SiteTabs);

    return 1;
}



void ConfigWindow::sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable)
{
    DSMTable->addRow();
    if (sensor->getCatalogName().length() > 0) {
        DSMTable->setName(sensor->getCatalogName()+sensor->getSuffix());
    }
    else
    {
        DSMTable->setName(sensor->getClassName()+sensor->getSuffix());
    }

    DSMTable->setDevice(sensor->getDeviceName());

    const Parameter * parm = sensor->getParameter("SerialNumber");
    if (parm) {
        DSMTable->setSerialNumber(parm->getStringValue(0));
    }

    CalFile *cf = sensor->getCalFile();
    if (cf) {
        string A2D_SN(cf->getFile());
        A2D_SN = A2D_SN.substr(0,A2D_SN.find(".dat"));
        DSMTable->setSerialNumber(A2D_SN);
    }

    QString idStr;
    idStr.append("("); idStr.append(QString::number(sensor->getDSMId()));
    idStr.append(',');idStr.append(QString::number(sensor->getShortId()));
    idStr.append(')');
    DSMTable->setID(idStr);
}

void ConfigWindow::parseAnalog(const DSMConfig * dsm, DSMTableWidget * DSMTable)
{
    int gain=0, bipolar=0, channel=0;
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
           continue;

        sensorTitle(sensor, DSMTable);
        CalFile *cf = sensor->getCalFile();
        try {
            cf->open();
            }
        catch(const n_u::IOException& e) {
            cerr << e.what() << endl;
            int button =
                QMessageBox::information( 0, "Error parsing calibration file",
                    QString::fromAscii(e.what())+
                    QString::fromAscii("\nCancel processing or continue without this calibration file?"),
                    "Cancel", "Continue", 0, 1, 1 );
            if (button == 0) {
                CancelProcessingException cpe(e.what());
                throw(cpe);
                }
            if (button == 1) cf=0;
            }

        const Parameter * parm;
        QString varStr;
        int tagNum = 0;
        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag * tag = ti.next();
            if (!tag->isProcessed()) continue;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable * var = vi.next();
                tagNum++;
                if (tagNum > 1) DSMTable->addRow();

                DSMTable->setSampRate(tag->getRate());
                varStr.append("S");
                varStr.append(QString::number(tag->getSampleId()));
                varStr.append(":");
                varStr.append(QString::fromStdString(var->getName()));
                DSMTable->setAnalogVariable(varStr);
                varStr.clear();

                channel = var->getA2dChannel();
                DSMTable->setAnalogChannel(channel);

                parm = var->getParameter("gain");
                if (parm) {
                    gain = (int) parm->getNumericValue(0);
                    DSMTable->setGain(gain);
                }

                parm = var->getParameter("bipolar");
                if (parm) {
                    bipolar = (int) parm->getNumericValue(0);
                    DSMTable->setBiPolar(bipolar);
                }

                parm = var->getParameter("linear");
                if (parm) {
                std::string tmpStr = parm->getStringValue(0);
cerr<<"Found a linear cal: "<< tmpStr <<endl;
                }

                parm = var->getParameter("poly");
                if (parm) {
                std::string tmpStr = parm->getStringValue(0);
cerr<<"Found a poly cal: "<< tmpStr <<endl;
                }

 
                parm = var->getParameter("corIntercept");
                QString tmpStr;
                //cout.width(12); cout.precision(6);
                if (parm) {
                    // A2D cals are in "old school" form rather than cal file
                    //cout << right << parm->getNumericValue(0);
                    tmpStr.append("(");
                    tmpStr.append(QString::number(parm->getNumericValue(0)));
                    parm = var->getParameter("corSlope");
                    //cout.width(10); cout.precision(6);
                    if (parm) {
                        //cout << right << parm->getNumericValue(0);
                        tmpStr.append(", ");
                        tmpStr.append(QString::number(parm->getNumericValue(0)));
                        tmpStr.append(")");
                        DSMTable->setA2DCal(tmpStr);
                        tmpStr.clear();
                    }  // TODO: else alert user to fact that we only have half a cal
                }
                else if (cf) {
                  try {
                        // i.e. cf->reset()
                        cf->close();
                        cf->open();

                        float slope = 1, intercept = 0;

                        // time_t curTime = time(NULL);
                        dsm_time_t tnow = getSystemTime();
                        dsm_time_t calTime = 0;

//cerr<<"Working on calfile:"<<cf->getFile()<< "  channel:"<< channel<< "  tnow:"<<tnow<<endl;
                        while (tnow > calTime && channel >= 0) {
                            int nd = 2 + numA2DChannels  * 2;
                            float d[nd];
                                int n = cf->readData(d,nd);
                                calTime = cf->readTime().toUsecs();
//cerr<<" calTime:"<<calTime<<endl;
                                if (n < 2) { cerr<<"ERR: only found 2 items on the line"<<endl;continue; }
                                int cgain = (int)d[0];
                                int cbipolar = (int)d[1];
//cerr<<"   cgain:"<<cgain<<" gain:"<<gain;
//cerr<<"   cbipolar:"<<cbipolar<<" bipolar:"<<bipolar<<" firstcal:"<<d[2]<<endl;
                                if ((cgain < 0 || gain == cgain) &&
                                    (cbipolar < 0 || bipolar == cbipolar))
                                {
                                    intercept = d[2+channel*2];
                                    slope = d[3+channel*2];
//cerr<<"  *** setting :(" <<intercept<<", " << slope << ")"<<endl;
                                }

                        }
 
                        QString calStr;
                        calStr.append("(" + QString::number(intercept) + ", " +
                             QString::number(slope) + ")");
                        DSMTable->setA2DCal(calStr);

                }
                catch(const n_u::EOFException& e)
                            {
                                cerr << e.what() << endl;
                                    QMessageBox::information( 0,
                                       "No slope before End of config file",
                                       QString::fromStdString(e.what()), "OK" );
                            }
                            catch(const n_u::IOException& e)
                            {
                                cerr << e.what() << endl;
                                    QMessageBox::information( 0, "Error parsing config file",
                                       QString::fromStdString(e.what()),
                                       "OK" );
                            }
                            catch(const n_u::ParseException& e)
                            {
                                cerr << e.what() << endl;
                                    QMessageBox::information( 0, "Error parsing config file",
                                       QString::fromStdString(e.what()),
                                       "OK" );
                            }

              }
            }
        }
    }
}

void ConfigWindow::parseOther(const DSMConfig * dsm, DSMTableWidget * DSMTable)
{   
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor") == 0)
           continue;

        sensorTitle(sensor, DSMTable);

        if (sensor->getCatalogName().compare("IRIG") == 0)
            continue;

        if (sensor->getDeviceName().compare(0, 10, "/dev/arinc") == 0)
            continue;

        QStringList columnHeaders;
        columnHeaders << "Samp#" << "Rate" << "Variables";

        int row=0, column=0;
        QString sampleIdStr;
        QString rateStr;
        int sampleNumber=0;

        for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
            const SampleTag* tag = ti.next();
            if (!tag->isProcessed()) continue;

            sampleNumber++;
            if (sampleNumber>1) DSMTable->addRow();

            DSMTable->setSampRate(tag->getRate());

            sampleIdStr = QString::number(tag->getSampleId());
            QTableWidgetItem *sampleWidgetItem =  new QTableWidgetItem(sampleIdStr);
            sampleWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());

            rateStr = QString::number(tag->getRate());
            QTableWidgetItem *rateWidgetItem = new QTableWidgetItem(rateStr);
            rateWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());

            QComboBox * variableComboBox = new QComboBox(this);
            // It would be nice to have the combo box always return to "Sample N" heading after a user has viewed variables in the box
            // The following commented out line returns an error at runtime that setCurrentIndex(0) is not a valid SLOT.
            //connect(variableComboBox, SIGNAL(currentIndexChanged(int)), variableComboBox, SLOT(setCurrentIndex(0)));
            variableComboBox->addItem(QString("Sample " + QString::number(tag->getSampleId())));
            QString varInfo;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable* var = vi.next();
                varInfo.append(QString::fromStdString(var->getName()));
                VariableConverter* varConv = var->getConverter();
                if (varConv) {
                    varInfo.append(QString::fromStdString(" - " + varConv->toString()));
                }
                variableComboBox->addItem(varInfo);
                varInfo.clear();
            }

            DSMTable->setOtherVariables(variableComboBox);

            sampleIdStr.clear();
            rateStr.clear();
            row++; column = 0;
        }
    }
}


