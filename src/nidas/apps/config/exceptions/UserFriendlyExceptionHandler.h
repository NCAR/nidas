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
#ifndef _UserFriendlyExceptionHandler_h
#define _UserFriendlyExceptionHandler_h

#include <string>
#include <nidas/util/Exception.h>


class UserFriendlyExceptionHandler {
public:

 virtual void handle(std::string & where) {
   const char *what = 0;
   try { throw; }
   catch (nidas::util::Exception &e) { what = e.what(); }
   catch (...) { }
   display(where,what);
   }

 void handle(const char *where) {
  std::string swhere(where);
  handle(swhere);
  };


 virtual void display(std::string& where, std::string& what) = 0;

 void display(const char* where, std::string& what) {
  std::string swhere(where);
  display(swhere,what);
  }
 void display(std::string& where, const char* what) {
  std::string swhat(what);
  display(where,swhat);
  }
 void display(const char* where, const char* what) {
  std::string swhere(where);
  std::string swhat(what);
  display(swhere,swhat);
  }


 // override to actually implement logging
 virtual void log(std::string & where, std::string & what) { display(where,what); }
 virtual void show() {}
 virtual void hide() {}
 virtual void setVisible(bool) {}


protected:

 UserFriendlyExceptionHandler() {};
 virtual ~UserFriendlyExceptionHandler() {};

};

#endif
