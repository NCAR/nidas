#include "AddA2DVariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

#include <vector>

using namespace config;

QRegExp _calRegEx("^\\d*.?\\d*");
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
  VoltageBox->addItem(" -5 to  5 Volts");
  VoltageBox->addItem("  0 to 10 Volts");
  VoltageBox->addItem("-10 to 10 Volts");
  ChannelBox->addItem("0");
  ChannelBox->addItem("1");
  ChannelBox->addItem("2");
  ChannelBox->addItem("3");
  ChannelBox->addItem("4");
  ChannelBox->addItem("5");
  ChannelBox->addItem("6");
  ChannelBox->addItem("7");

  _errorMessage = new QMessageBox(this);
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
        _errorMessage->setText(QString::fromStdString("6th Order calibration needs values for 5th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib5Text->text().size()) 
      if (!Calib4Text->text().size() || !Calib3Text->text().size() || 
          !Calib2Text->text().size() || !Calib1Text->text().size()) {
        _errorMessage->setText(QString::fromStdString("5th Order calibration needs values for 4th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib4Text->text().size()) 
      if (!Calib3Text->text().size() || !Calib2Text->text().size() || 
          !Calib1Text->text().size()) {
        _errorMessage->setText(QString::fromStdString("4th Order calibration needs values for 3th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib3Text->text().size()) 
      if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
        _errorMessage->setText(QString::fromStdString("3th Order calibration needs values for 2nd and 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib2Text->text().size() || Calib1Text->text().size()) {
      if (!Calib2Text->text().size() || !Calib1Text->text().size()) {
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
   std::cerr << " Units: " + UnitsText->text().toStdString() + "\n";
   std::cerr << " Cals: " + Calib1Text->text().toStdString() + Calib2Text->text().toStdString() +
                  Calib3Text->text().toStdString() + Calib4Text->text().toStdString() +
                  Calib5Text->text().toStdString() + Calib6Text->text().toStdString() + "\n";

     try {
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
                                         UnitsText->text().toStdString(),
                                         cals);
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
     _errorMessage->setText("Unacceptable input in Variable name, units or calibration fields");
     _errorMessage->exec();
     std::cerr << "Unacceptable input in either Var name, units or cal fields\n";
  }

}

void AddA2DVariableComboDialog::show()
{
   this->QDialog::show();
}
