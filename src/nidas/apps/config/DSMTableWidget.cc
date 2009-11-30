#include <QtGui>
#include "DSMTableWidget.h"

#include <iostream>
#include <fstream>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMWriter.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XDOM.h>


using namespace std;
using namespace xercesc;
using namespace nidas::core;



/* Constructor - create table, set column headers and width */
DSMTableWidget::DSMTableWidget( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QWidget *parent)
       : QTableWidget(parent), _dsmId(0)
{
    curRowCount = 0;
    dsmConfig = dsm;
    domdoc = doc;
    dsmDomNode = 0;

    setObjectName("DSMTable");
    setColumnCount(NUMCOLS); 
    QStringList columnHeaders;
    columnHeaders << NAMEHDR << DEVICEHDR << SNHDR 
                  << CHANHDR << SRHDR << VARHDR << GNHDR 
                  << BIHDR << ADCALHDR << IDHDR;

    setHorizontalHeaderLabels(columnHeaders);
    //resizeColumnsToContents();
    //setAlternatingRowColors(true);
    setAutoFillBackground(true);
    setColumnWidth(SNCOL, 40);
    setColumnWidth(CHANCOL, 50);
    setColumnWidth(GNCOL, 25);
    setColumnWidth(BICOL, 25);
    setColumnWidth(ADCALCOL, 60);

}

void DSMTableWidget::addRow()
{
    curRowCount++;
    setRowCount(curRowCount);    
    if (curRowCount > 4) setBackgroundRole(QPalette::AlternateBase);
    rowHeaders << "";
    setVerticalHeaderLabels(rowHeaders);
}

void DSMTableWidget::setName(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    //setForegroundRole(QPalette::AlternateBase);
    setItem(curRowCount-1, NAMECOL, tempWidgetItem);
    resizeColumnToContents(NAMECOL);
}

void DSMTableWidget::setDevice(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, DEVICECOL, tempWidgetItem);
    resizeColumnToContents(DEVICECOL);
}

void DSMTableWidget::setSerialNumber(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, SNCOL, tempWidgetItem);
    resizeColumnToContents(SNCOL);
}

void DSMTableWidget::setNidasId(const unsigned int & sensor_id)
{
    QString idStr;
    idStr.append("("); idStr.append(QString::number(_dsmId));
    idStr.append(',');idStr.append(QString::number(sensor_id));
    idStr.append(')');

    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(idStr);
    setItem(curRowCount-1, IDCOL, tempWidgetItem);
    resizeColumnToContents(IDCOL);
}

void DSMTableWidget::setSampRate(const float samprate)
{
    QString tmpStr;

    tmpStr.append(QString::number(samprate));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, SRCOL, tempWidgetItem);
    resizeColumnToContents(SRCOL);
}

void DSMTableWidget::setOtherVariables(QComboBox *variables)
{
    setCellWidget(curRowCount-1, VARCOL, variables);
    setColumnWidth(VARCOL, 100);
}

void DSMTableWidget::setAnalogVariable(const QString & variable)
{
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(variable);
    setItem(curRowCount-1, VARCOL, tempWidgetItem);
    resizeColumnToContents(VARCOL);
}

void DSMTableWidget::setAnalogChannel(const int channel)
{
    QString tmpStr;

    tmpStr.append(QString::number(channel));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, CHANCOL, tempWidgetItem);
    resizeColumnToContents(CHANCOL);
}

void DSMTableWidget::setGain(const int gain)
{
    QString tmpStr;

    tmpStr.append(QString::number(gain));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, GNCOL, tempWidgetItem);
    resizeColumnToContents(GNCOL);
}

void DSMTableWidget::setBiPolar(const int bipolar)
{
    QString tmpStr;

    tmpStr.append(QString::number(bipolar));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, BICOL, tempWidgetItem);
    resizeColumnToContents(BICOL);
}

void DSMTableWidget::setA2DCal(const QString & variable)
{
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(variable);
    setItem(curRowCount-1, ADCALCOL, tempWidgetItem);
    resizeColumnToContents(ADCALCOL);
}


// Return a pointer to the node which defines this DSM
DOMNode * DSMTableWidget::getDSMNode()
{
if (dsmDomNode) return(dsmDomNode);
cerr << "DSMTableWidget::getDSMNode()\n";
if (!domdoc) return(0);

  DOMNodeList * DSMNodes = domdoc->getElementsByTagName((const XMLCh*)XMLStringConverter("dsm"));
  DOMNode * DSMNode = 0;
  cerr << "nodes length = " << DSMNodes->getLength() << "\n";
  for (XMLSize_t i = 0; i < DSMNodes->getLength(); i++) 
  {
     XDOMElement xnode((DOMElement *)DSMNodes->item(i));
     const std::string& sDSMId = xnode.getAttributeValue("id");
     if ((unsigned int)atoi(sDSMId.c_str()) == _dsmId) { 
       cerr<<"getDSMNode - Found DSMNode with id:" << sDSMId << endl;
       DSMNode = DSMNodes->item(i);
     }
  }

  return(dsmDomNode=DSMNode);

}

