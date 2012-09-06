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

  _delThisGroup = new QButtonGroup(this);
  _delThisGroup->setObjectName(QString::fromUtf8("_delThisGroup"));
  _delThisGroup->setExclusive(false);
  _delThisGroup->addButton(_delButton0, 0);
  _delThisGroup->addButton(_delButton1, 1);
  _delThisGroup->addButton(_delButton2, 2);
  _delThisGroup->addButton(_delButton3, 3);
  _delThisGroup->addButton(_delButton4, 4);
  _delThisGroup->addButton(_delButton5, 5);
  _delThisGroup->addButton(_delButton6, 6);
  _delThisGroup->addButton(_delButton7, 7);
  _delThisGroup->addButton(_delButton8, 8);
  connect(_delThisGroup, SIGNAL(buttonClicked(int)), this, SLOT(delThisSetPoint(int)));

  _curveFitGroup = new QButtonGroup(this);
  _curveFitGroup->setObjectName(QString::fromUtf8("_curveFitGroup"));
  _curveFitGroup->addButton(_linearRB,   2);
  _curveFitGroup->addButton(_2ndOrderRB, 3);
  _curveFitGroup->addButton(_3rdOrderRB, 4);
  connect(_curveFitGroup, SIGNAL(buttonClicked(int)), this, SLOT(changeFitButtonClicked(int)));
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setModel(QSqlTableModel *model)
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
    QString sql = QString("SELECT DISTINCT %1 FROM calibrations").arg(sql_column);
    std::cout << sql.toStdString() << std::endl;

    QSqlQuery query(sql, QSqlDatabase::database() );
    query.exec();
    while (query.next())
        items << query.value(0).toString().trimmed();

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
//  _mapper->addMapping( _commentSel,     clm_comment,       "currentText");
    _mapper->addMapping( _commentTxt,     clm_comment,         "plainText");

    _setPointList.append(_setPoint0);
    _setPointList.append(_setPoint1);
    _setPointList.append(_setPoint2);
    _setPointList.append(_setPoint3);
    _setPointList.append(_setPoint4);
    _setPointList.append(_setPoint5);
    _setPointList.append(_setPoint6);
    _setPointList.append(_setPoint7);
    _setPointList.append(_setPoint8);

    _delButtonList.append(_delButton0);
    _delButtonList.append(_delButton1);
    _delButtonList.append(_delButton2);
    _delButtonList.append(_delButton3);
    _delButtonList.append(_delButton4);
    _delButtonList.append(_delButton5);
    _delButtonList.append(_delButton6);
    _delButtonList.append(_delButton7);
    _delButtonList.append(_delButton8);

    _newVList.append(_newV0);
    _newVList.append(_newV1);
    _newVList.append(_newV2);
    _newVList.append(_newV3);
    _newVList.append(_newV4);
    _newVList.append(_newV5);
    _newVList.append(_newV6);
    _newVList.append(_newV7);
    _newVList.append(_newV8);

    _new_sdList.append(_new_sd0);
    _new_sdList.append(_new_sd1);
    _new_sdList.append(_new_sd2);
    _new_sdList.append(_new_sd3);
    _new_sdList.append(_new_sd4);
    _new_sdList.append(_new_sd5);
    _new_sdList.append(_new_sd6);
    _new_sdList.append(_new_sd7);
    _new_sdList.append(_new_sd8);

    _appliedList.append(_applied0);
    _appliedList.append(_applied1);
    _appliedList.append(_applied2);
    _appliedList.append(_applied3);
    _appliedList.append(_applied4);
    _appliedList.append(_applied5);
    _appliedList.append(_applied6);
    _appliedList.append(_applied7);
    _appliedList.append(_applied8);

    _setDateTimeList.append(_setDateTime0);
    _setDateTimeList.append(_setDateTime1);
    _setDateTimeList.append(_setDateTime2);
    _setDateTimeList.append(_setDateTime3);
    _setDateTimeList.append(_setDateTime4);
    _setDateTimeList.append(_setDateTime5);
    _setDateTimeList.append(_setDateTime6);
    _setDateTimeList.append(_setDateTime7);
    _setDateTimeList.append(_setDateTime8);

    _currCalCList.append(_currCalC0);
    _currCalCList.append(_currCalC1);
    _currCalCList.append(_currCalC2);
    _currCalCList.append(_currCalC3);

    _prevCalCList.append(_prevCalC0);
    _prevCalCList.append(_prevCalC1);
    _prevCalCList.append(_prevCalC2);
    _prevCalCList.append(_prevCalC3);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::setRow(int row)
{
    _row = row;
    _mapper->setCurrentIndex(row);
}

/* -------------------------------------------------------------------- */

void CalibrationForm::delThisSetPoint( int index )
{
    std::cout << "delThisSetPoint index: " << index << std::endl;

    QList<QAbstractButton *> delThisButtons = _delThisGroup->buttons();
//  delThisButtons.at(index)->toggle();  // TODO does not show a toggled state?

    // note, this just clears the displayed values.  They are actually
    // removed by MainWindow::delThisSetPoint.
    _setPointList   .at(index)->clear();
    _newVList       .at(index)->clear();
    _new_sdList     .at(index)->clear();
    _appliedList    .at(index)->clear();
    _setDateTimeList.at(index)->clear();

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
}
