#include "AddSensorComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>

using namespace config;

QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _idRegEx("\\d+");
QRegExp _sfxRegEx("^(_\\S+)?$");

AddSensorComboDialog::AddSensorComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
//  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));
  IdText->setValidator( new QRegExpValidator ( _idRegEx, this));
  SuffixText->setValidator( new QRegExpValidator ( _sfxRegEx, this));
  _errorMessage = new QMessageBox(this);
}


void AddSensorComboDialog::accept()
{
  if (IdText->hasAcceptableInput() &&
      SuffixText->hasAcceptableInput()) {
     std::cerr << "AddSensorComboDialog::accept()\n";
     std::cerr << " sensor: " + SensorBox->currentText().toStdString() + "\n";
     std::cerr << " device: " + DeviceText->text().toStdString() + "\n";
     std::cerr << " id: " + IdText->text().toStdString() + "\n";
     std::cerr << " suffix: " + SuffixText->text().toStdString() + "\n";

     try {
        if (_document) _document->addSensor(SensorBox->currentText().toStdString(),
                                         DeviceText->text().toStdString(),
                                         IdText->text().toStdString(),
                                         SuffixText->text().toStdString()
                                         );
     } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
     } catch ( nidas::util::InvalidParameterException &e) {
        _errorMessage->setText(QString::fromStdString("Invalid parameter: " + e.toString()));
        _errorMessage->exec();
        return; // do not accept, keep dialog up for further editing
     }

     QDialog::accept(); // accept (or bail out) and make the dialog disappear

  }  else {
     _errorMessage->setText("Unacceptable input in either Device or Id fields");
     _errorMessage->exec();
     std::cerr << "Unaccptable input in either Device or Id fields\n";
  }
}

void AddSensorComboDialog::newSensor(QString sensor)
{
   std::cerr << "New Sensor selected " << sensor.toStdString() << std::endl;
}

void AddSensorComboDialog::setDevice(int channel)
{
   std::cerr << "New device channel selected " << channel << std::endl;
}
