

#include "CuteLoggingExceptionHandler.h"

#include <QBoxLayout>
#include <QPushButton>


void CuteLoggingExceptionHandler::show() { window->show(); }

void CuteLoggingExceptionHandler::hide() { window->hide(); }

void CuteLoggingExceptionHandler::setVisible(bool checked) { window->setVisible(checked); }

void CuteLoggingExceptionHandler::display(std::string& where, std::string& what) {
    log(where,what);
    window->show();
    }



CuteLoggingExceptionHandler::CuteLoggingExceptionHandler(QWidget * parent)
{
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
textwin->setLineWrapMode(QTextEdit::NoWrap);

QPushButton *clearButton = new QPushButton("Clear");
clearButton->setDefault(false);

QPushButton *closeButton = new QPushButton("Close");
closeButton->setDefault(true);

window->connect(clearButton,SIGNAL(clicked()),textwin,SLOT(clear()));
window->connect(closeButton,SIGNAL(clicked()),window,SLOT(hide()));

buttonLayout->addWidget(clearButton);
buttonLayout->addStretch(1);
buttonLayout->addWidget(closeButton);

mainLayout->addWidget(textwin);
mainLayout->addLayout(buttonLayout);

window->setLayout(mainLayout);
}
