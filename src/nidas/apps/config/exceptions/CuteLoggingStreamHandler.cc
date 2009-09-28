

#include "CuteLoggingStreamHandler.h"

#include <QBoxLayout>
#include <QPushButton>
#include <string>


void CuteLoggingStreamHandler::show() { window->show(); }

void CuteLoggingStreamHandler::hide() { window->hide(); }

void CuteLoggingStreamHandler::setVisible(bool checked) { window->setVisible(checked); }



CuteLoggingStreamHandler::CuteLoggingStreamHandler(std::ostream &stream, QWidget * parent) :
    m_stream(stream)
{
    // setup streams
m_old_buf = stream.rdbuf();
stream.rdbuf(this);

    // setup Qt
window = new QDialog(parent);
window->hide();
window->resize(600,300);
window->setWindowTitle("Errors");

QBoxLayout *mainLayout = new QVBoxLayout;
QBoxLayout *buttonLayout = new QHBoxLayout;

textwin = new QTextEdit;
textwin->setTextColor(Qt::black);
textwin->setReadOnly(true);
QSizePolicy sp(QSizePolicy::Expanding,QSizePolicy::Expanding);
textwin->setSizePolicy(sp);

QPushButton *clearButton = new QPushButton("Clear");
QPushButton *closeButton = new QPushButton("Close");

window->connect(clearButton,SIGNAL(clicked()),textwin,SLOT(clear()));
window->connect(closeButton,SIGNAL(clicked()),window,SLOT(hide()));

buttonLayout->addWidget(clearButton);
buttonLayout->addStretch(1);
buttonLayout->addWidget(closeButton);

mainLayout->addWidget(textwin);
mainLayout->addLayout(buttonLayout);

window->setLayout(mainLayout);
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
