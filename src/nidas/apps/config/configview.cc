/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-11-28 23:09:46 -0700 (Wed, 28 Nov 2007) $

    $LastChangedRevision: 4060 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/xml_dump.cc $
 ********************************************************************
*/

#include <QtGui>
#include <QFileDialog>

#include <nidas/core/XMLParser.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/PortSelectorTest.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/DSMConfig.h>

#include "DSMTableWidget.h"

#include <iostream>
#include <fstream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

void parseAnalog(const DSMConfig * dsm, DSMTableWidget  * DSMTable);
void parseOther(const DSMConfig * dsm, DSMTableWidget  * DSMTable);

int usage(const char* argv0)
{
    cerr << "Usage: " << argv0 << " [xml_file]" << endl;
    cerr << "   where xml_file is an optional argument." << endl;
    return 1;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QMainWindow *MainWindow = new QMainWindow();

    QString _mainWinTitle;
    QString filename;

    if (argc > 2 )
      return usage(argv[0]);
    if (argc == 2) 
      filename.append(argv[1]);
    else
    {
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
        //_winTitle.append(QString::fromStdString(_project));
        _winTitle.append(filename);
        MainWindow->setWindowTitle(_winTitle);
    }

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

        QTabWidget *SiteTabs = new QTabWidget();
        MainWindow->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));

	for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
	    Site * site = si.next();

            QTabWidget *DSMTabs = new QTabWidget();
            //cout << "site:" << site->getName() << endl;

	    for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
		const DSMConfig * dsm = di.next();
 

                tmpStr.append("DSM: "); 
                tmpStr.append(QString::fromStdString(dsm->getLocation())); 
                tmpStr.append(", ["); tmpStr.append(QString::fromStdString(dsm->getName())); 
                tmpStr.append("]");

        //        QTextEdit *DSMText = new QTextEdit();
                DSMTableWidget *DSMTable = new DSMTableWidget();
                //dsmTable.table = new QTableWidget();
                //dsmTable.row=0; dsmTable.col=0;

                QVBoxLayout *DSMLayout = new QVBoxLayout;
                QLabel *DSMLabel = new QLabel(tmpStr);
                DSMLayout->addWidget(DSMLabel);
                //DSMText->append(tmpStr);
                QGroupBox *DSMGroupBox = new QGroupBox("");
                //cout << endl << "-----------------------------------------" << endl;
                //cout	<< "  dsm: " << dsm->getLocation() << ", ["
			//<< dsm->getName() << "]" << endl;

                parseOther(dsm, DSMTable);
                parseAnalog(dsm, DSMTable);

                DSMLayout->addWidget(DSMTable);
                //DSMLayout->addWidget(DSMText);
                //DSMLayout->adjustSize();
                DSMGroupBox->setLayout(DSMLayout);
                DSMTabs->addTab(DSMGroupBox, QString::fromStdString(dsm->getLocation()));
                cout << "DSMTable: " << tmpStr.toStdString() << " size hint: " 
                     << DSMTable->sizeHint().width()
                     << ", " << DSMTable->sizeHint().height() << endl;
                //DSMTabs->addTab(DSMTable, tmpStr);
                tmpStr.clear();
                //DSMTabs->adjustSize();

	    }
            SiteTabs->addTab(DSMTabs, QString::fromStdString(site->getName()));
            //SiteTabs->adjustSize();
	}

        //SiteTabs->show();
        MainWindow->setCentralWidget(SiteTabs);
        //MainWindow->setFixedHeight(MainWindow->sizeHint().height());
        //MainWindow->setFixedWidth(MainWindow->sizeHint().width());
        //MainWindow->adjustSize();
        cout << "MainWindow size : " << MainWindow->size().width() 
             << ", " << MainWindow->size().height() << endl;
        MainWindow->resize(1000, 600);
        MainWindow->show();
        return app.exec();
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

void sensorTitle(DSMSensor * sensor, DSMTableWidget * DSMTable)
{
    //QString tmpStr;
    //cout << endl << "    ";
    //tmpStr.append("     ");
    DSMTable->addRow();
    if (sensor->getCatalogName().length() > 0) {
        //cout << sensor->getCatalogName();
        //tmpStr.append(QString::fromStdString(sensor->getCatalogName()));
        DSMTable->setName(sensor->getCatalogName()+sensor->getSuffix());
    }
    else
    {
        //cout << sensor->getClassName();
        //tmpStr.append(QString::fromStdString(sensor->getClassName()));
        DSMTable->setName(sensor->getClassName()+sensor->getSuffix());
    }


    //cout << sensor->getSuffix() << ", " << sensor->getDeviceName();

    //tmpStr.append(QString::fromStdString(sensor->getSuffix())); tmpStr.append(", "); 
    //tmpStr.append(QString::fromStdString(sensor->getDeviceName()));

    DSMTable->setDevice(sensor->getDeviceName());

    const Parameter * parm = sensor->getParameter("SerialNumber");
    if (parm) {
        //cout << ", SerialNumber " << parm->getStringValue(0);
        //tmpStr.append(", SerialNumber:"); tmpStr.append(QString::fromStdString(parm->getStringValue(0)));
        DSMTable->setSerialNumber(parm->getStringValue(0));
    }

    CalFile *cf = sensor->getCalFile();
    if (cf) {
        string A2D_SN(cf->getFile());
        A2D_SN = A2D_SN.substr(0,A2D_SN.find(".dat"));
        //cout << ", SerialNumber " << A2D_SN;
        //tmpStr.append(", SerialNumber:");tmpStr.append(QString::fromStdString(A2D_SN));
        DSMTable->setSerialNumber(A2D_SN);
    }
    //cout << ", (" << sensor->getDSMId() << ',' << sensor->getShortId() << ')';
    //tmpStr.append(", ("); tmpStr.append(QString::number(sensor->getDSMId())); 
    //tmpStr.append(',');tmpStr.append(QString::number(sensor->getShortId()));
    //tmpStr.append(')');

    QString idStr;
    idStr.append("("); idStr.append(QString::number(sensor->getDSMId()));
    idStr.append(',');idStr.append(QString::number(sensor->getShortId()));
    idStr.append(')');
    DSMTable->setID(idStr);

    //DSMText->append(tmpStr);
    //QLabel *SensorLabel = new QLabel;
    //SensorLabel->setText(tmpStr);
    //DSMLayout->addWidget(SensorLabel);

    //cout << endl;
}

