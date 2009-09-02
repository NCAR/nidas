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

#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <libgen.h>


#include "configwindow.h"

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
        _winTitle.append(filename);
        //setWindowTitle(_winTitle);
        doc = new Document();
        cerr << "filename is " << filename.toStdString() << endl;
        doc->setFilename(filename.toStdString());
        cerr << "doc filename is " << doc->getFilename() << endl;
        if (parseFile(doc)) setWindowTitle(_winTitle);  
        }

    show();
    return filename;
}



QString ConfigWindow::saveFile()
{
cerr << "saveFile called" << endl;
return(NULL);
}


QString ConfigWindow::saveAsFile()
{
xercesc::LocalFileFormatTarget *target;
QString qfilename;
QString _caption;
std::string _dir(".");

char *buf,*cdir;
buf = new char [ doc->getFilename().length() + 1];
strcpy(buf,doc->getFilename().c_str());
cdir = dirname(buf);

    qfilename = QFileDialog::getSaveFileName(
                0,
                _caption,
                cdir,
                "Config Files (*.xml)");
    delete buf;

    cerr << "saveAs dialog returns " << qfilename.toStdString() << endl;

    if (qfilename.isNull() || qfilename.isEmpty()) {
        cerr << "qfilename null/empty ; not saving" << endl;
        return(NULL);
        }

    try {
        target = new xercesc::LocalFileFormatTarget(qfilename.toStdString().c_str());
        if (!target) {
            cerr << "target is null" << endl;
            return(0);
            }
    } catch (...) {
        cerr << "LocalFileFormatTarget new exception" << endl;
        return(NULL);
    }

writeDOM(target,doc->getDomDocument());
return(NULL);
}


bool ConfigWindow::writeDOM( xercesc::XMLFormatTarget * const target, const xercesc::DOMNode * node )
{
xercesc::DOMImplementation *domimpl;
xercesc::DOMImplementationLS *lsimpl;
xercesc::DOMWriter *myWriter;

        static const XMLCh gLS[] = { xercesc::chLatin_L, xercesc::chLatin_S, xercesc::chNull };
        static const XMLCh gNull[] = { xercesc::chNull };

    try {
        domimpl = XMLImplementation::getImplementation();
        //xercesc::DOMImplementation *domimpl = xercesc::DOMImplementationRegistry::getDOMImplementation(gLS);
    } catch (...) {
        cerr << "getImplementation exception" << endl;
        return(false);
    }
        if (!domimpl) {
            cerr << "xml implementation is null" << endl;
            return(false);
            }

    try {
        lsimpl =
        // (xercesc::DOMImplementationLS*)domimpl;
         (domimpl->hasFeature(gLS,gNull)) ? (xercesc::DOMImplementationLS*)domimpl : 0;
    } catch (...) {
        cerr << "hasFeature/cast exception" << endl;
        return(false);
    }

        if (!lsimpl) {
            cerr << "dom implementation is null" << endl;
            return(false);
            }

    try {
        myWriter = lsimpl->createDOMWriter();
        if (!myWriter) {
            cerr << "writer is null" << endl;
            return(false);
            }
    } catch (...) {
        cerr << "createDOMWriter exception" << endl;
        return(false);
    }

        if (myWriter->canSetFeature(xercesc::XMLUni::fgDOMWRTValidation, true))
            myWriter->setFeature(xercesc::XMLUni::fgDOMWRTValidation, true);
        if (myWriter->canSetFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true))
            myWriter->setFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint, true);

        myWriter->setErrorHandler(&errorHandler);

    try {
        if (!myWriter->writeNode(target,*node)) {
            cerr << "writeNode returns false" << endl;
            }
    } catch (...) {
        cerr << "writeNode exception" << endl;
        return(false);
    }

        target->flush();
        myWriter->release();
        delete target;

        return(true);
}

int ConfigWindow::parseFile(Document *doc)
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

        cerr << "parsing: " << doc->getFilename() << endl;
        doc->setDomDocument(parser->parse(doc->getFilename()));
        cerr << "parsed" << endl;
        cerr << "deleting parser" << endl;
        delete parser;
        project = Project::getInstance();
        cerr << "doing fromDOMElement" << endl;
        project->fromDOMElement(doc->getDomDocument()->getDocumentElement());
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
        QMessageBox::information( 0,
               QString::fromStdString("XML Parsing Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
        cerr << e.what() << endl;
        return 0;
    }
    catch (const n_u::InvalidParameterException& e) {
        QMessageBox::information( 0,
               QString::fromStdString("Invalid Parameter Parsing Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
        cerr << e.what() << endl;
        return 0;
    }
    catch (n_u::IOException& e) {
        QMessageBox::information( 0,
               QString::fromStdString("I/O Error on file: "+doc->getFilename()),
               QString::fromStdString(e.what()), 
               "OK" );
        cerr << e.what() << endl;
        return 0;
    }

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
                else 
                {
                    CalFile *cf = sensor->getCalFile();
                    if (cf) {

                        float slope = 1, intercept = 0;

                        // time_t curTime = time(NULL);
                        dsm_time_t tnow = getSystemTime();
                        dsm_time_t calTime = 0;

//cerr<<"Working on calfile:"<<cf->getFile()<< "  channel:"<< channel<< "  tnow:"<<tnow<<endl;
                        while (tnow > calTime && channel >= 0) {
                            int nd = 2 + numA2DChannels  * 2;
                            float d[nd];
                            try {
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
                            catch(const n_u::EOFException& e)
                            {
                                if (slope == 0) 
                                   QMessageBox::information( 0, "No slope before End of config file: " +
                                       QString::fromStdString(cf->getCurrentFileName().c_str()), 
                                       QString::fromStdString(e.what()), "OK" );
                            }
                            catch(const n_u::IOException& e)
                            {
                                QMessageBox::information( 0, "Error parsing config file: " +
                                       QString::fromStdString(cf->getCurrentFileName().c_str()), 
                                       QString::fromStdString(e.what()), "OK" );
                            }
                            catch(const n_u::ParseException& e)
                            {
                                QMessageBox::information( 0, "Error parsing config file: " +
                                       QString::fromStdString(cf->getCurrentFileName().c_str()), 
                                       QString::fromStdString(e.what()), "OK" );
                            }
                        }
 
                        QString calStr;
                        calStr.append("(" + QString::number(intercept) + ", " +
                             QString::number(slope) + ")");
                        DSMTable->setA2DCal(calStr);
                    }

                    cf->close();
                    cf->open();
                    //cout << "";
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

            QComboBox * variableComboBox = new QComboBox();
            variableComboBox->addItem(QString("Sample " + QString::number(tag->getSampleId())));
            QString varInfo;
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                const Variable* var = vi.next();
cerr << "About to go after variabel: " << var->getName() << endl;
                VariableConverter* varConv = var->getConverter();
                if (varConv) {
cerr << "Have the converter" << endl;
                    const std::list<const Parameter*>& calCoes = varConv->getParameters();
cerr << "Have the list of coefs" << endl;
                    std::list< const Parameter *>::const_iterator it = calCoes.begin();
 cerr << "Variable: " << var->getName() << "  Coefs: ";
                    for (; it!=calCoes.end(); ++it)
                    {
 cerr << (*it)->getNumericValue(0) << "  ";
                    }
 cerr << endl;
                }
                varInfo.append(QString::fromStdString(var->getName()));
                variableComboBox->addItem(varInfo);
            }

            DSMTable->setOtherVariables(variableComboBox);

            sampleIdStr.clear();
            rateStr.clear();
            row++; column = 0;
        }
    }
}


