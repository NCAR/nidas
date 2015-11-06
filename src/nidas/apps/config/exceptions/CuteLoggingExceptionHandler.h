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
#ifndef _CuteLoggingExceptionHandler_h
#define _CuteLoggingExceptionHandler_h


#include "UserFriendlyExceptionHandler.h"
#include <QTextEdit>
#include <QDialog>
#include <string>


class CuteLoggingExceptionHandler : public UserFriendlyExceptionHandler,
    public QDialog
{

public:

CuteLoggingExceptionHandler(QWidget * parent = 0);
~CuteLoggingExceptionHandler() {}

virtual void display(std::string& where, std::string& what);


virtual void log(std::string& where, std::string& what) {
    textwin->append(QString::fromStdString(where+": "+what));
    }

virtual void show();
virtual void hide();
virtual void setVisible(bool checked=true);


protected:

  // Qt4.3+ we can  use QPlainTextEdit which "is optimized for use as a log display"
  // http://www.nabble.com/Log-viewer-td21499499.html
  QTextEdit * textwin;

};

#endif
