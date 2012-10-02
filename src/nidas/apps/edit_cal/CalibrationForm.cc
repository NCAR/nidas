#include "CalibrationForm.h"
#include "PolyEval.h"
#include "calTableHeaders.h"

#include <iostream>

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRelationalDelegate>

//#include "logx/Logging.h"

//LOGGING("CalibrationForm");

/* -------------------------------------------------------------------- */

CalibrationForm::CalibrationForm(QWidget* parent) : QWidget(parent)
{
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  setupUi(this);

  connect(_commentSel, SIGNAL(activated(int)), this, SLOT(commentSelected(int)));

    _tableWidget->setRowCount(20);

    for (int r=0; r<20; r++) {
        _setPointList.append(new QLineEdit);
        _delButtonList.append(new QPushButton);
        _newVList.append(new QLineEdit);
        _new_sdList.append(new QLineEdit);
        _appliedList.append(new QLineEdit);
        _setDateTimeList.append(new QLineEdit);

        _setPointList[r]->setReadOnly(true);
        _delButtonList[r]->setText("del");
        _newVList[r]->setReadOnly(true);
        _new_sdList[r]->setReadOnly(true);
        _appliedList[r]->setReadOnly(true);
        _setDateTimeList[r]->setReadOnly(true);

        _setPointList[r]->setMaximumWidth(80);
        _delButtonList[r]->setMaximumWidth(40);
        _newVList[r]->setMaximumWidth(90);
        _new_sdList[r]->setMaximumWidth(90);
        _appliedList[r]->setMaximumWidth(100);

        _tableWidget->setCellWidget(r, 0, _setPointList[r] );
        _tableWidget->setCellWidget(r, 1, _delButtonList[r] );
        _tableWidget->setCellWidget(r, 2, _newVList[r] );
        _tableWidget->setCellWidget(r, 3, _new_sdList[r] );
        _tableWidget->setCellWidget(r, 4, _appliedList[r] );
        _tableWidget->setCellWidget(r, 5, _setDateTimeList[r] );
    }
    _tableWidget->resizeColumnsToContents();
    _tableWidget->horizontalHeader()->setStretchLastSection( true ); 

    _currCalCList.append(_currCalC0);
    _currCalCList.append(_currCalC1);
    _currCalCList.append(_currCalC2);
    _currCalCList.append(_currCalC3);

    _prevCalCList.append(_prevCalC0);
    _prevCalCList.append(_prevCalC1);
    _prevCalCList.append(_prevCalC2);
    _prevCalCList.append(_prevCalC3);

  _delButtonGroup = new QButtonGroup(this);
  _delButtonGroup->setObjectName(QString::fromUtf8("_delButtonGroup"));
  _delButtonGroup->setExclusive(false);
  for (int r=0; r<20; r++)
    _delButtonGroup->addButton(_delButtonList[r], r);

  connect(_delButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(delThisSetPoint(int)));

  _curveFitGroup = new QButtonGroup(this);
  _curveFitGroup->setObjectName(QString::fromUtf8("_curveFitGroup"));
  _curveFitGroup->addButton(_linearRB,   2);
  _curveFitGroup->addButton(_2ndOrderRB, 3);
  _curveFitGroup->addButton(_3rdOrderRB, 4);
  connect(_curveFitGroup, SIGNAL(buttonClicked(int)), this, SLOT(changeFitButtonClicked(int)));
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setModel(QAbstractItemModel *model)
{
    _model = model;
    setupMapper();
}

/* -------------------------------------------------------------------- */

void CalibrationForm::commitData(QWidget* widget)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::cout << widget << std::endl;

    QComboBox *combobox = qobject_cast<QComboBox *>(widget);
    if (!combobox) return;

    QString currentText = combobox->currentText();

    QStringListModel *slm = qobject_cast<QStringListModel *>(combobox->model());

    QStringList list = slm->stringList();

    // TODO resort the list after insertion?
    if (!list.contains(currentText)) {
        list.prepend(currentText);
        slm->setStringList(list);
        combobox->setCurrentIndex(0);
    }
}

/* -------------------------------------------------------------------- */

