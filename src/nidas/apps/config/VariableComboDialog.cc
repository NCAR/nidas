/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

  SRText->setToolTip("The name of the Variable - use CAPS");
  setAttribute(Qt::WA_AlwaysShowToolTips);
  _errorMessage = new QMessageBox(this);
  _xmlCals = false;
}


void VariableComboDialog::accept()
{
  std::cerr << "VariableComboDialog::accept()\n";

  // Should only be called in edit mode (non a2d variables come from sensor
  //  catalog - i.e. cannot create new ones.)
  if (_indexList.size() <= 0)  
    throw InternalProcessingException("Don't have a variable we're editing - should not happen");

  if (VariableText->hasAcceptableInput() && UnitsText->hasAcceptableInput()) {

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

   bool useCalfile = false;
   if (calFileCheckBox->checkState() == Qt::Checked) useCalfile=true;
    
   std::cerr << " Name: " << VariableText->text().toStdString() << "\n";
   std::cerr << " Long Name: " << LongNameText->text().toStdString() << "\n";
   std::cerr << " Sample Rate: " << SRText->text().toStdString() << "\n";
   std::cerr << " Units: " << UnitsText->text().toStdString() << "\n";
   std::string uCf = "No";
   if (useCalfile) uCf = "Yes";
   std::cerr << " UseCalFile: " + uCf + "\n";
   if (_xmlCals) {
      std::cerr << " XML Cals: " << Calib1Text->text().toStdString() 
                                 << Calib2Text->text().toStdString() 
                                 << Calib3Text->text().toStdString() 
                                 << Calib4Text->text().toStdString() 
                                 << Calib5Text->text().toStdString() 
                                 << Calib6Text->text().toStdString() << "\n";
   }

     try {

        // Check for unusual situation of move from xml calibration to
        // cal file - make sure that's what user wants.
        if (useCalfile && _xmlCals) {
           QMessageBox msgBox;
           int ret = 0;
           QString msg("You are changing the source of calibration\n");
           msg.append("information from the XML to a calinbration file.\n");
           msg.append("This will remove the XML defined calibrations.\n");
           msg.append("Is this your intent?\n");
           msgBox.setText(msg);
           msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
           msgBox.setDefaultButton(QMessageBox::Yes);
           ret = msgBox.exec();
           if (ret == QMessageBox::Cancel) return;
        }

        vector <std::string> cals;
        if (!useCalfile && _xmlCals) {
           cals.push_back(Calib1Text->text().toStdString());
           cals.push_back(Calib2Text->text().toStdString());
           cals.push_back(Calib3Text->text().toStdString());
           cals.push_back(Calib4Text->text().toStdString());
           cals.push_back(Calib5Text->text().toStdString());
           cals.push_back(Calib6Text->text().toStdString());
        }

        if (_document) {
           _document->updateVariable( _varItem,
                                      VariableText->text().toStdString(),
                                      LongNameText->text().toStdString(),
                                      SRText->text().toStdString(),
                                      UnitsText->text().toStdString(),
                                      cals, useCalfile);
           _document->setIsChanged(true);
        }
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
     _errorMessage->setText("Unacceptable input in Variable name or units");
     _errorMessage->exec();
     std::cerr << "Unacceptable input in either Var name or units\n";
  }

}

void VariableComboDialog::show(NidasModel* model, 
                                     QModelIndexList indexList)
{
  VariableText->clear();
  LongNameText->clear();
  SRText->clear();
  UnitsText->clear();
  Calib1Text->clear();
  Calib2Text->clear();
  Calib3Text->clear();
  Calib4Text->clear();
  Calib5Text->clear();
  Calib6Text->clear();
  calFileCheckBox->setCheckState(Qt::Unchecked);


  _model = model;
  _indexList = indexList;

  // Interface is that if indexList is null then we are in "add" modality and
  // if it is not, then it contains the index to the VariableItem we are 
  // editing.
  NidasItem *item = NULL;
  if (indexList.size() <= 0)  
    throw InternalProcessingException("Don't have a variable we're editing - should not happen");
  for (int i=0; i<indexList.size(); i++) {
    QModelIndex index = indexList[i];
    // the NidasItem for the selected row resides in column 0
    if (index.column() != 0) continue;
    if (!index.isValid()) continue; // XXX where/how to destroy the rootItem (Project)
    item = model->getItem(index);
  }

  setCalLabels();

  _varItem = dynamic_cast<VariableItem*>(item);
  if (!_varItem)
    throw InternalProcessingException("Selection is not a Variable.");

  VariableText->insert(QString::fromStdString(_varItem->getBaseName()));
  LongNameText->insert(_varItem->getLongName());

  float rate = _varItem->getRate();
cerr<<"  Get Rate returns: " << rate << "\n";
  SRText->insert(QString::number(rate));
  SRText->setEnabled(false);

  QString calSrc = _varItem->getCalSrc();
  CalLabel->setText(QString("CalSrc:")+calSrc);
  if (calSrc == "N/A") {
    UnitsText->setText(
         QString::fromStdString(_varItem->getVariable()->getUnits()));
    Calib1Text->setText(QString("0"));
    Calib2Text->setText(QString("1"));
  } else {

    if (calSrc != "XML") calFileCheckBox->setCheckState(Qt::Checked);
    else {
      _xmlCals = true;
      calFileCheckBox->setCheckState(Qt::Unchecked);
    }
    std::vector<std::string> calInfo = _varItem->getCalibrationInfo();

    std::cerr<<__func__<<" Members of calInfo vector are:\n";
      for (std::vector<std::string>::iterator it = calInfo.begin(); 
            it != calInfo.end(); it++) {
        std::cerr<<*it<<" ";
      }
    std::cerr<<"\n";
  
    if (calInfo.size() > 0) {
        if (calInfo.size() == 1 || 
           (calInfo.size() == 2 && calInfo[0] == "ERROR")) {
          Calib1Text->setText(QString("0"));
          Calib2Text->setText(QString("1"));
          Calib3Text->setText(QString("Missing File"));
          UnitsText->setText(QString::fromStdString(calInfo.back()));

        } else if (calInfo.size() > 6 || calInfo.size() < 3) {
          std::cerr << "Unexpected # of cal info items from VarItem\n";
        } else {
          if (calInfo.back().size() == 0)
            UnitsText->insert(QString("V"));
          else
            UnitsText->insert(QString::fromStdString(calInfo.back()));
          calInfo.pop_back();

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
                  break;

          default: std::cerr << "No Cals";

        }
      }
    } else {
      // If there is no calibration info then we're just measuring Volts
      CalLabel->setText(QString("No Calibrations Found"));
      UnitsText->insert(QString("V"));
      Calib1Text->setText(QString("0"));
      Calib2Text->setText(QString("1"));
    }
  }

  VariableText->setFocus(Qt::ActiveWindowFocusReason);

  this->QDialog::show();
}

void VariableComboDialog::setCalLabels()
{
  Cal1Label->setText("C0");
  Cal2Label->setText("C1");
  Cal3Label->setText("C2");
  Cal4Label->setText("C3");
  Cal5Label->setText("C4");
  Cal6Label->setText("C5");
  return;
}
