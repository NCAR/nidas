/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
#include "AddSensorComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"
#include <dirent.h>
#include <set>
#include <sys/stat.h>
#include <sstream>

using namespace config;

QRegExp _deviceRegEx("/dev/[a-zA-Z/_0-9.\\-+]+");
QRegExp _idRegEx("\\d+");

/*
 * Important note:  Some connections are set up in the designer.
 * - e.g. changes in SensorBox trigger a call to newSensor
 */

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
  
  setupA2DSerNums(a2dCalDir+"/DMMAT/");
  setupA2DSerNums(a2dCalDir);

  setupPMSSerNums(pmsSpecsFile);
 
  _errorMessage = new QMessageBox(this);
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
  std::string serNum;
  std::string resolution;
  char rangeStep[] = "RANGE_STEP";
  size_t dotLoc;
  for (int i = 0; i < num; i++) {
    temp << string(list[i]) << "\n";
    serNum = string(list[i]);
    resolution = pmsSpex.GetParameter(serNum, rangeStep); 
    dotLoc = resolution.find(".");
    if (dotLoc!=std::string::npos)
      resolution.replace(dotLoc,resolution.size()-dotLoc,"");
//std::cerr<<"PMSSpec:"<<serNum<<" "<<rangeStep<<":"<<resolution<<"|\n";
    _pmsResltn[serNum]=resolution;
    pmsSerialNums << QString(list[i]);
  }

  pmsSerialNums.sort();
  PMSSNBox->addItems(pmsSerialNums);
  for (int i = 0; i<<num; i++) delete list[i];
}

/**
 * Obtains cal files for both NCAR A2D Cards 
 * Takes QString a2dCalDir
 * Adds all a2dCalFiles to A2DSNBox 
 * Returns nothing
 * */
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
  if (sensor == QString("ANALOG_NCAR")) 
  {
    A2DTempSuffixLabel->show();
    A2DTempSuffixText->show();
    A2DSNLabel->show();
    A2DSNBox->show();
  }else if (sensor == QString("ANALOG_DMMAT")) 
  {
    A2DTempSuffixLabel->hide();
    A2DTempSuffixText->hide();
    A2DSNLabel->show();
    A2DSNBox->show();
  } else
  {
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


  if (SensorBox->currentText() == QString("ANALOG_DMMAT") ){
        cerr<<"DMMAT adding correctly in AddSensor ComboDialog::accept"<<endl;
    }
  if (SensorBox->currentText() == QString("ANALOG_NCAR") &&
      A2DTempSuffixText->text().isEmpty())
  {
    _errorMessage->setText("A2D Temp Suffix must be set when Analog Sensor is selected - Please enter a suffix");
    _errorMessage->exec();
    return;
  }
  if (IdText->hasAcceptableInput()) 
  {

    // Clean up the suffixes - one and exactly one underscore at the beginning
    QString suffixText=SuffixText->text();
    if (suffixText.length() > 0) {
      suffixText.replace("_", "");
      suffixText.prepend("_");
      SuffixText->clear();
      SuffixText->insert(suffixText);
    }
    suffixText=A2DTempSuffixText->text();
    if (suffixText.length() > 0) {
      suffixText.replace("_", "");
      suffixText.prepend("_");
      A2DTempSuffixText->clear();
      A2DTempSuffixText->insert(suffixText);
    }

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
    std::cerr << " pmsRES: " + _pmsResltn[PMSSNBox->currentText().toStdString()] + "\n";

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
                              _pmsResltn[PMSSNBox->currentText().toStdString()],
                              _indexList
                             );

        else
          _document->addSensor(SensorBox->currentText().toStdString(),
                              DeviceText->text().toStdString(),
                              IdText->text().toStdString(),
                              SuffixText->text().toStdString(),
                              A2DTempSuffixText->text().toStdString(),
			      A2DSNBox->currentText().toStdString(),
                              PMSSNBox->currentText().toStdString(),
                              _pmsResltn[PMSSNBox->currentText().toStdString()]
                             );
        DeviceText->clear();
        IdText->clear();
        SuffixText->clear();
        _document->setIsChanged(true);
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

/**
 * Dialog for a new sensor
 * Takes QString sensor
 * */
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

   SuffixText->clear();
   SuffixText->insert(_sfxMap[sensor]);
   if (_devMap[sensor].size()) {
     DeviceText->clear();
     DeviceText->setText(_devMap[sensor]);

     // Find the beginning of the port or device number
     std::string sDevice = _devMap[sensor].toStdString();
     size_t numStart = sDevice.find_first_of("0123456789");
     if (numStart != std::string::npos) {
       std::string sDevNum = sDevice.substr(numStart);
       istringstream buffer(sDevNum);
       int iDevNum;
       buffer>>iDevNum;
       ChannelBox->setValue(iDevNum);
     }
   }

   cerr << "end of newSensor()\n";
}
/**
 * SetDevice
 * Takes int channel
 * calls setText on fullDecive
 *
 * */
