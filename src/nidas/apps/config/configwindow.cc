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
//#include <QFileDialog>
//#include <QMenu>
//#include <QAction>


#include "configwindow.h"

using namespace nidas::core;

ConfigWindow::ConfigWindow()
{
    SiteTabs = new QTabWidget();

    QAction * openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setStatusTip(tr("Open a new configuration file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(getFile()));

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(exitAct);
}

QString ConfigWindow::getFile()
{
    QString filename;
    std::string _dir("/"), _project;
    char * _tmpStr;
    QString _caption;
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

    QString _winTitle("Configview:  ");
    _winTitle.append(filename);
    setWindowTitle(_winTitle);

    parseFile(filename);
    show();
    return filename;
}

int ConfigWindow::parseFile(QString filename)
{

    QString _mainWinTitle;
    Project * project = 0;
    try {
        cerr << "creating parser" << endl;
        XMLParser * parser = new XMLParser();
    
        // turn on validation
        parser->setDOMValidation(true);
        parser->setDOMValidateIfSchema(true);
        parser->setDOMNamespaces(true);
        parser->setXercesSchema(true);
        parser->setXercesSchemaFullChecking(true);
        parser->setDOMDatatypeNormalization(false);
        parser->setXercesUserAdoptsDOMDocument(true);

        cerr << "parsing: " << filename.toStdString() << endl;
        xercesc::DOMDocument* doc = parser->parse(filename.toStdString());
        cerr << "parsed" << endl;
        cerr << "deleting parser" << endl;
        delete parser;
        project = Project::getInstance();
        cerr << "doing fromDOMElement" << endl;
        project->fromDOMElement(doc->getDocumentElement());
        cerr << "fromDOMElement done" << endl;

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
        resize(1000, 600);

        delete project;
    }
    catch (const nidas::core::XMLException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (const n_u::InvalidParameterException& e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;

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
    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
           continue;

        sensorTitle(sensor, DSMTable);

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

                DSMTable->setAnalogChannel(var->getA2dChannel());

                parm = var->getParameter("gain");
                if (parm) {
                    DSMTable->setGain((int)parm->getNumericValue(0));
                }

                parm = var->getParameter("bipolar");
                if (parm) {
                    DSMTable->setBiPolar((int)parm->getNumericValue(0));
                }
 
/** Need to deal with calibration coeficients
                parm = var->getParameter("corIntercept");
                //cout.width(12); cout.precision(6);
                if (parm) {
                    //cout << right << parm->getNumericValue(0);
                    //tmpStr.append(QString::number(parm->getNumericValue(0)));
                }
                else
                    //cout << "";
 
                parm = var->getParameter("corSlope");
                //cout.width(10); cout.precision(6);
                if (parm) {
                    //cout << right << parm->getNumericValue(0);
                    //tmpStr.append(QString::number(parm->getNumericValue(0)));
                }
                else
                    //cout << "";

                //cout << endl;
                //DSMText->append(tmpStr);
                //tmpStr.clear();
**/
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
        QString variableStr;
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

            QComboBox * variableComboBox = new QComboBox();
            int iv = 0;
            variableComboBox->addItem(QString("Sample " + QString::number(tag->getSampleId())));
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); iv++) {
                const Variable* var = vi.next();
                variableComboBox->addItem(QString::fromStdString(var->getName()));
                if (iv) {
                    variableStr = variableStr + ',' + QString::fromStdString(var->getName());
                }
                else {
                    variableStr = variableStr + QString::fromStdString(var->getName());
                }
            }

            DSMTable->setOtherVariables(variableComboBox);

            sampleIdStr.clear();
            rateStr.clear();
            variableStr.clear();
            row++; column = 0;
        }
    }
}


