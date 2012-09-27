#ifndef _CalibrationForm_h_
#define _CalibrationForm_h_

#include <QAbstractItemModel>
#include <QStringListModel>
#include <QDataWidgetMapper>

#include "ui_CalibrationForm.h"

/**
 * @class calib::CalibrationForm
 */
class CalibrationForm : public QWidget, public Ui::CalibrationForm
{
  Q_OBJECT

public:
  CalibrationForm(QWidget* parent = 0);

  void setModel(QAbstractItemModel *model);

  QStringListModel* setupComboModel(QString sql_column);

  void setupMapper();

  void setRow(int row);

  QList<QLineEdit*>   _setPointList;
  QList<QPushButton*> _delButtonList;
  QList<QLineEdit*>   _newVList;         // TODO rename to _inputList
  QList<QLineEdit*>   _new_sdList;       // TODO rename to _stdDevList
  QList<QLineEdit*>   _appliedList;
  QList<QLineEdit*>   _setDateTimeList;
  QList<QLineEdit*>   _currCalCList;
  QList<QLineEdit*>   _prevCalCList;

  QButtonGroup* _delThisGroup;
  QButtonGroup* _curveFitGroup;

//protected slots:
public slots:
  void commitData(QWidget* widget);
  void delThisSetPoint(int index);
  void changeFitButtonClicked(int degree);
  void commentSelected(int index);

signals:
  void delThisSetPoint(int row, int index);
  void changeFitButtonClicked(int row, int degree);
  void replot(int row);

private:
  QAbstractItemModel* _model;
  QDataWidgetMapper* _mapper;
  int _row;
};

#endif
