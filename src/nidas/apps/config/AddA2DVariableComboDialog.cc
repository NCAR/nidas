#include "AddA2DVariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

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
  Calib1Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  Calib2Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  Calib3Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  Calib4Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  Calib5Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  Calib6Text->setValidator( new QRegExpValidator ( _calRegEx, this));
  VariableText->setValidator( new QRegExpValidator ( _nameRegEx, this));
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
  SRBox->addItem("1");
  SRBox->addItem("10");
  SRBox->addItem("50");
  SRBox->addItem("100");
  SRBox->addItem("500");

}


void AddA2DVariableComboDialog::accept()
{
   std::cerr << "AddA2DVariableComboDialog::accept()\n";
  if (VariableText->hasAcceptableInput() && UnitsText->hasAcceptableInput() &&
      (!Calib1Text->text().size() || Calib1Text->hasAcceptableInput()) &&
      (!Calib2Text->text().size() || Calib2Text->hasAcceptableInput()) &&
      (!Calib3Text->text().size() || Calib3Text->hasAcceptableInput()) &&
      (!Calib4Text->text().size() || Calib4Text->hasAcceptableInput()) &&
      (!Calib5Text->text().size() || Calib5Text->hasAcceptableInput()) &&
      (!Calib6Text->text().size() || Calib6Text->hasAcceptableInput()) ) {

    // If we have a calibration, then we need a unit
    if (Calib1Text->text().size() && !UnitsText->text().size()) {
      QMessageBox * _errorMessage = new QMessageBox(this);
      _errorMessage->setText(QString::fromStdString("Must have units defined if a calibration is defined"));
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
        _errorMessage->setText(QString::fromStdString("6th Order calibration needs values for 5th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib5Text->text().size()) 
      if (!Calib4Text->text().size() || !Calib3Text->text().size() || 
          !Calib2Text->text().size() || !Calib1Text->text().size()) {
        QMessageBox * _errorMessage = new QMessageBox(this);
        _errorMessage->setText(QString::fromStdString("5th Order calibration needs values for 4th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib4Text->text().size()) 
      if (!Calib3Text->text().size() || !Calib2Text->text().size() || 
          !Calib1Text->text().size()) {
        QMessageBox * _errorMessage = new QMessageBox(this);
        _errorMessage->setText(QString::fromStdString("4th Order calibration needs values for 3th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib3Text->text().size()) 
      if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
        QMessageBox * _errorMessage = new QMessageBox(this);
        _errorMessage->setText(QString::fromStdString("3th Order calibration needs values for 2nd and 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib2Text->text().size() || Calib1Text->text().size()) {
      if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
        QMessageBox * _errorMessage = new QMessageBox(this);
        _errorMessage->setText(QString::fromStdString("Must have calibration values for at least the first two orders"));
        _errorMessage->exec();
        return;
      }
  }

    
   std::cerr << " Name: " + VariableText->text().toStdString() + "\n";
   std::cerr << " Long Name: " + LongNameText->text().toStdString() + "\n";
   std::cerr << "Volt Range Index: " << VoltageBox->currentIndex() << 
                " Val: " + VoltageBox->currentText().toStdString() +  "\n";
   std::cerr << " Channel Index: " << ChannelBox->currentIndex() <<
                " Val: " + ChannelBox->currentText().toStdString() + "\n";
   std::cerr << " SR Box Index: " << SRBox->currentIndex() <<
                " Val: " + SRBox->currentText().toStdString() + "\n";
   std::cerr << " Units: " + UnitsText->text().toStdString() + "\n";
   std::cerr << " Cals: " + Calib1Text->text().toStdString() + Calib2Text->text().toStdString() +
                  Calib3Text->text().toStdString() + Calib4Text->text().toStdString() +
                  Calib5Text->text().toStdString() + Calib6Text->text().toStdString() + "\n";

     try {
        // If we're in edit mode, we need to delete the A2DVariableItem from the model
        // first and then we can add it back in.
        if (_indexList.size() > 0)  {
          if(SRBox->currentIndex() !=_origSRBoxIndex) {
            QString msg("NOTE: changing the sample rate.");
            msg.append("For data acquisition you will need ");
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
        if (_document) _document->addA2DVariable(VariableText->text().toStdString(),
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
     _errorMessage->setText("Unacceptable input in Variable name, units or calibration fields");
     _errorMessage->exec();
     std::cerr << "Unacceptable input in either Var name, units or cal fields\n";
  }

}

void AddA2DVariableComboDialog::show(NidasModel* model, 
                                     QModelIndexList indexList)
{
  ChannelBox->clear();
  VariableText->clear();
  LongNameText->clear();
  UnitsText->clear();
  Calib1Text->clear();
  Calib2Text->clear();
  Calib3Text->clear();
  Calib4Text->clear();
  Calib5Text->clear();
  Calib6Text->clear();

  _model = model;
  _indexList = indexList;
  _origSRBoxIndex = -1;

  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the A2DVariableItem we are 
  // editing.
  NidasItem *item;
  if (indexList.size() > 0)  {
std::cerr<< "A2DVariableDialog called in edit mode\n";
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

    VariableText->insert(a2dVarItem->name());
    LongNameText->insert(a2dVarItem->getLongName());

    int gain = a2dVarItem->getGain();
    int bipolar = a2dVarItem->getBipolar();
    if (gain == 4 && bipolar == 0) VoltageBox->setCurrentIndex(0);
    if (gain == 2 && bipolar == 0) VoltageBox->setCurrentIndex(1);
    if (gain == 2 && bipolar == 1) VoltageBox->setCurrentIndex(2);
    if (gain == 1 && bipolar == 1) VoltageBox->setCurrentIndex(3);

    ChannelBox->addItem(QString::number(a2dVarItem->getA2DChannel()));
    float rate = a2dVarItem->getRate();
    if (rate == 1.0)   {
      SRBox->setCurrentIndex(0);
      _origSRBoxIndex = 0;
    }
    if (rate == 10.0)  {
      SRBox->setCurrentIndex(1);
      _origSRBoxIndex = 1;
    }
    if (rate == 50.0)  {
      SRBox->setCurrentIndex(2);
      _origSRBoxIndex = 2;
    }
    if (rate == 100.0) {
      SRBox->setCurrentIndex(3);
      _origSRBoxIndex = 3;
    }
    if (rate == 500.0) {
      SRBox->setCurrentIndex(4);
      _origSRBoxIndex = 4;
    }

    std::vector<std::string> calInfo = a2dVarItem->getCalibrationInfo();

    if (calInfo.size() > 0) {
      if (calInfo.size() > 7 || calInfo.size() < 3) 
        std::cerr << "Something wrong w/calibration info received from variable\n";
      else {
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
    }

  } else {
std::cerr<< "A2DVariableDialog called in add mode\n";
  }

  // Set up the Channel box by adding available a2d channels on this card.
  list<int> channels;
  if (_document) channels = _document->getAvailableA2DChannels();
  list<int>::iterator it;
  for (it=channels.begin(); it != channels.end(); it++)
    ChannelBox->addItem(QString::number(*it));

  this->QDialog::show();
}
