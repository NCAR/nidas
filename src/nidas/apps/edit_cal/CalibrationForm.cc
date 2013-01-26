#include "CalibrationForm.h"
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

    _tableWidget->setRowCount(nRows);

    for (int r=0; r<nRows; r++) {
        _setPointList.append(new QLineEdit);
        _delButtonList.append(new QPushButton);
        _newVList.append(new QLineEdit);
        _new_sdList.append(new QLineEdit);
        _appliedList.append(new QLineEdit);
        _setDateTimeList.append(new QLineEdit);

        // TODO make the 'del' button toggle-able
//      _delButtonList[r]->setCheckable(true);
//      _delButtonList[r]->setChecked(false);

        _setPointList[r]->setReadOnly(true);
        _delButtonList[r]->setText("del");
        _newVList[r]->setReadOnly(true);
        _new_sdList[r]->setReadOnly(true);
        _appliedList[r]->setReadOnly(true);
        _setDateTimeList[r]->setReadOnly(true);

        _setPointList[r]->setMinimumWidth(80);
        _delButtonList[r]->setFixedWidth(40);
        _newVList[r]->setMinimumWidth(90);
        _new_sdList[r]->setMinimumWidth(90);
        _appliedList[r]->setMinimumWidth(20);
        _setDateTimeList[r]->setMinimumWidth(50);

        _tableWidget->setCellWidget(r, 0, _setPointList[r] );
        _tableWidget->setCellWidget(r, 1, _delButtonList[r] );
        _tableWidget->setCellWidget(r, 2, _newVList[r] );
        _tableWidget->setCellWidget(r, 3, _new_sdList[r] );
        _tableWidget->setCellWidget(r, 4, _appliedList[r] );
        _tableWidget->setCellWidget(r, 5, _setDateTimeList[r] );
    }
    _tableWidget->resizeColumnsToContents();
    _tableWidget->horizontalHeader()->setStretchLastSection( true ); 
    _tableWidget->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);

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
  for (int r=0; r<nRows; r++)
    _delButtonGroup->addButton(_delButtonList[r], r);

  connect(_delButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(removeSetPoint(int)));

  _curveFitGroup = new QButtonGroup(this);
  _curveFitGroup->setObjectName(QString::fromUtf8("_curveFitGroup"));
  _curveFitGroup->addButton(_linearRB,   1);
  _curveFitGroup->addButton(_2ndOrderRB, 2);
  _curveFitGroup->addButton(_3rdOrderRB, 3);
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

    _revertBtn->setEnabled(true);
    _submitBtn->setEnabled(true);

    // update Combo Box...
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
//  std::cout << __PRETTY_FUNCTION__ << " : " << sql_column.toStdString() << std::endl;
    QStringList items;
    items << "";

    // Extract a list of previously used values from the database
    QString sql = QString("SELECT DISTINCT %1 FROM " DB_TABLE).arg(sql_column);
//  std::cout << sql.toStdString() << std::endl;

    QSqlQuery query(sql, QSqlDatabase::database() );
    query.exec();
    while (query.next())
        items << query.value(0).toString().trimmed();
    query.finish();

    if (sql_column == "cal_type")
        items << "bath" << "analog" << "instrument";

    items = items.toSet().toList();
    items.sort();

