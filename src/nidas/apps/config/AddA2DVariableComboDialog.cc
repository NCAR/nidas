#include "AddA2DVariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

using namespace config;

QRegExp _calRegEx("^-?\\d*.?\\d*");
QRegExp _nameRegEx("^[A-Z|0-9|_]*$");
QRegExp _unitRegEx("^\\S*$");
//QRegExp _nameRegEx("^\\S+$");

AddA2DVariableComboDialog::AddA2DVariableComboDialog(QWidget *parent): 
    QDialog(parent)
{
   setupUi(this);

   //Calib1Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   //Calib2Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   //Calib3Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   //Calib4Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   //Calib5Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   //Calib6Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   UnitsText->setValidator( new QRegExpValidator ( _unitRegEx, this));
   VoltageBox->addItem("  0 to  5 Volts");
   VoltageBox->addItem("  0 to 10 Volts");
   VoltageBox->addItem(" -5 to  5 Volts");
   VoltageBox->addItem("-10 to 10 Volts");
   ChannelBox->addItem("0");
   ChannelBox->addItem("1");
   ChannelBox->addItem("2");
   ChannelBox->addItem("3");
   ChannelBox->addItem("4");
   ChannelBox->addItem("5");
   ChannelBox->addItem("6");
   ChannelBox->addItem("7");
   SRBox->addItem("10");
   SRBox->addItem("100");
   SRBox->addItem("500");
   _vardb = 0;
}

void AddA2DVariableComboDialog::accept()
{
   bool editMode = false;
   if (_indexList.size() > 0)  editMode = true;
   
   //checkUnitsAndCalCoefs();

   // Don't allow variables to start with a numeric value
   QRegExp rx("\\d");
   if (rx.indexIn(VariableBox->currentText()) == 0) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(QString::fromStdString(
         "Variable names cannot begin with a numeric value."));
      _errorMessage->exec();
      return;
   }

   std::cerr << "AddA2DVariableComboDialog::accept()\n";
   // If we have a calibration, then we need a unit
   if (Calib1Text->text().size() && !UnitsText->text().size()) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(QString::fromStdString(
           "Must have units defined if a calibration is defined"));
      _errorMessage->exec();
      return;
   }

   // Make sure we have exactly one "_" at the beginning of the suffix
   // Document class handles inclusion of single "_" between prefix & suffix
   QString suffixText=SuffixText->text();
   if (suffixText.length() > 0) {
      suffixText.replace("_", "");
      //suffixText.prepend("_");  Now Document should take care of "_"
      SuffixText->clear();
      SuffixText->insert(suffixText);
   }
       
   std::cerr << " Name: " + VariableBox->currentText().toStdString() + "\n";
   std::cerr << " Sfx : " + SuffixText->text().toStdString() + "\n";
   std::cerr << " Long Name: " + LongNameText->text().toStdString() + "\n";
   std::cerr << "Volt Range Index: " << VoltageBox->currentIndex() << 
                " Val: " + VoltageBox->currentText().toStdString() +  "\n";
   std::cerr << " Channel Index: " << ChannelBox->currentIndex() <<
                " Val: " + ChannelBox->currentText().toStdString() + "\n";
   std::cerr << " SR Box Index: " << SRBox->currentIndex() <<
                " Val: " + SRBox->currentText().toStdString() + "\n";
   std::cerr << " Units: " + UnitsText->text().toStdString() + "\n";
   std::cerr << " Cals: " + Calib1Text->text().toStdString() + 
                  Calib2Text->text().toStdString() +
                  Calib3Text->text().toStdString() + 
                  Calib4Text->text().toStdString() +
                  Calib5Text->text().toStdString() + 
                  Calib6Text->text().toStdString() + "\n";

   try {
      // If we're in edit mode, we need to delete the A2DVariableItem 
      // from the model first and then we can add it back in.
      if (editMode)  {
         if(SRBox->currentIndex() !=_origSRBoxIndex) {
            QString msg("NOTE: changing the sample rate.");
            msg.append("For data acquisition you MAY need ");
            msg.append("to generate and use a new xml file.");
            QMessageBox * _errorMessage = new QMessageBox(this);
            _errorMessage->setText(msg);
            _errorMessage->setInformativeText("Do you want to continue?");
            _errorMessage->setStandardButtons(QMessageBox::Apply | 
                                              QMessageBox::Cancel);
            int ret = _errorMessage->exec();
            switch (ret) {
               case QMessageBox::Apply:
                  // All is fine
                  break;
               case QMessageBox::Cancel:
                  // bail
                  return;
                  default:
                  // HMM? Guess we should bail
                  cerr << "Unexpected return from message box!\n";
                  return;
            }
         }
         _model->removeIndexes(_indexList);
      }
     
      vector <std::string> cals;
      cals.push_back(Calib1Text->text().toStdString());
      cals.push_back(Calib2Text->text().toStdString());
      cals.push_back(Calib3Text->text().toStdString());
      cals.push_back(Calib4Text->text().toStdString());
      cals.push_back(Calib5Text->text().toStdString());
      cals.push_back(Calib6Text->text().toStdString());
      if (_document) {
         _document->addA2DVariable(VariableBox->currentText().toStdString(), 
                                    SuffixText->text().toStdString(),
                                    LongNameText->text().toStdString(),
                                    VoltageBox->currentText().toStdString(),
                                    ChannelBox->currentText().toStdString(),
                                    SRBox->currentText().toStdString(),
                                    UnitsText->text().toStdString(),
                                    cals);
         if (editMode) _document->setIsChanged(true);
         else _document->setIsChangedBig(true);
      }
   } catch ( InternalProcessingException &e) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(QString::fromStdString
                            ("Bad internal error. Get help! " + e.toString()));
      _errorMessage->exec();
   } catch ( nidas::util::InvalidParameterException &e) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(QString::fromStdString("Invalid parameter: " + 
                               e.toString()));
      _errorMessage->exec();
       return; // do not accept, keep dialog up for further editing
   } catch (...) { 
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText("Caught Unspecified error"); 
      _errorMessage->exec(); 
   }

   QDialog::accept(); // accept (or bail out) and make the dialog disappear

}

