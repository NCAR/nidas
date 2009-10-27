#include "AddSensorComboDialog.h"

using namespace config;

QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _idRegEx("\\d+");

AddSensorComboDialog::AddSensorComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));
  IdText->setValidator( new QRegExpValidator ( _idRegEx, this));
  _errorMessage = new QMessageBox(this);
}


void AddSensorComboDialog::accept()
{
  if (DeviceText->hasAcceptableInput() && IdText->hasAcceptableInput()) {
     std::cerr << "AddSensorComboDialog::accept()\n";
     std::cerr << " " + SensorBox->currentText().toStdString() + "\n";
     std::cerr << " " + DeviceText->text().toStdString() + "\n";
     std::cerr << " " + IdText->text().toStdString() + "\n";
     if (_document) _document->addSensor(SensorBox->currentText().toStdString(),
                                         DeviceText->text().toStdString(),
                                         IdText->text().toStdString());
 
     QDialog::accept();
  }  else {
     _errorMessage->setText("Unacceptable input in either Device or Id fields");
     _errorMessage->exec();
     std::cerr << "Unaccptable input in either Device or Id fields\n";
  }
}
