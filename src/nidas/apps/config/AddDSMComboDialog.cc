#include "AddDSMComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

using namespace config;

//QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _dsmNameRegEx("^(dsm\\S+)?$");
QRegExp _dsmIdRegEx("\\d+");
//QRegExp _sfxRegEx("^(_\\S+)?$");

AddDSMComboDialog::AddDSMComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
//  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));
  DSMNameText->setValidator( new QRegExpValidator ( _dsmNameRegEx, this));
  DSMIdText->setValidator( new QRegExpValidator ( _dsmIdRegEx, this));
//  SuffixText->setValidator( new QRegExpValidator ( _sfxRegEx, this));
  _errorMessage = new QMessageBox(this);
}


/*
void AddDSMComboDialog::accept()
{
  if (IdText->hasAcceptableInput() &&
      SuffixText->hasAcceptableInput()) {
     std::cerr << "AddDSMComboDialog::accept()\n";
     std::cerr << " sensor: " + DSMBox->currentText().toStdString() + "\n";
     std::cerr << " device: " + DeviceText->text().toStdString() + "\n";
     std::cerr << " id: " + IdText->text().toStdString() + "\n";
     std::cerr << " suffix: " + SuffixText->text().toStdString() + "\n";

     try {
        if (_document) _document->addDSM(DSMBox->currentText().toStdString(),
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
*/

/*
void AddDSMComboDialog::newDSM(QString sensor)
{
   std::string stdDSM = sensor.toStdString();
   std::cerr << "New DSM selected " << stdDSM << std::endl;

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

   cerr << "sensor " << stdDSM << "\n";
   cerr << "min " << devVal->getMin(stdDSM) << "\n";
   cerr << "max " << devVal->getMax(stdDSM) << "\n";
   cerr << "label " << devVal->getInterfaceLabel(stdDSM) << "\n";
   cerr << "device " << devVal->getDevicePrefix(stdDSM) << "\n";

   int min = devVal->getMin(stdDSM);
   ChannelLabel->setText(devVal->getInterfaceLabel(stdDSM));
   ChannelBox->setMinimum(min);
   ChannelBox->setMaximum(devVal->getMax(stdDSM));
   if (ChannelBox->value() == min) setDevice(min);
   ChannelBox->setValue(min);
   cerr << "end of newDSM()\n";
}
*/

/*
void AddDSMComboDialog::setDevice(int channel)
{
   QString sensor = DSMBox->currentText();
   std::cerr << "New device channel selected " << channel << std::endl;
   DeviceValidator * devVal = DeviceValidator::getInstance();
   std::string stdDSM = sensor.toStdString();
   std::string dev = devVal->getDevicePrefix(stdDSM);
   QString fullDevice = QString::fromStdString(dev) + QString::number(channel);
   DeviceText->setText(fullDevice);
}
*/

void AddDSMComboDialog::show()
{
   if (setUpDialog())
     this->QDialog::show();
}

bool AddDSMComboDialog::setUpDialog()
{
/*
   newDSM(DSMBox->currentText());
   setDevice(ChannelBox->value());

   try {
     if (_document) IdText->setText(QString::number(_document->getNextDSMId()));
     cerr<<"after call to getNextDSMId"<<endl;
   } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
        return false;
        }
*/
DSMIdText->setText("999");

return true;
}
