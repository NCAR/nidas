

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
    window->show();
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
