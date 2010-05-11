#include "AddA2DVariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

using namespace config;

QRegExp _calRegEx("\\d*\\.\\d+");
QRegExp _nameRegEx("^\\S+$");

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
  if (VariableText->hasAcceptableInput() &&
      Calib1Text->hasAcceptableInput() &&
      Calib2Text->hasAcceptableInput() &&
      Calib3Text->hasAcceptableInput() &&
      Calib4Text->hasAcceptableInput() &&
      Calib5Text->hasAcceptableInput() &&
      Calib6Text->hasAcceptableInput() ) {

    // Now we need to validate that calibrations entered make sense
    if (Calib6Text->hasSelectedText()) 
      if (!Calib5Text->hasSelectedText() || !Calib4Text->hasSelectedText() ||
          !Calib3Text->hasSelectedText() || !Calib2Text->hasSelectedText() ||
          !Calib1Text->hasSelectedText()) 
      {
        _errorMessage->setText(QString::fromStdString("6th Order calibration needs values for 5th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib5Text->hasSelectedText()) 
      if (!Calib4Text->hasSelectedText() || !Calib3Text->hasSelectedText() || 
          !Calib2Text->hasSelectedText() || !Calib1Text->hasSelectedText()) {
        _errorMessage->setText(QString::fromStdString("5th Order calibration needs values for 4th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib4Text->hasSelectedText()) 
      if (!Calib3Text->hasSelectedText() || !Calib2Text->hasSelectedText() || 
          !Calib1Text->hasSelectedText()) {
        _errorMessage->setText(QString::fromStdString("4th Order calibration needs values for 3th thru 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib3Text->hasSelectedText()) 
      if (!Calib2Text->hasSelectedText() || !Calib1Text->hasSelectedText()) {
        _errorMessage->setText(QString::fromStdString("3th Order calibration needs values for 2nd and 1st orders"));
        _errorMessage->exec();
        return;
      }
    if (Calib2Text->hasSelectedText() || Calib1Text->hasSelectedText()) 
      if (!Calib2Text->hasSelectedText() || !Calib1Text->hasSelectedText()) {
        _errorMessage->setText(QString::fromStdString("Must have calibration values for at least the first two orders"));
        _errorMessage->exec();
        return;
      }
    
   std::cerr << "AddA2DVariableComboDialog::accept()\n";
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

/*
     try {
        if (_document) _document->addA2DVarible(A2DVaribleBox->currentText().toStdString(),
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
*/

     QDialog::accept(); // accept (or bail out) and make the dialog disappear

  }
/*
  }  else {
     _errorMessage->setText("Unacceptable input in Variable name or calibration fields");
     _errorMessage->exec();
     std::cerr << "Unaccptable input in either Var name or cal fields\n";
  }
*/

}

void AddA2DVariableComboDialog::show()
{
   this->QDialog::show();
}
