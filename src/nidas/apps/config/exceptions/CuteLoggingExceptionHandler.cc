/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/


#include "CuteLoggingExceptionHandler.h"

#include <QBoxLayout>
#include <QPushButton>


void CuteLoggingExceptionHandler::show() {
    this->QDialog::show();
    this->QDialog::raise();
    this->QDialog::activateWindow();
    }

void CuteLoggingExceptionHandler::hide() { this->QDialog::hide(); }

void CuteLoggingExceptionHandler::setVisible(bool checked) { this->QDialog::setVisible(checked); }

void CuteLoggingExceptionHandler::display(std::string& where, std::string& what) {
    log(where,what);
    show();
    }



CuteLoggingExceptionHandler::CuteLoggingExceptionHandler(QWidget * parent) :
    QDialog(parent)
{
this->QDialog::hide();
this->QDialog::resize(600,300);
this->QDialog::setWindowTitle("Errors");

QBoxLayout *mainLayout = new QVBoxLayout;
QBoxLayout *buttonLayout = new QHBoxLayout;

textwin = new QTextEdit;
textwin->setTextColor(Qt::black);
textwin->setReadOnly(true);
QSizePolicy sp(QSizePolicy::Expanding,QSizePolicy::Expanding);
textwin->setSizePolicy(sp);
textwin->setLineWrapMode(QTextEdit::NoWrap);

QPushButton *clearButton = new QPushButton("Clear");
clearButton->setDefault(false);

QPushButton *closeButton = new QPushButton("Close");
closeButton->setDefault(true);

this->QDialog::connect(clearButton,SIGNAL(clicked()),textwin,SLOT(clear()));
this->QDialog::connect(closeButton,SIGNAL(clicked()),this,SLOT(hide()));

buttonLayout->addWidget(clearButton);
buttonLayout->addStretch(1);
buttonLayout->addWidget(closeButton);

mainLayout->addWidget(textwin);
mainLayout->addLayout(buttonLayout);

this->QDialog::setLayout(mainLayout);
}
