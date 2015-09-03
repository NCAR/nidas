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
#ifndef _CuteLoggingStreamHandler_h
#define _CuteLoggingStreamHandler_h


// stream bits from http://lists.trolltech.com/qt-interest/2005-06/thread00166-0.html


#include "CuteLoggingExceptionHandler.h"
#include <QTextEdit>
#include <QDialog>
#include <string>
#include <iostream>
#include <streambuf>


class CuteLoggingStreamHandler : public CuteLoggingExceptionHandler,
    public std::basic_streambuf<char>
{

public:

CuteLoggingStreamHandler(std::ostream &stream, QWidget * parent = 0);
~CuteLoggingStreamHandler() {}

virtual void display(std::string& where, std::string& what);


protected:

  virtual int overflow(int);
  virtual std::streamsize xsputn(const char *, std::streamsize);


private:
    std::ostream &m_stream;
    std::streambuf *m_old_buf;
    std::string m_string;

};

#endif