QStringListModel* CalibrationForm::setupComboModel(QString sql_column)
{
    std::cout << __PRETTY_FUNCTION__ << " : " << sql_column.toStdString() << std::endl;
    QStringList items;

    // Extract a list of previously used values from the database
    QString sql = QString("SELECT DISTINCT %1 FROM " DB_TABLE).arg(sql_column);
    std::cout << sql.toStdString() << std::endl;

    QSqlQuery query(sql, QSqlDatabase::database() );
    query.exec();
    while (query.next())
        items << query.value(0).toString().trimmed();

//  foreach(QString item, items)
//      std::cout << __PRETTY_FUNCTION__ << " > " << item.toStdString() << std::endl;

    QStringListModel* comboModel =  new QStringListModel(items, this);
    return comboModel;
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setupMapper()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

/* TODO use ManualSubmit to prevent uncommited changes to the model?   (use _mapper->submit(); during a save)
 * QDataWidgetMapper supports two submit policies, \c AutoSubmit and \c{ManualSubmit}.
 * \c AutoSubmit will update the model as soon as the current widget loses focus,
 * \c ManualSubmit will not update the model unless submit() is called. \c ManualSubmit
 * is useful when displaying a dialog that lets the user cancel all modifications.
 * Also, other views that display the model won't update until the user finishes
 * all their modifications and submits.
 */
    _mapper = new QDataWidgetMapper(this);
    _mapper->setModel(_model);
    _mapper->setItemDelegate(new QSqlRelationalDelegate(this));
    connect(_mapper->itemDelegate(), SIGNAL(commitData(QWidget*)), this, SLOT(commitData(QWidget*)));

          _projTxt->setModel( setupComboModel("project_name") );
          _userTxt->setModel( setupComboModel("username") );
    _sensorTypeTxt->setModel( setupComboModel("sensor_type") );
     _serialNumTxt->setModel( setupComboModel("serial_number") );
           _dsmTxt->setModel( setupComboModel("dsm_name") );
       _calTypeTxt->setModel( setupComboModel("cal_type") );
          _addrTxt->setModel( setupComboModel("channel") );
      _gainbplrTxt->setModel( setupComboModel("gainbplr") );
       _commentSel->setModel( setupComboModel("comment") );

    _projTxt      ->setEditText( "" );
    _userTxt      ->setEditText( "" );
    _sensorTypeTxt->setEditText( "" );
    _serialNumTxt ->setEditText( "" );
    _dsmTxt       ->setEditText( "" );
    _calTypeTxt   ->setEditText( "" );
    _addrTxt      ->setEditText( "" );
    _gainbplrTxt  ->setEditText( "" );
    _commentSel   ->setEditText( "" );

    _mapper->addMapping( _projTxt,        clm_project_name,  "currentText");
    _mapper->addMapping( _userTxt,        clm_username,      "currentText");
    _mapper->addMapping( _sensorTypeTxt,  clm_sensor_type,   "currentText");
    _mapper->addMapping( _serialNumTxt,   clm_serial_number, "currentText");
    _mapper->addMapping( _varNameTxt,     clm_var_name,             "text");
    _mapper->addMapping( _dsmTxt,         clm_dsm_name,      "currentText");
    _mapper->addMapping( _calTypeTxt,     clm_cal_type,      "currentText");
    _mapper->addMapping( _addrTxt,        clm_channel,       "currentText");
    _mapper->addMapping( _gainbplrTxt,    clm_gainbplr,      "currentText");
    _mapper->addMapping( _adsFileName,    clm_ads_file_name, "currentText");
    _mapper->addMapping( _temperatureTxt, clm_temperature                 );
    _mapper->addMapping( _commentSel,     clm_comment,       "currentText");
    _mapper->addMapping( _commentTxt,     clm_comment,         "plainText");
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setRow(int row)
{
    _row = row;
    _mapper->setCurrentIndex(row);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setEnabled(bool state)
{
    std::cout << __PRETTY_FUNCTION__ << " state: " << state << std::endl;

    _projTxt->setEnabled(state);
    _userTxt->setEnabled(state);
    _sensorTypeTxt->setEnabled(state);
    _serialNumTxt->setEnabled(state);
    _varNameTxt->setEnabled(state);
    _dsmTxt->setEnabled(state);
    _calTypeTxt->setEnabled(state);
    _gainbplrTxt->setEnabled(state);
    _adsFileName->setEnabled(state);
    _platformTxt->setEnabled(state);
    _addrTxt->setEnabled(state);
//  _tableWidget->setEnabled(true);
    _linearRB->setEnabled(state);
    _2ndOrderRB->setEnabled(state);
    _3rdOrderRB->setEnabled(state);
    _currCalC0->setEnabled(state);
    _prevCalC0->setEnabled(state);
    _currCalC1->setEnabled(state);
    _prevCalC1->setEnabled(state);
    _currCalC2->setEnabled(state);
    _prevCalC2->setEnabled(state);
    _currCalC3->setEnabled(state);
    _prevCalC3->setEnabled(state);
    _temperatureTxt->setEnabled(state);
    _commentSel->setEnabled(state);
    _commentTxt->setEnabled(state);

    if (state)
        connect(_delButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(delThisSetPoint(int)));
    else
        disconnect(_delButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(delThisSetPoint(int)));
}

/* -------------------------------------------------------------------- */

void CalibrationForm::delThisSetPoint( int index )
{
    std::cout << "delThisSetPoint index: " << index << std::endl;

//  _delButtonList[index]->toggle();  // TODO does not show a toggled state?

    // note, this just clears the displayed values.  They are actually
    // removed by MainWindow::delThisSetPoint.
    _setPointList   [index]->clear();
    _delButtonList  [index]->setEnabled(false);
    _newVList       [index]->clear();
    _new_sdList     [index]->clear();
    _appliedList    [index]->clear();
    _setDateTimeList[index]->clear();

    emit delThisSetPoint(_row, index);
    emit replot(_row);
}
                                                                                                                                                                                                     
/* -------------------------------------------------------------------- */

void CalibrationForm::changeFitButtonClicked(int degree)
{
    std::cout << "changeFitButtonClicked degree: " << degree << std::endl;
    emit changeFitButtonClicked(_row, degree);
    emit replot(_row);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::commentSelected( int index )
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    _commentTxt->setText( _commentSel->currentText() );
    emit scrollToLastClicked();
}
