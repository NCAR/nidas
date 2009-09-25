#include <QtGui>
#include "dsmtablewidget.h"

#include <iostream>
#include <fstream>


/* Constructor - create table, set column headers and width */
DSMTableWidget::DSMTableWidget(QWidget *parent)
       : QTableWidget(parent), _dsmID(0)
{
    curRowCount = 0;

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

void DSMTableWidget::setID(const unsigned int & sensor_id)
{
    QString idStr;
    idStr.append("("); idStr.append(QString::number(_dsmID));
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
