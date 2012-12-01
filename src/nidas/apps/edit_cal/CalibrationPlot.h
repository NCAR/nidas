#ifndef _CalibrationPlot_h_
#define _CalibrationPlot_h_

//#include <QDataWidgetMapper>
//#include <QItemDelegate>
#include <QtGui>

class QColor;
class QwtPlotCurve;

#include <list>

using namespace std;

#include "ui_CalibrationPlot.h"

class CalibrationCurve
{
public:
    QString rid;
    QwtPlotCurve* actual;
    QwtPlotCurve* fitted;
};

/**
 * @class calib::CalibrationPlot
 */
class CalibrationPlot : public QWidget, public Ui::CalibrationPlot
{
    Q_OBJECT

public:
    CalibrationPlot(QWidget* parent = 0);
    void dropColors();
    void setupColors();

    list<QColor> colors;
};

#endif
