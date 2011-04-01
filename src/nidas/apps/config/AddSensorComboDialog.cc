#include "AddSensorComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"
#include <dirent.h>
#include <set>
#include <sys/stat.h>

using namespace config;

QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _idRegEx("\\d+");
QRegExp _sfxRegEx("^(_\\S+)?$");

AddSensorComboDialog::AddSensorComboDialog(QString a2dCalDir, 
                                  QString pmsSpecsFile, QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
  connect(SensorBox, SIGNAL(currentIndexChanged(const QString &)), this, 
           SLOT(dialogSetup(const QString &)));
  SensorBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  A2DSNBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
//  DeviceText->setValidator( new QRegExpValidator (_deviceRegEx, this ));

  IdText->setValidator( new QRegExpValidator ( _idRegEx, this));
  SuffixText->setValidator( new QRegExpValidator ( _sfxRegEx, this));

  setupA2DSerNums(a2dCalDir);

  setupPMSSerNums(pmsSpecsFile);
 
  return;
}

void AddSensorComboDialog::setupPMSSerNums(QString pmsSpecsFile)
{

  // Initialize the PMSspex object using the default file.
  struct stat buffer ;
  if ( stat( pmsSpecsFile.toStdString().c_str(), &buffer ) == -1 ) {
    QMessageBox* errorMessage = new QMessageBox(this);
    errorMessage->setText("Could not find PMSSpecs file: " + pmsSpecsFile +
                          "\n Can't provide serial numbers for PMS probes.");
    errorMessage->exec();
    return;
  }

  PMSspex pmsSpex(pmsSpecsFile.toStdString());
  cerr<< "AddSensorComboDialog::" << __func__ <<
        " - created new pmsSpex with filename: " <<
        pmsSpecsFile.toStdString() << "\n";

  //char *list[];
  char *list[100];
  int num = pmsSpex.GetSerialNumList(list);

  QStringList pmsSerialNums;
  stringstream temp;
  for (int i = 0; i < num; i++) {
    temp << string(list[i]) << "\n";
    pmsSerialNums << QString(list[i]);
  }

  pmsSerialNums.sort();
  PMSSNBox->addItems(pmsSerialNums);
  for (int i = 0; i<<num; i++) delete list[i];
}

void AddSensorComboDialog::setupA2DSerNums(QString a2dCalDir)
{

  // Get listing of A2D calibration files to allow selection by  user
  DIR *dir;
  char *tmp_dir = (char*) malloc(a2dCalDir.size()+1);

  strcpy(tmp_dir, a2dCalDir.toStdString().c_str());
  //char *directory = dirname(tmp_dir);

  if ((dir = opendir(tmp_dir)) == 0)
  {
    QMessageBox *errorMessage = new QMessageBox(this);
    errorMessage->setText("Could not open A2D calibrations directory: " + 
                          a2dCalDir +
                          "\n Can't provide serial numbers for A2D Cards.");
    errorMessage->exec();
    free(tmp_dir);
    return;
  }

  struct dirent *entry;

  // Read directory entries and get files with matching A2D cal file form
  QStringList a2dCalFiles;
  while ( (entry = readdir(dir)) )
    if ( strstr(entry->d_name, "A2D") &&
         strstr(entry->d_name, ".dat"))
    {
      a2dCalFiles << QString(entry->d_name);
      //A2DSNBox->addItem(QString(entry->d_name));
      //cerr<<"found A2D cal file "<< entry->d_name << "\n";
    }
  a2dCalFiles.sort();
  A2DSNBox->addItems(a2dCalFiles);

  free(tmp_dir);
}


