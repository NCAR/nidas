#include <QtGui>
#include "dsmtablewidget.h"

#include <iostream>
#include <fstream>


/* Constructor - create table, set column headers and width */
DSMTableWidget::DSMTableWidget(QWidget *parent)
       : QTableWidget(parent)
{
    curRowCount = 0;

    setColumnCount(NUMCOLS); 
    QStringList columnHeaders;
    columnHeaders << NAMEHDR << DEVICEHDR << SNHDR << IDHDR
                  << CHANHDR << SRHDR << VARHDR << GNHDR 
                  << BIHDR << ADCALHDR;

    setHorizontalHeaderLabels(columnHeaders);
    //resizeColumnsToContents();
    //setAlternatingRowColors(true);
    setAutoFillBackground(true);

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

void DSMTableWidget::setID(const QString & id)
{
    QTableWidgetItem *tempWidgetItem = new QTableWidgetItem(id);
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
