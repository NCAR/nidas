
#include "AddSensorComboDialog.h"

using namespace config;

QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _idRegEx("\\d+");

AddSensorComboDialog::AddSensorComboDialog(QWidget *parent) :
    QDialog(parent)
{
  setupUi(this);
  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));
  IdText->setValidator( new QRegExpValidator ( _idRegEx, this));
}


void AddSensorComboDialog::accept()
{
  if (DeviceText->hasAcceptableInput() && IdText->hasAcceptableInput()) {
     std::cerr << "AddSensorComboDialog::accept()\n";
     std::cerr << " " + SensorBox->currentText().toStdString() + "\n";
     std::cerr << " " + DeviceText->text().toStdString() + "\n";
     std::cerr << " " + IdText->text().toStdString() + "\n";
  }  else {
     std::cerr << "Unaccptable input in either Device or Id fields\n";
  }
}
