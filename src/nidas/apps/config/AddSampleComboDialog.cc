#include "AddSampleComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

using namespace config;

QRegExp _sampleIdRegEx("\\d+");

AddSampleComboDialog::AddSampleComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
  SampleIdText->setValidator( new QRegExpValidator ( _sampleIdRegEx, this));
  _errorMessage = new QMessageBox(this);
}


void AddSampleComboDialog::accept()
{
  if (SampleIdText->hasAcceptableInput()) {
     std::cerr << "AddSampleComboDialog::accept()\n";
     std::cerr << " id: " + SampleIdText->text().toStdString() + "<EOS>\n";
     std::cerr << " Rate: " + SampleRateText->text().toStdString() + "<EOS>\n";
     std::cerr << " filter: " + FilterText->text().toStdString() + "<EOS>\n";

     try {
        // TODO - write document addSample
        //if (_document) _document->addSample(SampleIdText->text().toStdString(),
          //                               SampleRateText->text().toStdString(),
          //                               FilterText->text().toStdString()
           //                              );
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
     std::cerr << "Unaccptable input in either Name or Id fields\n";
  }
}

/*
void AddSampleComboDialog::newSample(QString sensor)
{
   std::string stdSample = sensor.toStdString();
   std::cerr << "New Sample selected " << stdSample << std::endl;

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

   cerr << "sensor " << stdSample << "\n";
   cerr << "min " << devVal->getMin(stdSample) << "\n";
   cerr << "max " << devVal->getMax(stdSample) << "\n";
   cerr << "label " << devVal->getInterfaceLabel(stdSample) << "\n";
   cerr << "device " << devVal->getDevicePrefix(stdSample) << "\n";

   int min = devVal->getMin(stdSample);
   ChannelLabel->setText(devVal->getInterfaceLabel(stdSample));
   ChannelBox->setMinimum(min);
   ChannelBox->setMaximum(devVal->getMax(stdSample));
   if (ChannelBox->value() == min) setDevice(min);
   ChannelBox->setValue(min);
   cerr << "end of newSample()\n";
}
*/

/*
void AddSampleComboDialog::setDevice(int channel)
{
   QString sensor = SampleBox->currentText();
   std::cerr << "New device channel selected " << channel << std::endl;
   DeviceValidator * devVal = DeviceValidator::getInstance();
   std::string stdSample = sensor.toStdString();
   std::string dev = devVal->getDevicePrefix(stdSample);
   QString fullDevice = QString::fromStdString(dev) + QString::number(channel);
   DeviceText->setText(fullDevice);
}
*/

void AddSampleComboDialog::show()
{
   if (setUpDialog())
     this->QDialog::show();
}

bool AddSampleComboDialog::setUpDialog()
{
/*
   newSample(SampleBox->currentText());
   setDevice(ChannelBox->value());

   try {
     if (_document) IdText->setText(QString::number(_document->getNextSampleId()));
     cerr<<"after call to getNextSampleId"<<endl;
   } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
        return false;
        }
*/
SampleIdText->setText("999");

return true;
}
