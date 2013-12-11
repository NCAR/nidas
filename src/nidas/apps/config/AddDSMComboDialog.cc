#include "AddDSMComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

using namespace config;

//QRegExp _dsmNameRegEx("dsm[a-zA-Z/_0-9.\\-+]+");
QRegExp _dsmNameRegEx("[a-zA-Z/_0-9.\\-+]+");   // chars, ints and a few symbls
QRegExp _dsmIdRegEx("\\d\\d?");                 // One or two digit integer
QRegExp _dsmLocRegEx("\\S[\\S\\s]+");           // At least one character

AddDSMComboDialog::AddDSMComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
  DSMNameText->setValidator( new QRegExpValidator ( _dsmNameRegEx, this));
  DSMIdText->setValidator( new QRegExpValidator ( _dsmIdRegEx, this));
  LocationText->setValidator( new QRegExpValidator ( _dsmLocRegEx, this));
  _errorMessage = new QMessageBox(this);
}


void AddDSMComboDialog::accept()
{
  // Validate input and notify if problematic
  if (!DSMNameText->hasAcceptableInput()) {
    QString msg("Name field must be a sequence of letters, numbers and a few");
    msg.append(" characters incl: '_' '/' '.' '+' and '-'");
    _errorMessage->setText(msg);
    _errorMessage->exec();
    std::cerr << "Unaccptable input in Name field\n";
    return;
  }

  if (!DSMIdText->hasAcceptableInput()) {
    QString msg("Id field must be a one or two digit integer");
    _errorMessage->setText(msg);
    _errorMessage->exec();
    std::cerr << "Unaccptable input in Id field\n";
    return;
  }

  if (!LocationText->hasAcceptableInput()) {
    QString msg("Location cannot be left blank");
    _errorMessage->setText(msg);
    _errorMessage->exec();
    std::cerr << "Unaccptable input in Location field\n";
    return;
  }

  std::cerr << "AddDSMComboDialog::accept()\n";
  std::cerr << " DSM: " + DSMNameText->text().toStdString() + "<EOS>\n";
  std::cerr << " id: " + DSMIdText->text().toStdString() + "<EOS>\n";
  std::cerr << " location: " + LocationText->text().toStdString() + "<EOS>\n";

  try {
     if (_document) {
       if (_indexList.size() > 0)
         _document->updateDSM(DSMNameText->text().toStdString(),
                              DSMIdText->text().toStdString(),
                              LocationText->text().toStdString(),
                              _indexList
                              );
       else
         _document->addDSM(DSMNameText->text().toStdString(),
                           DSMIdText->text().toStdString(),
                           LocationText->text().toStdString()
                           );
        _document->setIsChanged(true);
     }
  } catch ( InternalProcessingException &e) {
    QString msg("Bad internal error. Get help! : ");
    msg.append(QString::fromStdString(e.toString()));
    _errorMessage->setText(msg);
    _errorMessage->exec();
  } catch ( nidas::util::InvalidParameterException &e) {
    QString msg("Invalid parameter: ");
    msg.append(QString::fromStdString(e.toString()));
    _errorMessage->setText(msg);
    _errorMessage->exec();
    return; // do not accept, keep dialog up for further editing
  } catch (...) { 
    _errorMessage->setText("Caught Unspecified error"); 
    _errorMessage->exec(); 
  }

  QDialog::accept(); // accept (or bail out) and make the dialog disappear

}

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

void AddDSMComboDialog::show(NidasModel* model,
                             QModelIndexList indexList)
{
  _model = model;
  _indexList = indexList;

   if (setUpDialog())
     this->QDialog::show();
}

bool AddDSMComboDialog::setUpDialog()
{
  // Clear out all the fields
  DSMNameText->clear();
  DSMIdText->clear();
  LocationText->clear();

  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the SensorItem we are editing.
  NidasItem *item = NULL;
  if (_indexList.size() > 0)  {
    std::cerr<< "DSM Item Dialog called in edit mode\n";
    for (int i=0; i<_indexList.size(); i++) {
      QModelIndex index = _indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
      item = _model->getItem(index);
    }

    DSMItem* dsmItem = dynamic_cast<DSMItem*>(item);
    if (!dsmItem)
      throw InternalProcessingException("Selection is not a DSM.");

    existingDSM(dsmItem);

    DSMNameText->setFocus(Qt::ActiveWindowFocusReason);

  } else {  // New DSM being added.

    DSMNameText->insert("dsm");
    try {
      if (_document) DSMIdText->setText(QString::number(_document->getNextDSMId()));
    } catch ( InternalProcessingException &e) {
      _errorMessage->setText(QString::fromStdString("Bad internal error. Get help! " + e.toString()));
      _errorMessage->exec();
      return false;
    }
  }

return true;
}

void AddDSMComboDialog::existingDSM(DSMItem *dsmItem)
{
  // Fill in fields 
  DSMConfig *dsm = dsmItem->getDSMConfig();
  DSMNameText->insert(QString::fromStdString(dsm->getName()));
  DSMIdText->insert(QString::number(dsm->getId()));
  LocationText->insert(QString::fromStdString(dsm->getLocation()));
 
  return;
}