void parseAnalog(const DSMConfig * dsm, DSMTableWidget * DSMTable)
{
    //QString tmpStr;

    for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
        DSMSensor * sensor = si2.next();

        if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
           continue;

        sensorTitle(sensor, DSMTable);

        //cout << "      Samp#  Rate  Variable           gn bi   A/D Cal" << endl;
        //DSMText->append("      Samp#  Rate  Variable           gn bi   A/D Cal");

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
                //cout << "        " << tag->getSampleId() << "    ";
                //tmpStr.append("        "); tmpStr.append(QString::number(tag->getSampleId())); 
                //tmpStr.append("    ");
		//cout.width(4);
                //tmpStr.append(QString::number(tag->getRate())); tmpStr.append("  ");
		//cout << right << tag->getRate() << "  ";
		//cout.width(16);
                //tmpStr.append(QString::fromStdString(var->getName()));
		//cout << left << var->getName();

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
                    //cout.width(5);
                    //cout << right << (int)parm->getNumericValue(0);
                    //tmpStr.append(QString::number(parm->getNumericValue(0)));
                    DSMTable->setGain((int)parm->getNumericValue(0));
                }

                parm = var->getParameter("bipolar");
                if (parm) {
                    //cout.width(3);
                    //cout << right << (int)parm->getNumericValue(0);
                    //tmpStr.append(QString::number(parm->getNumericValue(0)));
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

void parseOther(const DSMConfig * dsm, DSMTableWidget * DSMTable)
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

        //cout << "      Samp#  Rate  Variables" << endl;
        //DSMText->append("      Samp#  Rate  Variables");
        //QString tmpStr;
        QStringList columnHeaders;
        columnHeaders << "Samp#" << "Rate" << "Variables";

        //QTableWidget * sensorTable = new QTableWidget;
        //sensorTable->setColumnCount(3);
        //sensorTable->setHorizontalHeaderLabels(columnHeaders);

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

            //sensorTable->setRowCount(row+1);
            DSMTable->setSampRate(tag->getRate());

            sampleIdStr = QString::number(tag->getSampleId());
            QTableWidgetItem *sampleWidgetItem =  new QTableWidgetItem(sampleIdStr);
            sampleWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());
            //QTableWidgetItem *sampleWidgetItem = sensorTable->item(row, column);
            //sensorTable->setItem(row, column++, sampleWidgetItem);
            //sprintf(temp, "%d", tag->getSampleId());
            //sampleWidgetItem->setText(temp);

            rateStr = QString::number(tag->getRate());
            QTab eWidgetItem *rateWidgetItem = new QTableWidgetItem(rateStr);
            rateWidgetItem->setSizeHint(sampleWidgetItem->sizeHint());
            //sensorTable->setItem(row, column++,rateWidgetItem);
            //sprintf(temp, "%f", tag->getRate());
            //rateWidgetItem->setText(temp);

            //cout	<< "        " << tag->getSampleId() << "     "
	//		<< tag->getRate() << "   ";
            //tmpStr = "        " + QString::number(tag->getSampleId()) + "     " 
            //                    + QString::number(tag->getRate()) + "   ";

            QComboBox * variableComboBox = new QComboBox();
            int iv = 0;
            variableComboBox->addItem(QString("Sample " + QString::number(tag->getSampleId())));
            for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); iv++) {
                const Variable* var = vi.next();
                variableComboBox->addItem(QString::fromStdString(var->getName()));
                if (iv) {
                    //cout << ',' << var->getName();
                    //tmpStr = tmpStr + ',' + QString::fromStdString(var->getName());
                    variableStr = variableStr + ',' + QString::fromStdString(var->getName());
                }
                else {
                    //cout << var->getName();
                    //tmpStr = tmpStr + QString::fromStdString(var->getName());
                    variableStr = variableStr + QString::fromStdString(var->getName());
                }
            }
            //cout << endl;
            //DSMText->append(tmpStr);
            //QTableWidgetItem *varWidItem = new QTableWidgetItem(variableStr);
            //sensorTable->setItem(row, column++, varWidItem);
            //sensorTable->setCellWidget(row, column++, variableComboBox);
            //sensorTable->resizeColumnsToContents();
            //sensorTable->resizeRowsToContents();
            //sensorTable->setColumnWidth(2,200);

            DSMTable->setOtherVariables(variableComboBox);

            //varWidItem->setTextAlignment(Qt::AlignJustify);
            //tmpStr.clear();
            sampleIdStr.clear();
            rateStr.clear();
            variableStr.clear();
            row++; column = 0;
        }

        //DSMLayout->addWidget(sensorTable);
    }
}
