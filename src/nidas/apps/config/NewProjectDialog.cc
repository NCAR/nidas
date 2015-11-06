/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
#include "NewProjectDialog.h"
#include "configwindow.h"
#include <QDir>

using namespace config;

QRegExp _projNameRegEx("[A-Z0-9\\-]*");

NewProjectDialog::NewProjectDialog(QString projDir, QWidget *parent): 
    QDialog(parent)
{
  setupUi(this);
  ProjName->setValidator( new QRegExpValidator(_projNameRegEx, this));
  PlatformComboBox->clear();
  PlatformComboBox->addItem("NCAR C130");
  PlatformComboBox->addItem("NCAR GV");
  PlatformComboBox->addItem("NRL P3");
  PlatformComboBox->addItem("Lab System");
  _errorMessage = new QMessageBox(this);
  _confWin = dynamic_cast<ConfigWindow*>(parent);
  if (!_confWin) std::cerr<<"Parent is not a configWindow?\n";
  _defaultDir = projDir;
}


void NewProjectDialog::accept()
{
  if (ProjName->hasAcceptableInput()) {
    std::cerr << "NewProjectDialog::accept()\n";
    std::cerr << " ProjName: " + ProjName->text().toStdString() + "<EOS>\n";
    std::cerr << " Platform: " + 
	PlatformComboBox->currentText().toStdString() + "<EOS>\n";

    // We need a platform string
    std::string platform;
    switch (PlatformComboBox->currentIndex()) {

      case 0:
        platform.append("C130_N130AR");
        break;

      case 1:
        platform.append("GV_N677F");
        break;

      case 2:
        platform.append("P3_NRL-P3");
        break;

      case 3:
        platform.append("Lab_N600");
        break;
    }

    // Verify that the project directory does not already exist
    QString fullProjDir;
    fullProjDir.append(_defaultDir);
    fullProjDir.append("/");
    fullProjDir.append(ProjName->text());
    QDir newDir(fullProjDir);
    if (newDir.exists()) {
      QMessageBox msgBox;
      msgBox.setText("The Project Directory:"+fullProjDir+" already exists.");
      msgBox.exec();
      return;
    } 

    // Create and execute the init_project command
    char cmd[1024];  
    strcpy(cmd, _defaultDir.toStdString().c_str());
    strcat(cmd, "/Configuration/raf/init_project ");
    strcat(cmd, ProjName->text().toStdString().c_str());
    strcat(cmd, " ");
    strcat(cmd, platform.c_str());
    std::cerr << "Calling init_project with command: " << cmd << "\n";
    if (system(cmd) <= 0) {
      std::cerr << " ERROR!:  Call to init_project failed!\n";
    }
 
    // get filename back to configwindow
    QString fileName;

    //fileName.append(_defaultDir);
    //fileName.append("/");
    //fileName.append(ProjName->text().toStdString().c_str());
    fileName.append(fullProjDir);
    fileName.append("/");
    fileName.append(platform.c_str());
    fileName.append("/nidas/default.xml");

    _confWin->setFilename(fileName);
    _confWin->openFile();
    _confWin->writeProjectName(ProjName->text());

  }  else {
    _errorMessage->setText("Unacceptable input in Project Name");
    _errorMessage->exec();
    std::cerr << "Unaccptable input in Project Name\n";
    return;
  }

  //_errorMessage->setText("The project has been created.  Now open the project configuration file and change the project name to the name you selected");
  //_errorMessage->exec();

  QDialog::accept();
}


void NewProjectDialog::show()
{
  if (setUpDialog())
    this->QDialog::show();
}

bool NewProjectDialog::setUpDialog()
{
  // Clear out all the fields
  ProjName->clear();

  return true;
}
