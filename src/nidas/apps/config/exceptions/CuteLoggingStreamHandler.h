#ifndef _CuteLoggingStreamHandler_h
#define _CuteLoggingStreamHandler_h


// stream bits from http://lists.trolltech.com/qt-interest/2005-06/thread00166-0.html


#include "UserFriendlyExceptionHandler.h"
#include <QTextEdit>
#include <QDialog>
#include <string>
#include <iostream>
#include <streambuf>


class CuteLoggingStreamHandler : public UserFriendlyExceptionHandler,
    public std::basic_streambuf<char>
{

public:

CuteLoggingStreamHandler(std::ostream &stream, QWidget * parent = 0);
~CuteLoggingStreamHandler() {}

void display(std::string& where, std::string& what) {
    textwin->setTextColor(Qt::red);
    log(where,what);
    textwin->setTextColor(Qt::black);
    window->show();
    }


void log(std::string& where, std::string& what) {
    textwin->append(QString::fromStdString(where+": "+what));
    }

    void show();
    void hide();
    void setVisible(bool checked=true);


protected:

  virtual int overflow(int);
  virtual std::streamsize xsputn(const char *, std::streamsize);


private:
    std::ostream &m_stream;
    std::streambuf *m_old_buf;
    std::string m_string;

    // Qt4.3+ we can  use QPlainTextEdit which "is optimized for use as a log display"
    // http://www.nabble.com/Log-viewer-td21499499.html
    QTextEdit * textwin;
    QDialog * window;


};

#endif
