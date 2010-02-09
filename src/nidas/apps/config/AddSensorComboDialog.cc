#include "AddSensorComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

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
     } catch (...) { _errorMessage->setText("Caught Unspecified error"); _errorMessage->exec(); }

     QDialog::accept(); // accept (or bail out) and make the dialog disappear

  }  else {
     _errorMessage->setText("Unacceptable input in either Device or Id fields");
     _errorMessage->exec();
     std::cerr << "Unaccptable input in either Device or Id fields\n";
  }
}

void AddSensorComboDialog::newSensor(QString sensor)
{
   std::string stdSensor = sensor.toStdString();
   std::cerr << "New Sensor selected " << stdSensor << std::endl;

   if (sensor.length()<=0) {
    ChannelBox->setMinimum(0);
    ChannelBox->setMaximum(0);
    DeviceText->setText("");
    return;
    }

   DeviceValidator * devVal = DeviceValidator::getInstance();
   if (devVal == 0) {
     cerr << "bad error: device validator is null\n";
     return;
     }

   cerr << "sensor " << stdSensor << "\n";
   cerr << "min " << devVal->getMin(stdSensor) << "\n";
   cerr << "max " << devVal->getMax(stdSensor) << "\n";
   cerr << "label " << devVal->getInterfaceLabel(stdSensor) << "\n";
   cerr << "device " << devVal->getDevicePrefix(stdSensor) << "\n";

   int min = devVal->getMin(stdSensor);
   ChannelLabel->setText(devVal->getInterfaceLabel(stdSensor));
   ChannelBox->setMinimum(min);
   ChannelBox->setMaximum(devVal->getMax(stdSensor));
   if (ChannelBox->value() == min) setDevice(min);
   ChannelBox->setValue(min);
   cerr << "end of newSensor()\n";
}

void AddSensorComboDialog::setDevice(int channel)
{
   QString sensor = SensorBox->currentText();
   std::cerr << "New device channel selected " << channel << std::endl;
   DeviceValidator * devVal = DeviceValidator::getInstance();
   std::string stdSensor = sensor.toStdString();
   std::string dev = devVal->getDevicePrefix(stdSensor);
   QString fullDevice = QString::fromStdString(dev) + QString::number(channel);
   DeviceText->setText(fullDevice);
}

void AddSensorComboDialog::show()
{
   if (setUpDialog())
     this->QDialog::show();
}

bool AddSensorComboDialog::setUpDialog()
{
   newSensor(SensorBox->currentText());
   setDevice(ChannelBox->value());
   try {
     if (_document) IdText->setText(QString::number(_document->getNextSensorId()));
     cerr<<"after call to getNextSensorId"<<endl;
   } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
        return false;
        }

return true;
}