void AddSensorComboDialog::dialogSetup(const QString & sensor)
{
  if (sensor == QString("Analog")) 
  {
    A2DTempSuffixLabel->show();
    A2DTempSuffixText->show();
    A2DSNLabel->show();
    A2DSNBox->show();
  } else {
    A2DTempSuffixLabel->hide();
    A2DTempSuffixText->hide();
    A2DSNLabel->hide();
    A2DSNBox->hide();
  }

  if (sensor == QString("CDP") ||
      sensor == QString("Fast2DC") || 
      sensor == QString("S100") ||
      sensor == QString("S200") ||
      sensor == QString("S300") ||
      sensor == QString("TwoDP") ||
      sensor == QString("UHSAS"))
  {
    PMSSNLabel->show();
    PMSSNBox->show();
  } else {
    PMSSNLabel->hide();
    PMSSNBox->hide();
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
    std::cerr << " calfile: " + A2DSNBox->currentText().toStdString() + "\n";
    std::cerr << " a2dTempSfx: " + A2DTempSuffixText->text().toStdString() 
                 + "\n";
    std::cerr << " a2dSN: " + A2DSNBox->currentText().toStdString() + "\n";
    std::cerr << " pmsSN: " + PMSSNBox->currentText().toStdString() + "\n";

    try {
      if (_document) {
        if (_indexList.size() > 0)  
          _document->updateSensor(SensorBox->currentText().toStdString(),
                                  DeviceText->text().toStdString(),
                                  IdText->text().toStdString(),
                                  SuffixText->text().toStdString(),
                                  A2DTempSuffixText->text().toStdString(),
                                  A2DSNBox->currentText().toStdString(),
                                  PMSSNBox->currentText().toStdString(),
                                  _indexList
                                 );

        else
          _document->addSensor(SensorBox->currentText().toStdString(),
                               DeviceText->text().toStdString(),
                               IdText->text().toStdString(),
                               SuffixText->text().toStdString(),
                               A2DTempSuffixText->text().toStdString(),
			       A2DSNBox->currentText().toStdString(),
                               PMSSNBox->currentText().toStdString()
                              );
        DeviceText->clear();
        IdText->clear();
        SuffixText->clear();
      } else {
        _errorMessage->setText(QString(
                        "Internal Error: _document not set in AddSensorComboDialog "));
        _errorMessage->exec();
      }

    } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString(
                        "Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
    } catch ( nidas::util::InvalidParameterException &e) {
      _errorMessage->setText(QString::fromStdString("Invalid parameter: " + e.toString()));
      _errorMessage->exec();
      return; // do not accept, keep dialog up for further editing
    } catch (...) { 
      _errorMessage->setText("Caught Unspecified error");
      _errorMessage->exec(); 
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

void AddSensorComboDialog::existingSensor(SensorItem* sensorItem)
{
  // Set up the Sensor Name
  QString baseName = sensorItem->getBaseName();
  int index = SensorBox->findText(sensorItem->getBaseName());
  if (index != -1) SensorBox->setCurrentIndex(index);
cerr<<"AddSensorComboDialog setting edit text to" << baseName.toStdString() << "\n";
  SensorBox->setEnabled(false);

  // Set up the device name and channel/board box
  QString device = sensorItem->getDevice();
  std::string stdBaseName = baseName.toStdString();

  DeviceValidator * devVal = DeviceValidator::getInstance();
  if (devVal == 0) {
    cerr << "bad error: device validator is null\n";
    return;
  }

  std::string stdDevPrefix = devVal->getDevicePrefix(stdBaseName);
  QString devNumber = device.right(device.size()-stdDevPrefix.size());
  int dNum = devNumber.toInt();
  ChannelBox->setValue(dNum);
  ChannelBox->setMinimum(devVal->getMin(stdBaseName));
  ChannelBox->setMaximum(devVal->getMax(stdBaseName));
  ChannelLabel->setText(devVal->getInterfaceLabel(stdBaseName));

  setDevice(ChannelBox->value());

  // Set up the Sensor ID box
  DSMSensor *sensor = sensorItem->getDSMSensor();
  IdText->insert(QString::number(sensor->getSensorId()));

  // Set up the Suffix box
  SuffixText->insert(QString::fromStdString(sensor->getSuffix()));

  // Set up A2D Temp Suffix box and serial number/cal file box
  if (baseName == "Analog") {
    A2DSensorItem* a2dSensorItem = dynamic_cast<A2DSensorItem*>(sensorItem);
    A2DTempSuffixText->insert(a2dSensorItem->getA2DTempSuffix());

    std::string a2dCalFn = a2dSensorItem->getCalFileName();
    if (a2dCalFn.empty()) {
      _errorMessage->setText(QString::fromStdString(
                         "Warning - no current Serial Number for A2D card.\n")
                     + QString::fromStdString(
                         " Will default to first possible S/N.\n")
                     + QString::fromStdString (
                         "And it's associated calibration coeficients.\n"));
      _errorMessage->exec();
      A2DSNBox->setCurrentIndex(0);
    } else {
      int index = A2DSNBox->findText(QString::fromStdString(a2dCalFn));
      if (index != -1) A2DSNBox->setCurrentIndex(index);
      else {
        _errorMessage->setText(QString::fromStdString(
                           "Could not find A2D Serial number cal file: ") 
                       + QString::fromStdString(a2dCalFn) 
                       + QString::fromStdString (
                         ".  Suggest you look in to that missing file."));
        _errorMessage->exec();
      }
    }
  }

  cerr << "end of existingSensor()\n";
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
  // Clear out all the fields
  DeviceText->clear();
  IdText->clear();
  SuffixText->clear();
  A2DTempSuffixText->clear();

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

    existingSensor(sensorItem);


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
