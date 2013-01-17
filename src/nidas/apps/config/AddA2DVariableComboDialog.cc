#include "AddA2DVariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

#include <dirent.h>
#include <raf/vardb.h>  // Variable DataBase
#include <arpa/inet.h>


#include <vector>

using namespace config;

QRegExp _calRegEx("^-?\\d*.?\\d*");
QRegExp _nameRegEx("^[A-Z|0-9|_]*$");
QRegExp _unitRegEx("^\\S*$");
//QRegExp _nameRegEx("^\\S+$");

AddA2DVariableComboDialog::AddA2DVariableComboDialog(QString engCalDirRoot,
                                                     QWidget *parent): 
    QDialog(parent)
{
   _engCalDirRoot = engCalDirRoot;

   setupUi(this);

   Calib1Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   Calib2Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   Calib3Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   Calib4Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   Calib5Text->setValidator( new QRegExpValidator ( _calRegEx, this));
   Calib6Text->setValidator( new QRegExpValidator ( _calRegEx, this));
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
}

void AddA2DVariableComboDialog::setDocument(Document* document)
{
   _document = document;

   vector <std::string> siteNames;
   siteNames=_document->getSiteNames();
   _engCalDir = _engCalDirRoot + QString::fromStdString(siteNames[0])
                + QString::fromStdString("/");

   cerr<<"Engineering cal dir = ";
   cerr<<_engCalDir.toStdString();
   cerr<<"\n";

   DIR *dir;
   char *temp_dir = (char*) malloc(_engCalDir.size()+1);

   strcpy(temp_dir, _engCalDir.toStdString().c_str());

   if ((dir = opendir(temp_dir)) == 0) {
      QMessageBox * errorMessage = new QMessageBox(this);
      errorMessage->setText("Could not open Engineering calibrations dir:" +
                              _engCalDir +
                            "\n Can't check on existence of Cal files and " +
                            "\n this may imply processing problems later");
      errorMessage->exec();
      free(temp_dir);
   }
   return;
}