//  foreach(QString item, items)
//      std::cout << __PRETTY_FUNCTION__ << " > " << item.toStdString() << std::endl;

    QStringListModel* comboModel =  new QStringListModel(items, this);
    return comboModel;
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setupMapper()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;

    _mapper = new QDataWidgetMapper(this);
    _mapper->setModel(_model);
    _mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    _mapper->setItemDelegate(new QSqlRelationalDelegate(_mapper));
    connect(_mapper->itemDelegate(), SIGNAL(commitData(QWidget*)), this, SLOT(commitData(QWidget*)));

          _projTxt->setModel( setupComboModel("project_name") );
          _userTxt->setModel( setupComboModel("username") );
    _sensorTypeTxt->setModel( setupComboModel("sensor_type") );
     _serialNumTxt->setModel( setupComboModel("serial_number") );
           _dsmTxt->setModel( setupComboModel("dsm_name") );
       _calTypeTxt->setModel( setupComboModel("cal_type") );
          _addrTxt->setModel( setupComboModel("channel") );
      _gainbplrTxt->setModel( setupComboModel("gainbplr") );

    _projTxt      ->setEditText( "" );
    _userTxt      ->setEditText( "" );
    _sensorTypeTxt->setEditText( "" );
    _serialNumTxt ->setEditText( "" );
    _dsmTxt       ->setEditText( "" );
    _calTypeTxt   ->setEditText( "" );
    _addrTxt      ->setEditText( "" );
    _gainbplrTxt  ->setEditText( "" );

    _mapper->addMapping( _projTxt,        clm_project_name,  "currentText");
    _mapper->addMapping( _userTxt,        clm_username,      "currentText");
    _mapper->addMapping( _sensorTypeTxt,  clm_sensor_type,   "currentText");
    _mapper->addMapping( _serialNumTxt,   clm_serial_number, "currentText");
    _mapper->addMapping( _varNameTxt,     clm_var_name,             "text");
    _mapper->addMapping( _dsmTxt,         clm_dsm_name,      "currentText");
    _mapper->addMapping( _calTypeTxt,     clm_cal_type,      "currentText");
    _mapper->addMapping( _addrTxt,        clm_channel,       "currentText");
    _mapper->addMapping( _gainbplrTxt,    clm_gainbplr,      "currentText");
    _mapper->addMapping( _adsFileName,    clm_ads_file_name,        "text");
    _mapper->addMapping( _temperatureTxt, clm_temperature                 );
    _mapper->addMapping( _commentTxt,     clm_comment,         "plainText");
    _mapper->addMapping( _calDateTime,    clm_cal_date,         "dateTime");
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
    _addrTxt->setEnabled(state);
    _gainbplrTxt->setEnabled(state);
    _adsFileName->setEnabled(state);
    _temperatureTxt->setEnabled(state);
    _commentTxt->setEnabled(state);
    _calDateTime->setEnabled(state);

    _platformTxt->setEnabled(state);
    _tableWidget->setEnabled(state);
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

    _revertBtn->setEnabled(false);
    _submitBtn->setEnabled(false);

    for (int r=0; r<nRows; r++)
        _delButtonGroup->button(r)->setEnabled(state);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::removeSetPoint( int index )
{
    std::cout << "removeSetPoint index: " << index << std::endl;

//  _deleted[index] != _deleted[index]; // TODO make button toggle-able

    // note, this just clears the displayed values.  They are actually
    // removed by MainWindow::removeSetPoint.
    _setPointList   [index]->clear();
    _delButtonList  [index]->setEnabled(false);
    _newVList       [index]->clear();
    _new_sdList     [index]->clear();
    _appliedList    [index]->clear();
    _setDateTimeList[index]->clear();

    emit removeSetPoint(_row, index);
    emit replot(_row);

    _revertBtn->setEnabled(true);
    _submitBtn->setEnabled(true);
}
                                                                                                                                                                                                     
/* -------------------------------------------------------------------- */

void CalibrationForm::changeFitButtonClicked(int order)
{
    std::cout << "changeFitButtonClicked order: " << order << std::endl;
    emit changeFitButtonClicked(_row, order);
    emit replot(_row);

    _revertBtn->setEnabled(true);
    _submitBtn->setEnabled(true);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::revert()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    emit initializeForm(_row);
    _mapper->revert();

    _revertBtn->setEnabled(false);
    _submitBtn->setEnabled(false);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::submit()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    emit submitForm(_row);
    _mapper->submit();

    _revertBtn->setEnabled(false);
    _submitBtn->setEnabled(false);
}
