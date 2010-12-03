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
  connect(SensorBox, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(A2DTempSetup(const QString &)));
//  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));
  IdText->setValidator( new QRegExpValidator ( _idRegEx, this));
  SuffixText->setValidator( new QRegExpValidator ( _sfxRegEx, this));
  _errorMessage = new QMessageBox(this);
}

void AddSensorComboDialog::A2DTempSetup(const QString & sensor)
{
  if (sensor == QString("Analog")) 
  {
    A2DTempSuffixLabel->show();
    A2DTempSuffixText->show();
  } else {
    A2DTempSuffixLabel->hide();
    A2DTempSuffixText->hide();
  }
}

void AddSensorComboDialog::accept()
{
  if (SensorBox->currentText() == QString("Analog") &&
      A2DTempSuffixText->text().isEmpty())
  {
    _errorMessage->setText("A2D Temp Suffix must be set when Analog Sensor is selected - Please enter a suffix");
    _errorMessage->exec();
    return;
  }
  if (IdText->hasAcceptableInput() &&
      SuffixText->hasAcceptableInput()) {
     std::cerr << "AddSensorComboDialog::accept()\n";
     std::cerr << " sensor: " + SensorBox->currentText().toStdString() + "\n";
     std::cerr << " device: " + DeviceText->text().toStdString() + "\n";
     std::cerr << " id: " + IdText->text().toStdString() + "\n";
     std::cerr << " suffix: " + SuffixText->text().toStdString() + "\n";

     try {
        if (_document) {
           _document->addSensor(SensorBox->currentText().toStdString(),
                                         DeviceText->text().toStdString(),
                                         IdText->text().toStdString(),
                                         SuffixText->text().toStdString(),
                                         A2DTempSuffixText->text().toStdString()
                                         );
           DeviceText->clear();
           IdText->clear();
           SuffixText->clear();
        } else {
           _errorMessage->setText(QString("Internal Error: _document not set in AddSensorComboDialog "));
           _errorMessage->exec();
        }

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

void AddSensorComboDialog::show(NidasModel* model, 
                                QModelIndexList indexList)
{
  _model = model;
  _indexList = indexList;

  if (setUpDialog())
    this->QDialog::show();
}

// Note: the SensorBox list is set up by configwindow in buildSensorCatalog
bool AddSensorComboDialog::setUpDialog()
{
  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the SensorItem we are editing.
  NidasItem *item;
  if (_indexList.size() > 0)  {
    std::cerr<< "SensorItemDialog called in edit mode\n";
    for (int i=0; i<_indexList.size(); i++) {
      QModelIndex index = _indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
      item = _model->getItem(index);
    }

    SensorItem* sensorItem = dynamic_cast<SensorItem*>(item);
    if (!sensorItem)
      throw InternalProcessingException("Selection is not a Sensor.");

    QString baseName = sensorItem->getBaseName();
    int index = SensorBox->findText(sensorItem->getBaseName());
    if (index != -1) SensorBox->setCurrentIndex(index);
cerr<<"AddSensorComboDialog setting edit text to" << baseName.toStdString() << "\n";
    SensorBox->setEnabled(false);

  } else {
    std::cerr<< "SensorItemDialog called in add mode\n";
    SensorBox->setEnabled(true);
    newSensor(SensorBox->currentText());
    setDevice(ChannelBox->value());
    try {
      if (_document) IdText->setText(QString::number(_document->getNextSensorId()));
      cerr<<"after call to getNextSensorId"<<endl;
    } catch ( InternalProcessingException &e) {
      _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + 
                                                       e.toString()));
      _errorMessage->exec();
      return false;
    }
  }

return true;
}