void AddSensorComboDialog::setDevice(int channel)
{
   QString sensor = SensorBox->currentText();
   std::cerr << "New device channel selected " << channel << std::endl;
   DeviceValidator * devVal = DeviceValidator::getInstance();
   std::string stdSensor = sensor.toStdString();
cout<<"setDevice stdSensor:"<<stdSensor<<endl;

   std::string dev = devVal->getDevicePrefix(stdSensor);
   QString fullDevice = QString::fromStdString(dev) + QString::number(channel);
   DeviceText->setText(fullDevice);
}

void AddSensorComboDialog::existingSensor(SensorItem* sensorItem)
{
  // Set up the Sensor Name
  QString baseName = sensorItem->getBaseName();
  int index = SensorBox->findText(sensorItem->getBaseName());
  cerr<<"AddSensorComboDialog index:"<<index<<" baseName:"<<baseName.toStdString()<<endl;
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
cerr<<"AddSensorItem IdText:"  << IdText<<"\n";

  // Set up the Suffix box
  SuffixText->insert(QString::fromStdString(sensor->getSuffix()));

  // Set up A2D Temp Suffix box and serial number/cal file box
  if (baseName == "ANALOG_NCAR"||baseName == "ANALOG_DMMAT") {
    A2DSensorItem* a2dSensorItem = dynamic_cast<A2DSensorItem*>(sensorItem);
    if (baseName == "ANALOG_NCAR"){
        A2DTempSuffixText->insert(a2dSensorItem->getA2DTempSuffix());
        std::string a2dCalFn = a2dSensorItem->getCalFileName();
    
        if (a2dCalFn.empty()) {
    cerr<<"AddSensorComboDialog4 setting edit text 5to" << baseName.toStdString() << "\n";
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
  }

  if (baseName == QString("CDP") ||
      baseName == QString("Fast2DC") || 
      baseName == QString("S100") ||
      baseName == QString("S200") ||
      baseName == QString("S300") ||
      baseName == QString("TwoDP") ||
      baseName == QString("UHSAS"))
  {
    std::string pmsSN = sensorItem->getSerialNumberString();
    if (pmsSN.empty()) {
      _errorMessage->setText(QString::fromStdString(
                          "Warning - no current Serial Number for PMS probe.\n")
                          + QString::fromStdString(
                          " Will default to first possible S/N.\n"));
      _errorMessage->exec();
      PMSSNBox->setCurrentIndex(0);
    } else {
      int index = PMSSNBox->findText(QString::fromStdString(pmsSN));
      if (index != -1) PMSSNBox->setCurrentIndex(index);
      else {
        _errorMessage->setText(QString::fromStdString(
                         "Could not find PMS Serial number in PMSSpecs file: ")
                         + QString::fromStdString(pmsSN)
                         + QString::fromStdString (
                         ".  Suggest you look in to that missing element."));
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
std::cerr<<"show _pmsRes:"<<_pmsResltn["2DC18"]<<"|\n";

  SensorBox->setFocus(Qt::ActiveWindowFocusReason);

  if (setUpDialog())
    this->QDialog::show();
}

// Note: the SensorBox list is set up by configwindow in buildSensorCatalog
bool AddSensorComboDialog::setUpDialog()
{
cerr<<__func__<<"\n";
  // Clear out all the fields
  DeviceText->clear();
  IdText->clear();
  SuffixText->clear();
  A2DTempSuffixText->clear();
std::cerr<< "in setup dialog:hello"<<endl;
  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the SensorItem we are editing.
  NidasItem *item = NULL;
  if (_indexList.size() > 0)  {
    std::cerr<< "SensorItemDialog called in edit mode\n";
    for (int i=0; i<_indexList.size(); i++) {
      QModelIndex index = _indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
      item = _model->getItem(index);
        cerr<<"Item ="<<item<<endl;
    }

    SensorItem* sensorItem = dynamic_cast<SensorItem*>(item);
    if (!sensorItem)
      throw InternalProcessingException("Selection is not a Sensor.");
cerr<<"should see this before existing sensor is called"<<endl;

    existingSensor(sensorItem);
cerr<<"should probably not see this before existing sensor is called4"<<endl;


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
