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


DSMTableWidget::_ColumnHeader DSMTableWidget::_theHeaders[] = {
 {NAMEIDX,NAMEHDR},
 {DEVICEIDX,DEVICEHDR},
 {SNIDX,SNHDR},
 {CHANIDX,CHANHDR},
 {SRIDX,SRHDR},
 {VARIDX,VARHDR},
 {GNIDX,GNHDR},
 {BIIDX,BIHDR},
 {ADCALIDX,ADCALHDR},
 {IDIDX,IDHDR},
 };


/* Constructor - create table, set column headers and width */
DSMTableWidget::DSMTableWidget( nidas::core::DSMConfig * dsm,
    xercesc::DOMDocument *doc, QWidget *parent)
       : QTableWidget(parent),
      columns(_theHeaders, _theHeaders + sizeof(_theHeaders) / sizeof(_ColumnHeader)),
       _dsmId(0)
{
    curRowCount = 0;
    dsmConfig = dsm;
    domdoc = doc;
    dsmDomNode = 0;

    setObjectName("DSMTable");
    setColumnCount(columns.size()); 
    QStringList columnHeaders;
    for (vector<_ColumnHeader>::iterator it = columns.begin(); it < columns.end(); it++) {
        columnHeaders << (*it).name;
        }

    setHorizontalHeaderLabels(columnHeaders);
    //resizeColumnsToContents();
    //setAlternatingRowColors(true);
    setAutoFillBackground(true);
    setColumnWidth(SNIDX, 40);
    setColumnWidth(CHANIDX, 50);
    setColumnWidth(GNIDX, 25);
    setColumnWidth(BIIDX, 25);
    setColumnWidth(ADCALIDX, 60);
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
    setItem(curRowCount-1, columns[NAMEIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[NAMEIDX].column);
}

void DSMTableWidget::setDevice(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[DEVICEIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[DEVICEIDX].column);
}

void DSMTableWidget::setSerialNumber(const std::string & name)
{
    QString tmpStr;

    tmpStr.append(QString::fromStdString(name));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[SNIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[SNIDX].column);
}

void DSMTableWidget::setNidasId(const unsigned int & sensor_id)
{
    QString idStr;
    idStr.append("("); idStr.append(QString::number(_dsmId));
    idStr.append(',');idStr.append(QString::number(sensor_id));
    idStr.append(')');

    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(idStr);
    setItem(curRowCount-1, columns[IDIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[IDIDX].column);
}

void DSMTableWidget::setSampRate(const float samprate)
{
    QString tmpStr;

    tmpStr.append(QString::number(samprate));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[SRIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[SRIDX].column);
}

void DSMTableWidget::setOtherVariables(QComboBox *variables)
{
    setCellWidget(curRowCount-1, columns[VARIDX].column, variables);
    setColumnWidth(columns[VARIDX].column, 100);
}

void DSMTableWidget::setAnalogVariable(const QString & variable)
{
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(variable);
    setItem(curRowCount-1, columns[VARIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[VARIDX].column);
}

void DSMTableWidget::setAnalogChannel(const int channel)
{
    QString tmpStr;

    tmpStr.append(QString::number(channel));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[CHANIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[CHANIDX].column);
}

void DSMTableWidget::setGain(const int gain)
{
    QString tmpStr;

    tmpStr.append(QString::number(gain));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[GNIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[GNIDX].column);
}

void DSMTableWidget::setBiPolar(const int bipolar)
{
    QString tmpStr;

    tmpStr.append(QString::number(bipolar));
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(tmpStr);
    setItem(curRowCount-1, columns[BIIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[BIIDX].column);
}

void DSMTableWidget::setA2DCal(const QString & variable)
{
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(variable);
    setItem(curRowCount-1, columns[ADCALIDX].column, tempWidgetItem);
    resizeColumnToContents(columns[ADCALIDX].column);
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

