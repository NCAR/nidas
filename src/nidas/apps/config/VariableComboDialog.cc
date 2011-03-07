#include "VariableComboDialog.h"
#include "configwindow.h"
#include "exceptions/InternalProcessingException.h"
#include <nidas/util/InvalidParameterException.h>
#include "DeviceValidator.h"

#include <vector>

using namespace config;

QRegExp _calibRegEx("^-?\\d*.?\\d*");
QRegExp _varnameRegEx("^[A-Z|0-9|_]*$");
QRegExp _varunitRegEx("^\\S*$");

VariableComboDialog::VariableComboDialog(QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
//  Calib1Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
//  Calib2Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
//  Calib3Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
//  Calib4Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
//  Calib5Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
//  Calib6Text->setValidator( new QRegExpValidator ( _calibRegEx, this));
  VariableText->setValidator( new QRegExpValidator ( _varnameRegEx, this));
  UnitsText->setValidator( new QRegExpValidator ( _varunitRegEx, this));
  SRBox->addItem("1");
  SRBox->addItem("10");
  SRBox->addItem("50");
  SRBox->addItem("100");
  SRBox->addItem("500");

  _errorMessage = new QMessageBox(this);
}


void VariableComboDialog::accept()
{
  std::cerr << "VariableComboDialog::accept()\n";

  // Should only be called in edit mode (non a2d variables come from sensor
  //  catalog - i.e. cannot create new ones.)
  if (_indexList.size() <= 0)  
    throw InternalProcessingException("Don't have a variable we're editing - should not happen");

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
   std::cerr << " SR Box Index: " << SRBox->currentIndex() <<
                " Val: " + SRBox->currentText().toStdString() + "\n";
   std::cerr << " Units: " + UnitsText->text().toStdString() + "\n";
   std::cerr << " Cals: " + Calib1Text->text().toStdString() + Calib2Text->text().toStdString() +
                  Calib3Text->text().toStdString() + Calib4Text->text().toStdString() +
                  Calib5Text->text().toStdString() + Calib6Text->text().toStdString() + "\n";

     try {

        if(SRBox->currentIndex() !=_origSRBoxIndex) {
          _errorMessage->setText(QString::fromStdString("NOTE: changing the sample rate. This will impact all variables in the sample and is generally unadvised - are you sure you want to do this?"));
          _errorMessage->exec();
        }
     
        vector <std::string> cals;
        cals.push_back(Calib1Text->text().toStdString());
        cals.push_back(Calib2Text->text().toStdString());
        cals.push_back(Calib3Text->text().toStdString());
        cals.push_back(Calib4Text->text().toStdString());
        cals.push_back(Calib5Text->text().toStdString());
        cals.push_back(Calib6Text->text().toStdString());
        if (_document) _document->updateVariable(
                                         _varItem,
                                         VariableText->text().toStdString(),
                                         LongNameText->text().toStdString(),
                                         SRBox->currentText().toStdString(),
                                         UnitsText->text().toStdString(),
                                         cals);
     } catch ( InternalProcessingException &e) {
        _errorMessage->setText(QString::fromStdString
                              ("Bad internal error. Get help! " + e.toString()));
        _errorMessage->exec();
     } catch ( nidas::util::InvalidParameterException &e) {
        _errorMessage->setText(QString::fromStdString("Invalid parameter: " + 
                               e.toString()));
        _errorMessage->exec();
        return; // do not accept, keep dialog up for further editing
     } catch (...) { 
       _errorMessage->setText("Caught Unspecified error"); 
       _errorMessage->exec(); 
     }

     QDialog::accept(); // accept (or bail out) and make the dialog disappear

  }  else {
     _errorMessage->setText("Unacceptable input in Variable name, units or calibration fields");
     _errorMessage->exec();
     std::cerr << "Unacceptable input in either Var name, units or cal fields\n";
  }

}

void VariableComboDialog::show(NidasModel* model, 
                                     QModelIndexList indexList)
{
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
  // if it is not, then it contains the index to the VariableItem we are 
  // editing.
  NidasItem *item;
  if (indexList.size() <= 0)  
    throw InternalProcessingException("Don't have a variable we're editing - should not happen");
  for (int i=0; i<indexList.size(); i++) {
    QModelIndex index = indexList[i];
    // the NidasItem for the selected row resides in column 0
    if (index.column() != 0) continue;
    if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
    item = model->getItem(index);
  }

  _varItem = dynamic_cast<VariableItem*>(item);
  if (!_varItem)
    throw InternalProcessingException("Selection is not an A2DVariable.");

  //VariableText->insert(_varItem->name());
  VariableText->insert(QString::fromStdString(_varItem->getBaseName()));
  LongNameText->insert(_varItem->getLongName());

  float rate = _varItem->getRate();
cerr<<"  Get Rate returns: " << rate << "\n";
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

  std::vector<std::string> calInfo = _varItem->getCalibrationInfo();

  if (calInfo.size() > 0) {
    if (calInfo.size() > 7 || calInfo.size() < 2) 
      std::cerr << "Something wrong w/calibration info received from variable\n";
    else {
      UnitsText->insert(QString::fromStdString(calInfo.back()));
      calInfo.pop_back();
      switch (calInfo.size()) {
        case 6: Calib6Text->insert(QString::fromStdString(calInfo.back()));
std::cerr<<"6th calcoef = "<<calInfo.back()<<"\n";
                calInfo.pop_back();
        case 5: Calib5Text->insert(QString::fromStdString(calInfo.back()));
std::cerr<<"5th calcoef = "<<calInfo.back()<<"\n";
                calInfo.pop_back();
        case 4: Calib4Text->insert(QString::fromStdString(calInfo.back()));
std::cerr<<"4th calcoef = "<<calInfo.back()<<"\n";
                calInfo.pop_back();
        case 3: Calib3Text->insert(QString::fromStdString(calInfo.back()));
std::cerr<<"3th calcoef = "<<calInfo.back()<<"\n";
                calInfo.pop_back();
        case 2: Calib2Text->insert(QString::fromStdString(calInfo.back()));
std::cerr<<"2th calcoef = "<<calInfo.back()<<"\n";
                calInfo.pop_back();
std::cerr<<"1st calcoef = "<<calInfo.back()<<"\n";
                Calib1Text->insert(QString::fromStdString(calInfo.back()));
      }
    }
  }

  this->QDialog::show();
}