void AddA2DVariableComboDialog::show(NidasModel* model, 
                                     QModelIndexList indexList)
{
  clearForm();

  _model = model;
  _indexList = indexList;
  _origSRBoxIndex = -1;

  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the A2DVariableItem we are 
  // editing.
  NidasItem *item = NULL;
  if (indexList.size() > 0)  {
    setCalLabels();
std::cerr<< "A2DVariableDialog called in edit mode\n";
    _addMode = false;
    setWindowTitle("Edit Variable");
    for (int i=0; i<indexList.size(); i++) {
      QModelIndex index = indexList[i];
      // the NidasItem for the selected row resides in column 0
      if (index.column() != 0) continue;
      if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
      item = model->getItem(index);
    }

    A2DVariableItem* a2dVarItem = dynamic_cast<A2DVariableItem*>(item);
    if (!a2dVarItem)
      throw InternalProcessingException("Selection is not an A2DVariable.");

    // TODO:
    // if (removeSuffix(a2dVarItem->name()) == "A2DTEMP") 
    //    Need to set up the form for Suffix edit only
    
    int index = VariableBox->findText(removeSuffix(a2dVarItem->name()));
    if (index == -1) {
      QMessageBox * errorMessage = new QMessageBox(this);
      QString msg("Variable:");
      msg.append(removeSuffix(a2dVarItem->name()));
      msg.append(" does not appear as an A2D variable in VarDB.\n");
      msg.append(" Adding to list to allow for editing here.\n");
      msg.append(" Recommend correcting in VarDB.");
      errorMessage->setText(msg);
      errorMessage->exec();

      VariableBox->addItem(removeSuffix(a2dVarItem->name()));
      index = VariableBox->findText(removeSuffix(a2dVarItem->name()));
    }

    LongNameText->insert(a2dVarItem->getLongName());

    int gain = a2dVarItem->getGain();
    int bipolar = a2dVarItem->getBipolar();
    if (gain == 4 && bipolar == 0) VoltageBox->setCurrentIndex(0);
    if (gain == 2 && bipolar == 0) VoltageBox->setCurrentIndex(1);
    if (gain == 2 && bipolar == 1) VoltageBox->setCurrentIndex(2);
    if (gain == 1 && bipolar == 1) VoltageBox->setCurrentIndex(3);

    ChannelBox->addItem(QString::number(a2dVarItem->getA2DChannel()));
    float rate = a2dVarItem->getRate();
    if (rate == 10.0)  {
      SRBox->setCurrentIndex(0);
      _origSRBoxIndex = 0;
    }
    if (rate == 100.0) {
      SRBox->setCurrentIndex(1);
      _origSRBoxIndex = 1;
    }
    if (rate == 500.0) {
      SRBox->setCurrentIndex(2);
      _origSRBoxIndex = 2;
    }
    if (rate != 10.0 && rate != 100.0 && rate != 500.0) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      QString msg("Current Sample Rate:");
      msg.append(QString::number(rate));
      msg.append(" is not one of the 'standard' rates (10,100,500)\n");
      msg.append("Fixing rate to that value - no editing allowed.");
      _errorMessage->setText(msg);
      _errorMessage->exec();
      SRBox->addItem(QString::number(rate));
      SRBox->setCurrentIndex(3);
      _origSRBoxIndex = 3;
      SRBox->setEnabled(false);
    }

    std::vector<std::string> calInfo = a2dVarItem->getCalibrationInfo();

std::cerr<<__func__<<" Members of calInfo vector are:\n";
for (std::vector<std::string>::iterator it = calInfo.begin(); it != calInfo.end(); it++) {
std::cerr<<*it<<" ";
}
std::cerr<<"\n";
    if (calInfo.size() > 0) {
      if (calInfo.size() == 1 
          && calInfo[0] == std::string("No Calibrations Found")) {
        CalLabel->setText(QString("No Calibrations Found"));
        Calib1Text->setText(QString("0"));
        Calib2Text->setText(QString("1"));
        UnitsText->setText(QString("V"));
      } else if (calInfo.size() > 7 || calInfo.size() < 4) {
        std::cerr << "Unexpected # of cal info items from a2dVarItem\n";
      } else {
        if (calInfo.back().size() == 0)
           UnitsText->insert(QString("V"));
        else
           UnitsText->insert(QString::fromStdString(calInfo.back()));
        calInfo.pop_back();
        if (calInfo[0] == std::string("XML:")) {
           CalLabel->setText(QString("Calibrations:XML"));
           calInfo.erase(calInfo.begin());
        } else if (calInfo[0] == std::string("CalFile:")) {
//TODO: Not actually getting cals from the file for display
           CalLabel->setText(QString("Calibrations:File"));
           calInfo.erase(calInfo.begin());
        } else
           std::cerr << "Unexpected calinfo from a2dVarItem\n";
        switch (calInfo.size()) {
          case 6: Calib6Text->setText(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 5: Calib5Text->setText(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 4: Calib4Text->setText(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 3: Calib3Text->setText(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 2: Calib2Text->setText(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
                  Calib1Text->setText(QString::fromStdString(calInfo.back()));
        }
      }
    } else {
        // If there is no calibration info then we're just measuring Volts
        CalLabel->setText(QString("No Calibrations Found"));
        UnitsText->insert(QString("V"));
        Calib1Text->setText(QString("0"));
        Calib2Text->setText(QString("1"));
    }

    // Since change of index will trigger dialogSetup we need to 
    // do this step last
    if (index != -1) VariableBox->setCurrentIndex(index);
    VariableBox->setEnabled(false);
    SuffixText->insert(getSuffix(a2dVarItem->name()));

  } else {
      clearCalLabels();
      setWindowTitle("Add Variable");
      _addMode = true;
      VariableBox->setEnabled(true);
      VariableBox->setCurrentIndex(0);
      VariableBox->setEditable(true);
std::cerr<< "A2DVariableDialog called in add mode\n";
  }

  SetUpChannelBox();

  VariableBox->setFocus(Qt::ActiveWindowFocusReason);

  this->QDialog::show();
}

void AddA2DVariableComboDialog::SetUpChannelBox()
{
  // Set up the Channel box by adding available a2d channels on this card.
  list<int> channels;
  if (_document) channels = _document->getAvailableA2DChannels();
  list<int>::iterator it;
  for (it=channels.begin(); it != channels.end(); it++)
    ChannelBox->addItem(QString::number(*it));

}

void AddA2DVariableComboDialog::dialogSetup(const QString & variable)
{
   QMessageBox * _errorMessage = new QMessageBox(this);
   if (_addMode) {
      if (VariableBox->currentIndex() == 0) {
         VariableBox->setEditable(true);
      } else {
         VariableBox->setEditable(false);
      }
      clearForm();
      SetUpChannelBox();
   }

   cerr<< __func__ <<"("<<variable.toStdString().c_str()<<")\n";
   if (_addMode && variable == "New") return;
   if (variable == "New") return;  // edit mode w/New selected
   if (variable.size() == 0) return;  // happens on a new proj open

    VDBVar* vdbVar = _vardb->search_var(variable.toStdString());
    if (vdbVar == NULL) {
        // Should not happen, but check anyway.
        QString msg("Could not find variable:\n");
        msg.append(variable); msg.append("\n");
        msg.append("in the Variable database.");
        return;
    }

    QString vDBTitle(vdbVar->get_attribute(VDBVar::LONG_NAME).c_str());
    if (!_addMode && LongNameText->text() != vDBTitle) {
        QString msg("VarDB/Configuration missmatch: \n");
        msg.append("   VarDB Title: "); msg.append(vDBTitle);
        msg.append("\n"); msg.append("   Config has : ");
        msg.append(LongNameText->text());
        msg.append("\n   Using Config value.");
        _errorMessage->setText(msg);
        _errorMessage->exec();
    }
    if (_addMode) LongNameText->insert(vDBTitle);


    QString vRange(vdbVar->get_attribute(VDBVar::VOLTAGE_RANGE).c_str());
    QStringList vRangeList = vRange.split(" ");
    int32_t vLow = vRangeList.at(0).toInt();
    int32_t vHigh = vRangeList.at(1).toInt();
    cerr<<"    - VarDB.xml lookup vLow:"<<vLow<<"  vHigh:"<<vHigh<<"  addmode:"<<_addMode<<"\n";

    if (vLow == 0 && vHigh == 5) {
        if(!_addMode && VoltageBox->currentIndex() != 0) 
            showVoltErr(vLow, vHigh, VoltageBox->currentIndex());
        VoltageBox->setCurrentIndex(0);
    }
    else if (vLow == 0 && vHigh == 10) {
        if(!_addMode && VoltageBox->currentIndex() != 1) 
            showVoltErr(vLow, vHigh, VoltageBox->currentIndex());
        VoltageBox->setCurrentIndex(1);
    }
    else if (vLow == -5 && vHigh == 5) 
    {
        if(!_addMode && VoltageBox->currentIndex() != 2) 
            showVoltErr(vLow, vHigh, VoltageBox->currentIndex());
        VoltageBox->setCurrentIndex(2);
    }
    else if (vLow == -10 && vHigh == 10) {
        if(!_addMode && VoltageBox->currentIndex() != 3) 
            showVoltErr(vLow, vHigh, VoltageBox->currentIndex());
        VoltageBox->setCurrentIndex(3);
    }
    else {
        //QMessageBox * _errorMessage = new QMessageBox(this);
        QString msg("VarDB error: Range ");
        msg.append(QString::number(vLow));
        msg.append(" - ");
        msg.append(QString::number(vHigh));
        msg.append(" is nonstandard - run vared to fix.  ");
        msg.append("Defaulting to 0-5V.");
        _errorMessage->setText(msg);
        _errorMessage->exec();
        VoltageBox->setCurrentIndex(0);
    } 
   
    int32_t sRate=atoi(vdbVar->get_attribute(VDBVar::DEFAULT_SAMPLE_RATE).c_str());
cerr<<"    - VarDB.xml lookup sRate:"<<sRate<<"\n";
    switch (sRate) {
        case 10 : 
           if (!_addMode && SRBox->currentIndex() != 0) {
               showSRErr(sRate, SRBox->currentIndex());
           }
           SRBox->setCurrentIndex(0);
           break;
       case 100 :
           if (!_addMode && SRBox->currentIndex() != 1) {
               showSRErr(sRate, SRBox->currentIndex());
           }
           SRBox->setCurrentIndex(1);
           break;
       case 500 :
           if (!_addMode && SRBox->currentIndex() != 2) {
               showSRErr(sRate, SRBox->currentIndex());
           }
           SRBox->setCurrentIndex(2);
           break;
       default:
           //QMessageBox * _errorMessage = new QMessageBox(this);
           QString msg("VarDB error: Default Sample Rate: ");
           msg.append(QString::number(sRate));
           msg.append(" is nonstandard - run vared to fix.");
           msg.append(" Defaulting to 10 SPS.");
           _errorMessage->setText(msg);
           _errorMessage->exec();
           SRBox->setCurrentIndex(0);
    }


    QString vDBUnits(vdbVar->get_attribute(VDBVar::UNITS).c_str());
cerr<<"    -VarDB.xml lookup Units:"<<vDBUnits.toStdString()<<"\n";

    if (!_addMode && UnitsText->text() != vDBUnits) {
        //QMessageBox * _errorMessage = new QMessageBox(this);
        QString msg("VarDB/Configuration missmatch: \n");
        msg.append("   VarDB Units: "); msg.append(vDBUnits); 
        msg.append("\n"); msg.append("   Config has : "); 
        msg.append(UnitsText->text());
        msg.append("\n   Using Config value.");
        _errorMessage->setText(msg);
        _errorMessage->exec();
    }
    if (_addMode) UnitsText->insert(vDBUnits);

    //checkUnitsAndCalCoefs();

   return;
}

void AddA2DVariableComboDialog::checkUnitsAndCalCoefs()
{
   // Make sure that units and calibrations make sense
   if (UnitsText->text().size() != 0) {
      if (Calib1Text->text().size() == 0 ||
          Calib2Text->text().size() == 0) {
         if (UnitsText->text() != QString("V")) {
            QMessageBox * _errorMessage = new QMessageBox(this);
            QString msg("Do not have calibration coefficients:\n");
            msg.append("  Assigning slope:1 offset:0\n");
            msg.append("  Please determine correct values and update\n");
            _errorMessage->setText(msg);
            _errorMessage->exec();
            Calib1Text->setText("0");
            Calib2Text->setText("1");
         } else {
            Calib1Text->setText("0");
            Calib2Text->setText("1");
         }
      }
   }
   return;
}

void AddA2DVariableComboDialog::showSRErr(int vDBsr, int srIndx)
{
    QMessageBox * eMsg = new QMessageBox(this);
    QString msg("VarDB/Configuration missmatch: \n");
    msg.append("   VarDB Sample Rate  = "); msg.append(QString::number(vDBsr));
    msg.append("\n   Config Sample Rate = ");
    switch (srIndx) {
        case 0: 
            msg.append("10\n");
            break;
        case 1:
            msg.append("100\n");
            break;
        case 2:
            msg.append("500\n");
            break;
        default:
            // Can't happen...
            break;
    }
    msg.append("Defaulting to Configuration Value.");
    eMsg->setText(msg);
    eMsg->exec();

    return;
}

void AddA2DVariableComboDialog::showVoltErr(int32_t vDBvLow, int32_t vDBvHi, 
                                           int confIndx) 
{
    QString confRange;
    switch (confIndx) {
        case 0:
            confRange.append("0 - 5 Volts"); break;
        case 1:
            confRange.append("0 - 10 Volts"); break;
        case 2:
            confRange.append("-5 - 5 Volts"); break;
        case 3: 
            confRange.append("-10 - 10 Volts"); break;
        default:
            // should never happen
            break;
    }
    QMessageBox * _errorMessage = new QMessageBox(this);
    QString msg("VarDB/Configuration missmatch: \n");
    msg.append("   VarDB Volt Range: ");
    msg.append(QString::number(vDBvLow));
    msg.append(" - ");
    msg.append(QString::number(vDBvHi));
    msg.append("Volts\n   Config Volt Range: ");
    msg.append(confRange); msg.append("\n   Defaulting to Configuration value");
    _errorMessage->setText(msg);
    _errorMessage->exec();
    return;
}

QString AddA2DVariableComboDialog::removeSuffix(const QString & varName)
{
  QString result = varName;
  int sfxIdx;

  sfxIdx = varName.lastIndexOf("_");
  if (sfxIdx != -1)
    result.remove(sfxIdx, varName.size()-sfxIdx);

  return result;
}

QString AddA2DVariableComboDialog::getSuffix(const QString & varName)
{
  QString result = varName;
  int sfxIdx;

  sfxIdx = varName.lastIndexOf("_");
  if (sfxIdx != -1)
    result.remove(0, sfxIdx);
  else
    return QString();

  return result;
}

void AddA2DVariableComboDialog::clearForm()
{
   ChannelBox->clear();
   SuffixText->clear();
   LongNameText->clear();
   UnitsText->clear();
   CalLabel->setText("");
   Calib1Text->setText("");
   Calib2Text->setText("");
   Calib3Text->setText("");
   Calib4Text->setText("");
   Calib5Text->setText("");
   Calib6Text->setText("");
   if (!SRBox->isEnabled()) {  // previous edit had "bad" sample rate
     SRBox->removeItem(3); // the added sample rate
     SRBox->setEnabled(true);
   }
   return;
}

bool AddA2DVariableComboDialog::setup(std::string filename)
{
    if (!openVarDB(filename)) return false;

    buildA2DVarDB();

    return true;
}

bool AddA2DVariableComboDialog::openVarDB(std::string filename)
{

    // Get project directory from config filename that's passed in
    std::cerr<<"Filename = "<<filename<<"\n";
    std::string temp = filename;
    size_t found;
    found=temp.find_last_of("/\\");
    temp = temp.substr(0,found);
    found = temp.find_last_of("/\\");
    std::string curProjDir  = temp.substr(0,found);
    std::string SXmlVarDBFile = curProjDir+"/vardb.xml";
    std::cerr<<"************************\n "<<SXmlVarDBFile<<"\n";

    QMessageBox * _errorMessage = new QMessageBox(this);

    _vardb = new VDBFile(SXmlVarDBFile.c_str());
    if (_vardb->is_valid() == false)
    {
        _errorMessage->setText(QString::fromStdString
                 ("Could not initialize VarDB file: "
                  + SXmlVarDBFile + ".  Does it exist?"));
        _errorMessage->exec();
        return false;
    }

    return true;
}

bool AddA2DVariableComboDialog::fileExists(QString filename)
{
  struct stat buffer;
  if (::stat(filename.toStdString().c_str(), &buffer) == 0) return true;
  return false;
}

void AddA2DVariableComboDialog::buildA2DVarDB()
//  Construct the A2D Variable Drop Down list from analog VarDB elements
{
    QMessageBox * _errorMessage = new QMessageBox(this);

    disconnect(VariableBox, SIGNAL(currentIndexChanged(const QString &)),
               this, SLOT(dialogSetup(const QString &)));

    if (_vardb->is_valid() == false)
    {
        _errorMessage->setText(QString::fromStdString
                 (string("Could not access variables in VarDB xml file. ") +
                  string(" So could not create Variable dropdown list!")));
        _errorMessage->exec();
        return;
    }

    // Set up to allow user to create a new A2D variable if they need
    VariableBox->clear();
    VariableBox->addItem("New");

    VDBVar *vdbVar;
    QString varName;
    for (int i = 0; i <_vardb->num_vars(); ++i)
    {
        vdbVar = _vardb->get_var(i);
        bool analog = vdbVar->get_attribute_value<bool>(VDBVar::IS_ANALOG);
        if (analog) {
            varName = QString::fromStdString(vdbVar->name());
            VariableBox->addItem(varName);
        }
    }

   connect(VariableBox, SIGNAL(currentIndexChanged(const QString &)), this, 
              SLOT(dialogSetup(const QString &)));

    return;
}

void AddA2DVariableComboDialog::setCalLabels()
{
  Cal1Label->setText("C0");
  Cal2Label->setText("C1");
  Cal3Label->setText("C2");
  Cal4Label->setText("C3");
  Cal5Label->setText("C4");
  Cal6Label->setText("C5");
  return;
}

void AddA2DVariableComboDialog::clearCalLabels()
{
  Cal1Label->setText("");
  Cal2Label->setText("");
  Cal3Label->setText("");
  Cal4Label->setText("");
  Cal5Label->setText("");
  Cal6Label->setText("");
  return;
}

