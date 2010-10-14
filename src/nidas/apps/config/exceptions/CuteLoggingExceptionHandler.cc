

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
