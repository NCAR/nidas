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
