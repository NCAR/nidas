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


#include "CuteLoggingStreamHandler.h"

#include <QBoxLayout>
#include <QPushButton>
#include <string>



CuteLoggingStreamHandler::CuteLoggingStreamHandler(std::ostream &stream, QWidget * parent) :
    CuteLoggingExceptionHandler(parent),
    m_stream(stream)
{
    // setup streams
m_old_buf = stream.rdbuf();
stream.rdbuf(this);
}



void CuteLoggingStreamHandler::display(std::string& where, std::string& what) {
    textwin->setTextColor(Qt::red);
    log(where,what);
    textwin->setTextColor(Qt::black);
    show();
    }



 int CuteLoggingStreamHandler::overflow(int v)
 {
  if (v == '\n')
  {
   textwin->append(m_string.c_str());
   //m_string.erase(m_string.begin(), m_string.end());
   m_string.clear();
  }
  else
   m_string += v;

  return v;
 }



 std::streamsize CuteLoggingStreamHandler::xsputn(const char *p, std::streamsize n)
 {
  //m_string.append(p, p + n); // huh??
  m_string.append(p, n);

  //int pos = 0;
  std::string::size_type pos = 0;
  while (pos != std::string::npos)
  {
   pos = m_string.find('\n');
   if (pos != std::string::npos)
   {
    std::string tmp(m_string.begin(), m_string.begin() + pos);
    textwin->append(tmp.c_str());
    m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
   }
  }

  return n;
 }