void AddA2DVariableComboDialog::accept()
{
   checkUnitsAndCalCoefs();

   std::cerr << "AddA2DVariableComboDialog::accept()\n";
   if ((!Calib1Text->text().size() || Calib1Text->hasAcceptableInput()) &&
       (!Calib2Text->text().size() || Calib2Text->hasAcceptableInput()) &&
       (!Calib3Text->text().size() || Calib3Text->hasAcceptableInput()) &&
       (!Calib4Text->text().size() || Calib4Text->hasAcceptableInput()) &&
       (!Calib5Text->text().size() || Calib5Text->hasAcceptableInput()) &&
       (!Calib6Text->text().size() || Calib6Text->hasAcceptableInput()) ) {

      // If we have a calibration, then we need a unit
      if (Calib1Text->text().size() && !UnitsText->text().size()) {
         QMessageBox * _errorMessage = new QMessageBox(this);
         _errorMessage->setText(QString::fromStdString(
              "Must have units defined if a calibration is defined"));
         _errorMessage->exec();
         return;
      }

      // Now we need to validate that calibrations entered make sense
      if (Calib6Text->text().size()) 
         if (!Calib5Text->text().size() || !Calib4Text->text().size() ||
             !Calib3Text->text().size() || !Calib2Text->text().size() ||
             !Calib1Text->text().size()) 
      {
         QMessageBox * _errorMessage = new QMessageBox(this);
         _errorMessage->setText(QString::fromStdString(
              "6th Order calibration needs values for 5th thru 1st orders"));
         _errorMessage->exec();
         return;
      }

      if (Calib5Text->text().size()) 
         if (!Calib4Text->text().size() || !Calib3Text->text().size() || 
             !Calib2Text->text().size() || !Calib1Text->text().size()) {
            QMessageBox * _errorMessage = new QMessageBox(this);
            _errorMessage->setText(QString::fromStdString(
              "5th Order calibration needs values for 4th thru 1st orders"));
            _errorMessage->exec();
            return;
         }

      if (Calib4Text->text().size()) 
         if (!Calib3Text->text().size() || !Calib2Text->text().size() || 
             !Calib1Text->text().size()) {
            QMessageBox * _errorMessage = new QMessageBox(this);
            _errorMessage->setText(QString::fromStdString(
              "4th Order calibration needs values for 3th thru 1st orders"));
            _errorMessage->exec();
            return;
         }

      if (Calib3Text->text().size()) 
         if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
            QMessageBox * _errorMessage = new QMessageBox(this);
            _errorMessage->setText(QString::fromStdString(
              "3th Order calibration needs values for 2nd and 1st orders"));
            _errorMessage->exec();
            return;
         }

      if (Calib2Text->text().size() || Calib1Text->text().size()) {
         if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
            QMessageBox * _errorMessage = new QMessageBox(this);
            _errorMessage->setText(QString::fromStdString(
             "Must have calibration values for at least the first two orders"));
            _errorMessage->exec();
            return;
         }
      }

      // Make sure we have exactly one "_" at the beginning of the suffix
      QString suffixText=SuffixText->text();
      if (suffixText.length() > 0) {
         suffixText.replace("_", "");
         suffixText.prepend("_");
         SuffixText->clear();
         SuffixText->insert(suffixText);
      }
       
      std::cerr << " Name: " + VariableBox->currentText().toStdString() + 
                     SuffixText->text().toStdString() + "\n";
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
         if (_indexList.size() > 0)  {
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
         if (_document) 
            _document->addA2DVariable(VariableBox->currentText().toStdString() +
                                       SuffixText->text().toStdString(),
                                       LongNameText->text().toStdString(),
                                       VoltageBox->currentText().toStdString(),
                                       ChannelBox->currentText().toStdString(),
                                       SRBox->currentText().toStdString(),
                                       UnitsText->text().toStdString(),
                                       cals);
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

   }  else {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(
          "Unacceptable input in Variable name, units or calibration fields");
      _errorMessage->exec();
      std::cerr << 
          "Unacceptable input in either Var name, units or cal fields\n";
   }

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

    int index = VariableBox->findText(removeSuffix(a2dVarItem->name()));
    if (index == -1) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      QString msg("Variable:");
      msg.append(removeSuffix(a2dVarItem->name()));
      msg.append(" does not appear as an A2D variable in VarDB.\n");
      msg.append(" Adding to list to allow for editing here.\n");
      msg.append(" Recommend correcting in VarDB.");
      _errorMessage->setText(msg);
      _errorMessage->exec();

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

    if (calInfo.size() > 0) {
      if (calInfo.size() > 7 || calInfo.size() < 3) 
        std::cerr << "Something wrong w/calibration info received from variable\n";
      else {
        if (calInfo.back().size() == 0)
           UnitsText->insert(QString("V"));
        else
           UnitsText->insert(QString::fromStdString(calInfo.back()));
        calInfo.pop_back();
        switch (calInfo.size()) {
          case 6: Calib6Text->insert(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 5: Calib5Text->insert(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 4: Calib4Text->insert(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 3: Calib3Text->insert(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
          case 2: Calib2Text->insert(QString::fromStdString(calInfo.back()));
                  calInfo.pop_back();
                  Calib1Text->insert(QString::fromStdString(calInfo.back()));
        }
      }
    } else {
        // If there is no calibration info then we're just measuring Volts
        UnitsText->insert(QString("V"));
        Calib1Text->insert(QString("0"));
        Calib2Text->insert(QString("1"));
    }

    // Since change of index will trigger dialogSetup we need to 
    // do this step last
    if (index != -1) VariableBox->setCurrentIndex(index);
    VariableBox->setEnabled(false);
    SuffixText->insert(getSuffix(a2dVarItem->name()));

  } else {
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
   int32_t idx = getVarDBIndex(variable);

   if (idx != ERR) {
       // Fill in the form according to VarDB lookup info
       // Notify the user when VarDB and configuration don't agree
       //   default to configuration value - assume user wanted specificity
       QString vDBTitle(((struct var_v2 *)VarDB)[idx].Title);
cerr<<"   - addMode: "<<_addMode<<"vDBTitle: "<<vDBTitle.toStdString().c_str()
    <<"  LongName: " <<LongNameText->text().toStdString().c_str()<<"\n";
       if (!_addMode && LongNameText->text() != vDBTitle) {
           QMessageBox * _errorMessage = new QMessageBox(this);
           QString msg("VarDB/Configuration missmatch: \n");
           msg.append("   VarDB Title: "); msg.append(vDBTitle); 
           msg.append("\n"); msg.append("   Config has : "); 
           msg.append(LongNameText->text());
           msg.append("\n   Using Config value.");
           _errorMessage->setText(msg);
           _errorMessage->exec();
       }
        if (_addMode) LongNameText->insert(vDBTitle);

        int32_t vLow = ntohl(((struct var_v2 *)VarDB)[idx].voltageRange[0]);
        int32_t vHigh = ntohl(((struct var_v2 *)VarDB)[idx].voltageRange[1]);
cerr<<"    - VarDB lookup vLow:"<<vLow<<"  vHight:"<<vHigh<<"\n";
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
            QMessageBox * _errorMessage = new QMessageBox(this);
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
    
       int32_t sRate = ntohl(((struct var_v2 *)VarDB)[idx].defaultSampleRate);
cerr<<"    - VarDB lookup sRate:"<<sRate<<"\n";
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
               QMessageBox * _errorMessage = new QMessageBox(this);
               QString msg("VarDB error: Default Sample Rate: ");
               msg.append(QString::number(sRate));
               msg.append(" is nonstandard - run vared to fix.");
               msg.append(" Defaulting to 10 SPS.");
               _errorMessage->setText(msg);
               _errorMessage->exec();
               SRBox->setCurrentIndex(0);
       }

cerr<<"    -VarDB lookup Units:"<<((struct var_v2 *)VarDB)[idx].Units<<"\n";
       QString vDBUnits(((struct var_v2 *)VarDB)[idx].Units);
       if (!_addMode && UnitsText->text() != vDBUnits) {
           QMessageBox * _errorMessage = new QMessageBox(this);
           QString msg("VarDB/Configuration missmatch: \n");
           msg.append("   VarDB Units: "); msg.append(vDBUnits); 
           msg.append("\n"); msg.append("   Config has : "); 
           msg.append(UnitsText->text());
           msg.append("\n   Using Config value.");
           _errorMessage->setText(msg);
           _errorMessage->exec();
       }
       if (_addMode) UnitsText->insert(vDBUnits);

       checkUnitsAndCalCoefs();

   }    

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
            Calib1Text->insert("0");
            Calib2Text->insert("1");
         } else {
            Calib1Text->insert("0");
            Calib2Text->insert("1");
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
    msg.append("Defaulting to VarDB Value.");
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
    msg.append(confRange); msg.append("\n   Defaulting to VarDB value");
    _errorMessage->setText(msg);
    _errorMessage->exec();
    return;
}

int AddA2DVariableComboDialog::getVarDBIndex(const QString & varName)
{

    int indx = VarDB_lookup(varName.toStdString().c_str());
    if (indx == ERR) 
    {
      QMessageBox * _errorMessage = new QMessageBox(this);
      QString msg("Variable name:");
      msg.append(varName);
      msg.append(" is not found in VarDB");
      _errorMessage->setText(msg);
      _errorMessage->exec();
    } else cerr << "VarDB index for "
                << varName.toStdString().c_str() << " is:"<<indx<<"\n";

    return indx;

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
   Calib1Text->clear();
   Calib2Text->clear();
   Calib3Text->clear();
   Calib4Text->clear();
   Calib5Text->clear();
   Calib6Text->clear();
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

    extern long VarDB_nRecords;
    std::cerr<<"Filename = "<<filename<<"\n";
    std::string temp = filename;
    size_t found;
    found=temp.find_last_of("/\\");
    temp = temp.substr(0,found);
    found = temp.find_last_of("/\\");
    std::string curProjDir  = temp.substr(0,found);
    std::string varDBfile=curProjDir + "/VarDB";
    QString QsNcVarDBFile(QString::fromStdString(varDBfile+".nc"));
    QMessageBox * _errorMessage = new QMessageBox(this);

    if (fileExists(QsNcVarDBFile)) {
        cerr << "Removing VarDB.nc \n";
        int i = unlink(QsNcVarDBFile.toStdString().c_str());
        if (i == -1 && errno != ENOENT) throw InternalProcessingException("Unable to remove VarDB.nc file!");
    }

    if (InitializeVarDB(varDBfile.c_str()) == ERR)
    {
        _errorMessage->setText(QString::fromStdString
                 ("Could not initialize VarDB file: "
                  + varDBfile + ".  Does it exist?"));
        _errorMessage->exec();
        return false;
    }

    if (VarDB_isNcML() == true)
    {
        QString msg("Configuration Editor needs the non-netCDF VARDB.");
        msg.append("We could not delete the netCDF version.");
        _errorMessage->setText(msg);
        _errorMessage->exec();
        return false;
    }

    SortVarDB();

    std::cerr<<"*******************  nrecs = "<<VarDB_nRecords<<"\n";
    return true;
}

bool AddA2DVariableComboDialog::fileExists(QString filename)
{
  struct stat buffer;
  if (stat(filename.toStdString().c_str(), &buffer) == 0) return true;
  return false;
}

void AddA2DVariableComboDialog::buildA2DVarDB()
//  Construct the A2D Variable Drop Down list from analog VarDB elements
{
    extern long VarDB_nRecords;

    disconnect(VariableBox, SIGNAL(currentIndexChanged(const QString &)),
               this, SLOT(dialogSetup(const QString &)));

    cerr<<__func__<<": Putting together A2D Variable list\n";
    cerr<< "    - number of vardb records = " << VarDB_nRecords << "\n";
    map<string,xercesc::DOMElement*>::const_iterator mi;

    VariableBox->clear();
    VariableBox->addItem("New");

    for (int i = 0; i < VarDB_nRecords; ++i)
    {
        if ((((struct var_v2 *)VarDB)[i].is_analog) != 0) {
            QString temp(((struct var_v2 *)VarDB)[i].Name);
            VariableBox->addItem(temp);
        }
    }

   connect(VariableBox, SIGNAL(currentIndexChanged(const QString &)), this, 
              SLOT(dialogSetup(const QString &)));

    return;
}

