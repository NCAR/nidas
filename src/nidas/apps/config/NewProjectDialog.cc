#include "NewProjectDialog.h"

using namespace config;

QRegExp _projNameRegEx("[A-Z0-9\\-]*");

NewProjectDialog::NewProjectDialog(QWidget *parent): 
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

    fileName.append(_defaultDir);
    fileName.append("/");
    fileName.append(ProjName->text().toStdString().c_str());
    fileName.append("/");
    fileName.append(platform.c_str());
    fileName.append("/nidas/default.xml");

    _fileName = &fileName;
    //(ConfigWindow *) parent.setFilename(fileName);
    //_configWin->setFilename(fileName);

  }  else {
    _errorMessage->setText("Unacceptable input in Project Name");
    _errorMessage->exec();
    std::cerr << "Unaccptable input in Project Name\n";
    return;
  }

  _errorMessage->setText("The project has been created.  Now open the project configuration file and change the project name to the name you selected");
  _errorMessage->exec();

  QDialog::accept();
}


void NewProjectDialog::show(std::string defaultDir, QString* fileName)
{
  _defaultDir=QString::fromStdString(defaultDir);
  _fileName = fileName;

  if (setUpDialog())
    this->QDialog::show();
}

bool NewProjectDialog::setUpDialog()
{
  // Clear out all the fields
  ProjName->clear();

  return true;
}